//
// HTTPSRV.H - Portable HTTP server implementation
//
// EBS - HTTP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __HTTPSSRV_H__
#define __HTTPSSRV_H__

#include "httpp.h"
#include "rtpssl.h"


#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* secure API functions */
#define HTTPS_InitServer HTTP_InitServer
#define HTTPS_FreeServer HTTP_FreeServer

int  HTTPS_ServerProcessOneRequest   (RTP_SSL_CONTEXT sslContext,
                                      HTTPServerContext *server,
                                      HTTP_INT32 timeoutMsec);

int  HTTPS_ServerConnectionClose     (HTTPServerRequestContext *ctx,
                                      HTTPSession *session);

int  HTTPS_ServerStartDaemon         (RTP_SSL_CONTEXT sslContext,
                                      HTTPServerContext *server,
                                      HTTP_INT16 maxConnections, 
                                      HTTP_INT16 maxHelperThreads);

void HTTPS_ServerStopDaemon          (HTTPServerContext *server,
                                      HTTP_INT32 secTimeout);
/* ------------------------------------------------------------------ */

#ifdef __cplusplus
}
#endif

#endif /* __HTTPSSRV_H__ */
