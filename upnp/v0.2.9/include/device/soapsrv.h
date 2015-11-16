//
// SOAP.H -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __SOAP_H__
#define __SOAP_H__

#include "rtpxml.h"
#include "httpp.h"
#include "httpsrv.h"
#include "rtpnet.h"
#include "soaptypes.h"

typedef struct s_SOAPServerContext SOAPServerContext;
typedef struct s_SOAPRequest SOAPRequest;

/* zero return means request successfully handled;
   negative return means send fault response */
typedef int (*SOAPServerCallback) (
		struct s_SOAPServerContext *ctx,
		struct s_SOAPRequest *request,
		void *cookie);

struct s_SOAPServerContext
{
	SOAPServerCallback  userCallback;
	void               *userCookie;
};

struct s_SOAPRequest
{
	struct
	{
		const SOAP_CHAR *target;
		const SOAP_CHAR *soapAction;
		SOAP_INT32       messageLen;
		RTP_NET_ADDR     clientAddr;
		SOAPEnvelope     envelope;
	}
	in;

	union
	{
		struct
		{
			SOAP_CHAR      code[SOAP_ERR_STR_LEN];
			SOAP_CHAR      string[SOAP_ERR_STR_LEN];
			RTPXML_Document *detail;
		}
		fault;

		RTPXML_Document *body;
	}
	out;
};


#ifdef __cplusplus
extern "C" {
#endif

int  SOAP_ServerInit           (SOAPServerContext *ctx,
                                SOAPServerCallback callback,
                                void *cookie);

void SOAP_ServerShutdown       (SOAPServerContext *ctx);

int  SOAP_ServerProcessRequest (SOAPServerContext *ctx,
                                HTTPServerRequestContext *srv,
                                HTTPSession *session,
                                HTTPRequest *request,
                                RTP_NET_ADDR *clientAddr);

#ifdef __cplusplus
}
#endif

#endif /*__SOAP_H__*/
