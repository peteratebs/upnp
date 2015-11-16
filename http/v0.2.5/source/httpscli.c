//
// HTTPSCLI.C -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/09/28 20:09:03 $
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

#include "httpscli.h"
#include "rtpstdup.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTPS_CLIENT_TIMEOUT_MSEC        (45*1000) /* timeout after 45 sec */

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

HTTP_INT32 _HTTPS_ClientRead (
		HTTPSClientSession* session,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	);

HTTP_INT32 _HTTPS_ClientWrite (
		HTTPSClientSession* session,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	);

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTPS_ClientSessionInit (
		HTTPClientContext* clientContext,
		HTTPSClientSession* clientSession,
		int (*validateCert) (void* ctx, HTTP_UINT8* certData, HTTP_INT32 certLen)
		/* return 0 = reject, 1 = accept once, 2 = store and accept from now on */,
		void* ctx
	)
{
	if (clientContext->sslEnabled)
	{
		if (HTTP_ClientSessionInitEx (
				clientContext,
				&clientSession->session,
				_HTTPS_ClientRead,
				_HTTPS_ClientWrite
			) >= 0)
		{
			clientSession->validateCert = validateCert;
			clientSession->validateData = ctx;

			return (0);
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  HTTPS_ClientSessionOpenHost (
		HTTPSClientSession* clientSession,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	)
{
	if (HTTP_ClientSessionOpenHost (
			&clientSession->session,
			host,
			port,
			1,
			RTP_TIMEOUT_INFINITE
		) >= 0)
	{
		if (rtp_ssl_connect (
				&clientSession->sslSession,
				clientSession->session.netSock,
				clientSession->session.clientContext->sslContext
			) >= 0)
		{
			clientSession->blocking = blocking;

			rtp_net_setblocking (clientSession->session.netSock, blocking);

			return (0);
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  HTTPS_ClientSessionOpenAddr (
		HTTPSClientSession* clientSession,
		RTP_NET_ADDR* addr,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	)
{
	if (HTTP_ClientSessionOpenAddr (
			&clientSession->session,
			addr,
			1,
			RTP_TIMEOUT_INFINITE
		) >= 0)
	{
		if (rtp_ssl_connect (
				&clientSession->sslSession,
				clientSession->session.netSock,
				clientSession->session.clientContext->sslContext
			) >= 0)
		{
			clientSession->blocking = blocking;

			rtp_net_setblocking (clientSession->session.netSock, blocking);

			return (0);
		}
	}

	return (-1);}

/*---------------------------------------------------------------------------*/
void HTTPS_ClientSessionClose (
		HTTPSClientSession* clientSession
	)
{
	rtp_ssl_close_stream(clientSession->sslSession);
	HTTP_ClientSessionClose(&clientSession->session);
}

/*---------------------------------------------------------------------------*/
void HTTPS_ClientSessionDestroy (
		HTTPSClientSession* clientSession
	)
{
	rtp_ssl_close_stream(clientSession->sslSession);
	HTTP_ClientSessionDestroy(&clientSession->session);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTPS_ClientRead (
		HTTPSClientSession* clientSession,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	)
{
	HTTP_INT32 totalRead = 0;
	HTTP_INT32 bytesRead;

	if (!buffer)
	{
		if (rtp_net_read_select(clientSession->session.netSock, 0) >= 0)
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
			if (rtp_net_read_select(clientSession->session.netSock, HTTPS_CLIENT_TIMEOUT_MSEC) < 0)
			{
				/* waited for the max timeout; no data, return error */
				return (HTTP_EIOFAILURE);
			}

			bytesRead = rtp_ssl_recv(clientSession->sslSession, buffer + totalRead, max, clientSession->blocking);

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

			min -= bytesRead;
			max -= bytesRead;
			totalRead += bytesRead;
		}
	}
	else
	{
		bytesRead = rtp_ssl_recv(clientSession->sslSession, buffer, max, clientSession->blocking);

		if (bytesRead == -1)
		{
			return (HTTP_EIOFAILURE);
		}
		else if (bytesRead == -2)
		{
			return (HTTP_EWOULDBLOCK);
		}

		totalRead = bytesRead;
	}

	return (totalRead);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTPS_ClientWrite (
		HTTPSClientSession* clientSession,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	)
{
	return (rtp_ssl_send(clientSession->sslSession, buffer, size, clientSession->blocking));
}
