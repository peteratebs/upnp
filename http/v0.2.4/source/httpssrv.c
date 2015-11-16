//
// HTTPSRV.C - HTTP Server
//
// EBS - 
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "httpsrv.h"
#include "httpssrv.h"
#include "rtpthrd.h"
#include "rtpsignl.h"
#include "rtpssl.h"
#include "filetype.h"
#include "fileext.h"
#include "assert.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTPS_SERVER_ENTER_CRITICAL(S)      rtp_sig_mutex_claim((S)->lock)
#define HTTPS_SERVER_EXIT_CRITICAL(S)       rtp_sig_mutex_release((S)->lock)
#define HTTPS_ASSERT(X)                     assert(X)

#define HTTPS_MASTER_PRIORITY               0 /* normal */
#define HTTPS_HELPER_PRIORITY               0 /* normal */
#define HTTPS_SINGLE_PRIORITY               0 /* normal */
#define HTTPS_MASTER_STACK_SIZE            -1 /* small */
#define HTTPS_HELPER_STACK_SIZE             1 /* large */
#define HTTPS_SINGLE_STACK_SIZE             1 /* large */

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_HTTPSServerContext
{
	HTTPServerContext*        server;
	RTP_SSL_CONTEXT           sslContext;
}
HTTPSServerContext;

typedef struct s_RtpSslSockStruct
{
	RTP_SOCKET                sockHandle;
	int                       serverKeepAlive; /* 1 = yes, 0 = no */
	int                       keepAlive;       /* -1 = no, 1 = yes, 0 = no preference */
	
	RTP_SSL_SESSION           sslSession;
	unsigned                  blocking;
}
RtpSslSockStruct;

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

static void     _MasterSecureEntryPoint               (void *userData);
static void     _HelperSecureEntryPoint               (void *userData);
static void     _SingleSecureEntryPoint               (void *userData);

void HTTPS_ServerMasterThread                         (HTTPSServerContext *secureServer);

void HTTPS_ServerHelperThread                         (HTTPSServerContext *secureServer, 
                                                       HTTP_INT32 maxIdleTimeSec);

void HTTPS_ServerSingleThread                         (HTTPSServerContext *secureServer);

void HTTPS_ServerSpawnHelperThread                    (HTTPSServerContext *secureServer);

int  HTTPS_ServerProcessOneConnection                 (RTP_SSL_SESSION sslStream,
                                                       HTTPServerContext *server, 
                                                       RTP_SOCKET sockHandle,
                                                       RTP_NET_ADDR *clientAddr);

/*****************************************************************************/
// 
/*****************************************************************************/

HTTP_INT32 RtpSslSockReadFn (
		void *streamPtr, 
		HTTP_UINT8 *buffer, 
		HTTP_INT32 min, 
		HTTP_INT32 max
	);

HTTP_INT32 RtpSslSockWriteFn (
		void *streamPtr, 
		const HTTP_UINT8 *buffer, 
		HTTP_INT32 size
	);


/*****************************************************************************/
// Server functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTPS_ServerProcessOneRequest (RTP_SSL_CONTEXT sslContext,
                                    HTTPServerContext *server,
                                    HTTP_INT32 timeoutMsec)
{
RTP_SSL_SESSION sslStream;
RTP_SOCKET sockHandle;
RTP_NET_ADDR clientAddr;
int result;

	if (rtp_net_read_select(server->serverSocket, timeoutMsec) < 0)
	{
		return (-1);
	}

	if (rtp_net_accept(&sockHandle, server->serverSocket, clientAddr.ipAddr, &clientAddr.port, &clientAddr.type) < 0)
	{
		return (-1);
	}
	
	if (rtp_net_read_select(sockHandle, timeoutMsec) < 0)
	{
		rtp_net_closesocket(sockHandle);
		return (-1);
	}
	
	if (rtp_ssl_accept(&sslStream, sockHandle, sslContext) < 0)
	{
		rtp_net_closesocket(sockHandle);
		return (-1);
	}
	
	result = HTTPS_ServerProcessOneConnection(sslStream, server, sockHandle, &clientAddr);
	return (result);
}

/*---------------------------------------------------------------------------*/
int  HTTPS_ServerConnectionClose   (HTTPServerRequestContext *ctx,
                                    HTTPSession *session)
{
	if (!ctx->connectionClosed)
	{
		rtp_ssl_close_stream(((RtpSslSockStruct*)session->streamPtr)->sslSession);
		return (HTTP_ServerConnectionClose(ctx, session));
	}
	return (0);
}

/*---------------------------------------------------------------------------*/
int  HTTPS_ServerStartDaemon       (RTP_SSL_CONTEXT sslContext,
                                    HTTPServerContext *server,
                                    HTTP_INT16 maxConnections, 
                                    HTTP_INT16 maxHelperThreads)
{
HTTP_BOOL startTask = 0;

	HTTPS_SERVER_ENTER_CRITICAL(server);

	if (server->status == HTTP_SERVER_INITIALIZED)
	{
		server->status = HTTP_SERVER_DAEMON_RUNNING;
		server->maxConnections = maxConnections;
		server->maxHelperThreads = maxHelperThreads;
		startTask = 1;
	}

	HTTPS_SERVER_EXIT_CRITICAL(server);

	if (startTask)
	{
		HTTPSServerContext *secureServer;
		RTP_THREAD threadHandle;
		
		secureServer = (HTTPSServerContext*) HTTP_MALLOC(sizeof(HTTPSServerContext));
		secureServer->sslContext = sslContext;
		secureServer->server     = server;
		
		if (maxHelperThreads > 0)
		{
			if (rtp_thread_spawn(&threadHandle, _MasterSecureEntryPoint, 0, 
			                     HTTPS_MASTER_STACK_SIZE, HTTPS_MASTER_PRIORITY, secureServer) < 0)
			{
				HTTPS_SERVER_ENTER_CRITICAL(server);
				server->status = HTTP_SERVER_INITIALIZED;
				HTTPS_SERVER_EXIT_CRITICAL(server);
				HTTP_FREE(secureServer);
				return (-1);
			}
		}
		else
		{
			if (rtp_thread_spawn(&threadHandle, _SingleSecureEntryPoint, 0, 
			                     HTTPS_SINGLE_STACK_SIZE, HTTPS_SINGLE_PRIORITY, secureServer) < 0)
			{
				HTTPS_SERVER_ENTER_CRITICAL(server);
				server->status = HTTP_SERVER_INITIALIZED;
				HTTPS_SERVER_EXIT_CRITICAL(server);
				HTTP_FREE(secureServer);
				return (-1);
			}
		}
		
		HTTPS_SERVER_ENTER_CRITICAL(server);
		server->masterThreadActive = 1;
		HTTPS_SERVER_EXIT_CRITICAL(server);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void HTTPS_ServerStopDaemon  (HTTPServerContext *server,
                              HTTP_INT32 secTimeout)
{		
	HTTP_ServerStopDaemon(server, secTimeout);
}


/*****************************************************************************/
// Helper functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTPS_ServerProcessOneConnection (RTP_SSL_SESSION sslStream,
                                       HTTPServerContext *server, 
                                       RTP_SOCKET sockHandle,
                                       RTP_NET_ADDR *clientAddr)
{
RtpSslSockStruct s;
HTTPRequest request;
HTTPSession session;
int result;
HTTP_UINT8 tempBuffer[1024];
HTTP_INT32 tempBufferSize = 1024;
int requestProcessed = 0;
HTTPServerPath *serverPath;
HTTP_INT32 len;
HTTPServerRequestContext ctx;

	s.sockHandle = sockHandle;
	
	_HTTP_ServerInitRequestContext(server, &ctx);

	ctx.socketHandle = sockHandle;

	/* validate the client by network address */
	if (_HTTP_ServerValidateClient(server, clientAddr) < 0)
	{
		_HTTP_ServerSendError (&ctx, &session, 403, "Forbidden");
		return (-1);
	}

	s.serverKeepAlive = 0;
	s.keepAlive = 0;
	s.blocking = 1;
	s.sslSession = sslStream;

	/* create a secure HTTP session */
	result = HTTP_InitSession (&session, 
							   RtpSslSockReadFn,	
							   RtpSslSockWriteFn,
							   &s);

	HTTP_SetWriteBufferSize(&session, 1452);
	
	if (result < 0)
	{
		/* Server error */
		_HTTP_ServerSendError (&ctx, &session, 500, "Internal Server Error");
		HTTPS_ServerConnectionClose(&ctx, &session);
		HTTP_FreeSession(&session);
		return (-1);
	}
	
	/* Read the request */
	len = HTTP_ReadRequest(&session, &request, tempBuffer, tempBufferSize);
						   
	if (len < 0)
	{
		/* Bad request */
		_HTTP_ServerSendError (&ctx, &session, 400, "Bad Request");
		HTTPS_ServerConnectionClose(&ctx, &session);
		HTTP_FreeSession(&session);
		return (-1);
	}

	serverPath = _HTTP_ServerFindMatchingPath(server, request.target);

	if (serverPath)
	{
		switch (serverPath->fn(serverPath->userData, &ctx, &session, &request, clientAddr))
		{
			case HTTP_REQUEST_STATUS_DONE:
				requestProcessed = 1;
				break;
				
			case HTTP_REQUEST_STATUS_CONTINUE:
				break;
		}
	}

	if (!requestProcessed)
	{
		_HTTP_ServerHandleRequest(&ctx, &session, &request, clientAddr);
	}
	
	if (!ctx.connectionClosed)
	{
		/* tbd - add support for connection keep-alive */
		HTTPS_ServerConnectionClose(&ctx, &session);
	}
	
	HTTP_FreeSession(&session);
	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 RtpSslSockReadFn  (void *streamPtr, HTTP_UINT8 *buffer, HTTP_INT32 min, HTTP_INT32 max)
{
	RtpSslSockStruct *s = (RtpSslSockStruct *) streamPtr;
	return (rtp_ssl_recv(s->sslSession, buffer, max, s->blocking));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 RtpSslSockWriteFn (void *streamPtr, const HTTP_UINT8 *buffer, HTTP_INT32 size)
{
	RtpSslSockStruct *s = (RtpSslSockStruct *) streamPtr;
	return (rtp_ssl_send(s->sslSession, buffer, size, s->blocking));
}

/*---------------------------------------------------------------------------*/
void HTTPS_ServerMasterThread (HTTPSServerContext *secureServer)
{
RTP_SOCKET sockHandle;
RTP_NET_ADDR clientAddr;
HTTPServerContext *server = secureServer->server;
HTTP_INT32 timeoutMsec = 1000;

	/* loop: accept 1 connection, spawn one thread up to max, enqueue connection */

	HTTPS_SERVER_ENTER_CRITICAL(server);
	
	while (server->status == HTTP_SERVER_DAEMON_RUNNING)
	{
		HTTPS_SERVER_EXIT_CRITICAL(server);
	
		/* accept 1 connection, spawn one thread up to max, enqueue connection */
	
		if (rtp_net_read_select(server->serverSocket, timeoutMsec) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);
			continue;
		}
	
		if (rtp_net_accept(&sockHandle, server->serverSocket, clientAddr.ipAddr, &clientAddr.port, &clientAddr.type) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);
			continue;
		}
		
		HTTP_ServerQueueRequest(server, sockHandle, &clientAddr);
		
		HTTPS_SERVER_ENTER_CRITICAL(server);
		
		HTTPS_ASSERT(server->numHelperThreadsAvailable >= 0);

		if (server->numHelperThreadsAvailable == 0)
		{
			HTTPS_SERVER_EXIT_CRITICAL(server);
			HTTPS_ServerSpawnHelperThread(secureServer);
			HTTPS_SERVER_ENTER_CRITICAL(server);
		}
	}

	server->masterThreadActive = 0;

	HTTPS_SERVER_EXIT_CRITICAL(server);

}

/*---------------------------------------------------------------------------*/
void HTTPS_ServerHelperThread  (HTTPSServerContext *secureServer, 
                                HTTP_INT32 maxIdleTimeSec)
{
RTP_SOCKET sockHandle;
RTP_NET_ADDR clientAddr;
RTP_SSL_SESSION sslStream;
HTTPServerContext *server = secureServer->server;
HTTP_INT32 idleSec = 0;	
	
	/* loop: wait for an accepted connection, process it */

	HTTPS_SERVER_ENTER_CRITICAL(server);		

	while (server->status == HTTP_SERVER_DAEMON_RUNNING && idleSec < maxIdleTimeSec)
	{		
		HTTPS_SERVER_EXIT_CRITICAL(server);

		if (HTTP_ServerWaitRequest(server, &sockHandle, &clientAddr, 1000) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);
			idleSec++;
			continue;
		}
		
		HTTPS_SERVER_ENTER_CRITICAL(server);		
		server->numHelperThreadsAvailable--;
		HTTPS_SERVER_EXIT_CRITICAL(server);
		
		if (rtp_net_read_select(sockHandle, ((maxIdleTimeSec - idleSec)*1000)) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);
			server->numHelperThreadsAvailable++;
			break;
		}
		
		if (rtp_ssl_accept(&sslStream, sockHandle, secureServer->sslContext) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);
			server->numHelperThreadsAvailable++;
			break;
		}
		
		idleSec = 0;
		
		if (HTTPS_ServerProcessOneConnection(sslStream, server, sockHandle, &clientAddr) < 0)
		{
			HTTPS_SERVER_ENTER_CRITICAL(server);		
			server->numHelperThreadsAvailable++;
			break;
		}
		
		HTTPS_SERVER_ENTER_CRITICAL(server);		

		server->numHelperThreadsAvailable++;
	}

	server->numHelperThreadsAvailable--;
	server->numHelperThreadsActive--;

	HTTPS_ASSERT(server->numHelperThreadsAvailable >= 0);
	HTTPS_ASSERT(server->numHelperThreadsActive >= 0);
	
	HTTPS_SERVER_EXIT_CRITICAL(server);
}

/*---------------------------------------------------------------------------*/
void HTTPS_ServerSingleThread  (HTTPSServerContext *secureServer)
{
HTTPServerContext *server = secureServer->server;

	/* loop: accept 1 connection, process it */
	
	HTTPS_SERVER_ENTER_CRITICAL(server);
	
	while (server->status == HTTP_SERVER_DAEMON_RUNNING)
	{
		HTTPS_SERVER_EXIT_CRITICAL(server);
		
		HTTPS_ServerProcessOneRequest(secureServer->sslContext, server, 1000);
		
		HTTPS_SERVER_ENTER_CRITICAL(server);
	}

	server->masterThreadActive = 0;

	HTTPS_SERVER_EXIT_CRITICAL(server);
}

/*---------------------------------------------------------------------------*/
void _MasterSecureEntryPoint (void *userData)
{
	HTTPS_ServerMasterThread((HTTPSServerContext *) userData);
	HTTP_FREE(userData);
}

/*---------------------------------------------------------------------------*/
void _HelperSecureEntryPoint (void *userData)
{
	HTTPS_ServerHelperThread((HTTPSServerContext *) userData, 100);
}

/*---------------------------------------------------------------------------*/
void _SingleSecureEntryPoint (void *userData)
{
	HTTPS_ServerSingleThread((HTTPSServerContext*) userData);
	HTTP_FREE(userData);
}

/*---------------------------------------------------------------------------*/
void HTTPS_ServerSpawnHelperThread (HTTPSServerContext *secureServer)
{
RTP_THREAD threadHandle;
HTTP_BOOL spawnThread = 0;
HTTPServerContext* server = secureServer->server;

	HTTPS_SERVER_ENTER_CRITICAL(server);

	if (server->numHelperThreadsActive < server->maxHelperThreads)
	{
		server->numHelperThreadsActive++;
		server->numHelperThreadsAvailable++;
		spawnThread = 1;
	}

	HTTPS_SERVER_EXIT_CRITICAL(server);

	if (spawnThread)
	{
		rtp_thread_spawn(&threadHandle, _HelperSecureEntryPoint, 0, 
		                 HTTPS_HELPER_STACK_SIZE, HTTPS_HELPER_PRIORITY, secureServer);
	}
}


