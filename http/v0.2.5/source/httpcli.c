//
// HTTPCLI.C -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/09/28 20:09:02 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "httpcli.h"
#include "rtpstdup.h"
#include "rtpstr.h"
#include "rtplog.h"
#include "rtpscnv.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTP_CLIENT_TIMEOUT_MSEC        (45*1000) /* timeout after 45 sec */
#define HTTP_CLIENT_DEFAULT_PORT        80

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

HTTP_INT32 _HTTP_ClientRead (
		void* streamPtr,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	);

HTTP_INT32 _HTTP_ClientWrite (
		void* streamPtr,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	);

int _HTTP_ClientSessionListToFdset (
		RTP_FD_SET* fdSet,
		HTTPClientSession** sList,
		HTTP_INT16 sNum
	);

void _HTTP_FdsetToClientSessionList (
		HTTPClientSession** sList,
		HTTP_INT16* sNum,
		RTP_FD_SET* fdSet
	);

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int HTTP_ClientInit (
		HTTPClientContext* clientContext,
		RTP_HANDLE sslContext,
		unsigned sslEnabled
	)
{
	clientContext->sslContext = sslContext;
	clientContext->sslEnabled = sslEnabled;

	return (0);
}

/*---------------------------------------------------------------------------*/
void HTTP_ClientDestroy (
		HTTPClientContext* clientContext
	)
{
}

/*---------------------------------------------------------------------------*/
/* HTTP Client Session Management                                            */

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionInit (
		HTTPClientContext* clientContext,
		HTTPClientSession* clientSession
	)
{
	return (HTTP_ClientSessionInitEx (
			clientContext,
			clientSession,
			_HTTP_ClientRead,
			_HTTP_ClientWrite
		));
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionInitEx (
		HTTPClientContext* clientContext,
		HTTPClientSession* clientSession,
		HTTPStreamReadFn readFn,
		HTTPStreamWriteFn writeFn
	)
{
	rtp_memset(clientSession, 0, sizeof(HTTPClientSession));

	if (HTTP_InitSession(&clientSession->session,
	                     readFn,
	                     writeFn,
	                     clientSession) >= 0)
	{
		clientSession->clientContext = clientContext;
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionOpenHost (
		HTTPClientSession* clientSession,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	)
{
	RTP_NET_ADDR netAddr;
	HTTP_INT16 result;

	netAddr.port = port;

	result = rtp_net_str_to_ip (netAddr.ipAddr, (char*) host, &netAddr.type);

	if(result < 0)
	{
		result = rtp_net_gethostbyname (netAddr.ipAddr,
							            &netAddr.type,
							            (char*) host);
	}

	if (result >= 0)
	{
		if (clientSession->host)
		{
			rtp_strfree(clientSession->host);
		}

		clientSession->host = rtp_strdup(host);

		if (clientSession->host)
		{
			return (HTTP_ClientSessionOpenAddr (clientSession,
												&netAddr,
												blocking,
												timeoutMsec) );
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionOpenAddr (
		HTTPClientSession* clientSession,
		RTP_NET_ADDR* addr,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	)
{
	clientSession->netAddr = *addr;

	if (rtp_net_socket_stream_dual (&clientSession->netSock, addr->type) >= 0)
	{
		/* attempt to put the socket into non-blocking mode */
		rtp_net_setblocking (clientSession->netSock, blocking);

		clientSession->socketOpen = 1;

		/* bind to any IP address (of the same type as the server) and port */
		if (rtp_net_bind_dual (clientSession->netSock,
                          RTP_NET_STREAM,
		                  0 /* addr = ANY */,
		                  0 /* port = ANY*/,
		                  clientSession->netAddr.type) >= 0)
		{
			int connectResult;

			connectResult = rtp_net_connect_dual (clientSession->netSock,
			                                 RTP_NET_STREAM,
			                                 clientSession->netAddr.ipAddr,
			                                 clientSession->netAddr.port,
			                                 clientSession->netAddr.type);

			if (connectResult == -2 /* EWOULDBLOCK */ ||
			    connectResult == 0)
			{
				return (0);
			}
		}

		clientSession->socketOpen = 0;

		rtp_net_closesocket(clientSession->netSock);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionSetBlocking (
		HTTPClientSession* clientSession,
		unsigned blocking
	)
{
	return (rtp_net_setblocking(clientSession->netSock, blocking));
}

/*---------------------------------------------------------------------------*/
unsigned HTTP_ClientSessionIsAlive (
		HTTPClientSession* clientSession
	)
{
	return (rtp_net_is_connected(clientSession->netSock));
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionSelect (
		HTTPClientSession** writeList,
		HTTP_INT16* writeNum,
		HTTPClientSession** readList,
		HTTP_INT16* readNum,
		HTTPClientSession** errList,
		HTTP_INT16* errNum,
		HTTP_INT32 timeoutMsec
	)
{
	RTP_FD_SET writeSet;
	RTP_FD_SET readSet;
	RTP_FD_SET errSet;
	int result = 0;
	int total = 0;

	total += _HTTP_ClientSessionListToFdset (&writeSet, writeList, *writeNum);
	total += _HTTP_ClientSessionListToFdset (&readSet, readList, *readNum);
	total += _HTTP_ClientSessionListToFdset (&errSet, errList, *errNum);

	if (total > 0)
	{
		result = rtp_net_select(&readSet, &writeSet, &errSet, timeoutMsec);

		_HTTP_FdsetToClientSessionList (writeList, writeNum, &writeSet);
		_HTTP_FdsetToClientSessionList (readList, readNum, &readSet);
		_HTTP_FdsetToClientSessionList (errList, errNum, &errSet);
	}

	return (result);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ClientSessionListToFdset (
		RTP_FD_SET* fdSet,
		HTTPClientSession** sList,
		HTTP_INT16 sNum
	)
{
	int count = 0;
	HTTP_INT16 n;

	rtp_fd_zero(fdSet);

	for (n = 0; n < sNum; n++)
	{
		if (sList[n]->session.state != HTTP_STATE_DONE)
		{
			rtp_fd_set(fdSet, sList[n]->netSock);
			count++;
		}
	}

	return (count);
}

/*---------------------------------------------------------------------------*/
void _HTTP_FdsetToClientSessionList (
		HTTPClientSession** sList,
		HTTP_INT16* sNum,
		RTP_FD_SET* fdSet
	)
{
	HTTP_INT16 n;

	for (n = 0; n < *sNum;)
	{
		if (sList[n]->session.state != HTTP_STATE_DONE &&
		    !rtp_fd_isset(fdSet, sList[n]->netSock))
		{
			(*sNum)--;
			if (*sNum > 0 && n < *sNum)
			{
				sList[n] = sList[*sNum];
			}
		}
		else
		{
			n++;
		}
	}
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionReset (
		HTTPClientSession* clientSession
	)
{
	return (HTTP_ResetSession(
			HTTP_CLIENT_SESSION_TO_SESSION(clientSession),
			_HTTP_ClientRead,
			_HTTP_ClientWrite,
			clientSession
		));
}

/*---------------------------------------------------------------------------*/
int HTTP_ClientSessionClose (
		HTTPClientSession* clientSession
	)
{
	if (clientSession->socketOpen)
	{
		clientSession->socketOpen = 0;
		return (rtp_net_closesocket(clientSession->netSock));
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
void HTTP_ClientSessionDestroy (
		HTTPClientSession* clientSession
	)
{
	HTTP_ClientSessionClose(clientSession);

	HTTP_FreeSession(&clientSession->session);

	if (clientSession->host)
	{
		rtp_strfree(clientSession->host);
	}
}

/*---------------------------------------------------------------------------*/
/* HTTP Client Request Processing                                            */

/*---------------------------------------------------------------------------*/
int HTTP_ClientRequestInit (
		HTTPClientSession* clientSession,
		HTTPRequest* request,
		const HTTP_CHAR* method,
		const HTTP_CHAR* target,
		HTTP_INT8 httpMajorVersion,
		HTTP_INT8 httpMinorVersion
	)
{
	if (HTTP_InitRequest (
			&clientSession->session,
			request,
			method,
			target,
			httpMajorVersion,
			httpMinorVersion
		) >= 0)
	{
		if (clientSession->netAddr.port != HTTP_CLIENT_DEFAULT_PORT)
		{
			HTTP_CHAR hostStr[256];
			HTTP_INT32 len;

			rtp_strncpy(hostStr, clientSession->host, 255);
			hostStr[255] = 0;

			len = rtp_strlen(hostStr);
			if (len < 250)
			{
				hostStr[len] = ':';
				len++;
				rtp_itoa(clientSession->netAddr.port, hostStr + len, 10);

				if (HTTP_SetRequestHeaderStr (
						request,
						HTTP_HeaderTypeToString(HTTP_HEADER_HOST),
						hostStr
					) >= 0)
				{
					return (0);
				}
			}
		}
		else
		{
			if (HTTP_SetRequestHeaderStr (
					request,
					HTTP_HeaderTypeToString(HTTP_HEADER_HOST),
					clientSession->host
				) >= 0)
			{
				return (0);
			}
		}

		HTTP_FreeRequest(request);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ClientRead (
		void* streamPtr,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	)
{
	HTTPClientSession* clientSession = (HTTPClientSession*) streamPtr;
	HTTP_INT32 totalRead = 0;
	HTTP_INT32 bytesRead;

	if (!buffer)
	{
		if (rtp_net_read_select(clientSession->netSock, 0) >= 0)
		{
			return (1);
		}
		else
		{
			return (HTTP_EWOULDBLOCK);
		}
	}

	/* sanity check for later */
	if (max < min)
	{
		max = min;
	}

	if (min > 0)
	{
		while (min > 0)
		{
			if (rtp_net_read_select(clientSession->netSock, HTTP_CLIENT_TIMEOUT_MSEC) < 0)
			{
				/* waited for the max timeout; no data, return error */
				return (HTTP_EIOFAILURE);
			}

			bytesRead = rtp_net_recv(clientSession->netSock, buffer + totalRead, max);

			/* if read select returned success, we shouldn't see these cases, but
			   just to be sure, handle them... */
			if (bytesRead == -1)
			{
				return (HTTP_EIOFAILURE);
			}
			else if (bytesRead == -2)
			{
				return (HTTP_EWOULDBLOCK);
			}
			else if (bytesRead == 0)
			{
				break;
			}
			RTP_LOG_WRITE("HTTP_ClientRead READ", (buffer + totalRead), bytesRead)

			min -= bytesRead;
			max -= bytesRead;
			totalRead += bytesRead;
		}
	}
	else
	{
		bytesRead = rtp_net_recv(clientSession->netSock, buffer + totalRead, max);

		if (bytesRead == -1)
		{
			return (HTTP_EIOFAILURE);
		}
		else if (bytesRead == -2)
		{
			return (HTTP_EWOULDBLOCK);
		}

		RTP_LOG_WRITE("HTTP_ClientRead READ", (buffer + totalRead), bytesRead)
		totalRead = bytesRead;
	}

	return (totalRead);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ClientWrite (
		void* streamPtr,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	)
{
	HTTPClientSession* clientSession = (HTTPClientSession*) streamPtr;

	RTP_LOG_WRITE("_HTTP_ClientWrite WRITE", buffer, size)
	return (rtp_net_send(clientSession->netSock, buffer, size));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ClientHostOptionGetLength (
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTPRequest* request,
		void* privateData
	)
{
	HTTP_INT32 len = rtp_strlen(host);
	return (len);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ClientHostOptionWriteValue (
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTPRequest* request,
		void* privateData,
		HTTP_CHAR* value
	)
{
	HTTP_INT32 len = rtp_strlen(host);

	rtp_strncpy(value, host, len);

	return (len);
}
