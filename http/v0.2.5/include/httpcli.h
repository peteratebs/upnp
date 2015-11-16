//
// HTTPCLI.H - HTTP Client
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __HTTPCLI_H__
#define __HTTPCLI_H__

#define HTTP_CLIENT_MULTITHREAD

typedef struct s_HTTPClientContext HTTPClientContext;
typedef struct s_HTTPClientSession HTTPClientSession;
typedef struct s_HTTPClientRequest HTTPClientRequest;

#include "httpp.h"
#include "rtpnet.h"

#ifdef HTTP_CLIENT_MULTITHREAD
#include "rtpsignl.h"
#endif

struct s_HTTPClientContext
{
	RTP_HANDLE         sslContext;
	unsigned           sslEnabled;
};

struct s_HTTPClientSession
{
	HTTPSession        session;
	HTTPClientContext* clientContext;
	RTP_SOCKET         netSock;
	unsigned           socketOpen;
	HTTP_CHAR*         host;
	RTP_NET_ADDR       netAddr;
};

#define HTTP_CLIENT_SESSION_TO_SESSION(X) (&((X)->session))

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP Client Context Management */

int  HTTP_ClientInit                   (HTTPClientContext* clientContext,
                                        RTP_HANDLE sslContext,
										unsigned sslEnabled);

void HTTP_ClientDestroy                (HTTPClientContext* clientContext);

/* HTTP Client Session Management */

int  HTTP_ClientSessionInit            (HTTPClientContext* clientContext,
                                        HTTPClientSession* clientSession);

int HTTP_ClientSessionInitEx           (HTTPClientContext* clientContext,
                                        HTTPClientSession* clientSession,
                                        HTTPStreamReadFn readFn,
                                        HTTPStreamWriteFn writeFn);

int  HTTP_ClientSessionOpenHost        (HTTPClientSession* clientSession,
                                        const HTTP_CHAR* host,
                                        HTTP_UINT16 port,
                                        unsigned blocking,
                                        HTTP_INT32 timeoutMsec);

int  HTTP_ClientSessionOpenAddr        (HTTPClientSession* clientSession,
                                        RTP_NET_ADDR* addr,
                                        unsigned blocking,
                                        HTTP_INT32 timeoutMsec);

int  HTTP_ClientSessionSelect          (HTTPClientSession** writeList,
                                        HTTP_INT16* writeNum,
                                        HTTPClientSession** readList,
                                        HTTP_INT16* readNum,
                                        HTTPClientSession** errList,
                                        HTTP_INT16* errNum,
                                        HTTP_INT32 timeoutMsec);

unsigned HTTP_ClientSessionIsAlive     (HTTPClientSession* clientSession);

int  HTTP_ClientSessionReset           (HTTPClientSession* clientSession);

int HTTP_ClientSessionSetBlocking      (HTTPClientSession* clientSession,
                                        unsigned blocking);

int  HTTP_ClientSessionClose           (HTTPClientSession* clientSession);

void HTTP_ClientSessionDestroy         (HTTPClientSession* clientSession);

/* HTTP Client Request Processing */

int  HTTP_ClientRequestInit            (HTTPClientSession* clientSession,
                                        HTTPRequest* request,
                                        const HTTP_CHAR* method,
                                        const HTTP_CHAR* target,
                                        HTTP_INT8 httpMajorVersion,
                                        HTTP_INT8 httpMinorVersion);

#ifdef __cplusplus
}
#endif

#endif
