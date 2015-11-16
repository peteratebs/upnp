//
// HTTPSCLI.H - HTTP Secure Client
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __HTTPSCLI_H__
#define __HTTPSCLI_H__

typedef struct s_HTTPSClientSession HTTPSClientSession;

#include "httpp.h"
#include "httpcli.h"
#include "rtpssl.h"

struct s_HTTPSClientSession
{
	HTTPClientSession session;
	RTP_HANDLE        sslSession;
	unsigned          blocking;

	int (*validateCert) (
			void* ctx,
			HTTP_UINT8* certData,
			HTTP_INT32 certLen
		);

	void*             validateData;
};

#ifdef __cplusplus
extern "C" {
#endif

int  HTTPS_ClientSessionInit           (HTTPClientContext* clientContext,
                                        HTTPSClientSession* clientSession,
                                        int (*validateCert) (void* ctx, HTTP_UINT8* certData, HTTP_INT32 certLen)
                                        /* return 0 = reject, 1 = accept once, 2 = store and accept from now on */,
                                        void* ctx);

int  HTTPS_ClientSessionOpenHost       (HTTPSClientSession* clientSession,
                                        const HTTP_CHAR* host,
                                        HTTP_UINT16 port,
                                        unsigned blocking,
                                        HTTP_INT32 timeoutMsec);

int  HTTPS_ClientSessionOpenAddr       (HTTPSClientSession* clientSession,
                                        RTP_NET_ADDR* addr,
                                        unsigned blocking,
                                        HTTP_INT32 timeoutMsec);

void HTTPS_ClientSessionClose          (HTTPSClientSession* clientSession);

void HTTPS_ClientSessionDestroy        (HTTPSClientSession* clientSession);

#ifdef __cplusplus
}
#endif

#endif
