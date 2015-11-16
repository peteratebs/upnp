//
// GENACLI.H -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:07 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __GENACLI_H__
#define __GENACLI_H__

typedef struct s_GENAClientContext       GENAClientContext;
typedef struct s_GENAClientEvent         GENAClientEvent;
typedef struct s_GENAClientRequest       GENAClientRequest;
typedef struct s_GENAClientResponse      GENAClientResponse;
typedef struct s_GENASid                 GENASid;

#include "genatypes.h"
#include "httpsrv.h"
#include "httpmcli.h"
#include "rtpxml.h"
#include "urlparse.h"

#define GENA_SID_STRING_SIZE            100

typedef GENA_INT32 (*GENAClientCallback) (
		GENAClientContext* ctx,
		GENAClientEvent* event,
		void* userData
	);

typedef GENA_INT32 (*GENAClientRequestCallback) (
		void* userData,
		GENAClientResponse* response
	);

enum e_GENAEventResponseStatus
{
	GENA_EVENT_NO_ERROR = 0,
	GENA_EVENT_ERROR_INTERNAL,
	GENA_EVENT_ERROR_INVALID_SID,
	GENA_EVENT_ERROR_MISSING_SID,
	GENA_EVENT_ERROR_MISSING_NT_NTS,
	GENA_EVENT_ERROR_INVALID_NT,
	GENA_EVENT_ERROR_INVALID_NTS,
	GENA_EVENT_ERROR_BAD_REQUEST,
	GENA_EVENT_NUM_STATES
};
typedef enum   e_GENAEventResponseStatus GENAEventResponseStatus;

enum e_GENAClientRequestState
{
	GENA_REQUEST_CONNECTING = 0,
	GENA_REQUEST_READING,
	GENA_REQUEST_ACCEPTED,
	GENA_REQUEST_REJECTED,
	GENA_REQUEST_ERROR,
	GENA_NUM_REQUEST_STATES
};
typedef enum   e_GENAClientRequestState  GENAClientRequestState;

struct s_GENAClientContext
{
	GENAClientCallback    userCallback;
	void*                 userData;
};

struct s_GENAClientEvent
{
	RTP_NET_ADDR              remoteAddr;
	GENA_CHAR                 sid[GENA_SID_STRING_SIZE];
	GENA_INT32                seq;
	GENA_INT32                contentLength;
	GENA_BOOL                 ntPresent;
	GENA_BOOL                 ntsPresent;
	GENA_BOOL                 sidPresent;
	GENAEventResponseStatus   status;
	RTPXML_Document*            propertySetDoc;
};

struct s_GENAClientResponse
{
	GENA_INT32                httpStatus;
	GENA_CHAR                 sid[GENA_SID_STRING_SIZE];
	GENA_INT32                timeoutSec;
};

struct s_GENAClientRequest
{
	GENAClientRequestState    state;
	GENARequestType           requestType;
	HTTPManagedClientSession* httpSession;
	URLDescriptor             eventSubURL;
	GENAClientRequestCallback callbackFn;
	void*                     callbackData;

	union
	{
		struct
		{
			GENA_CHAR*        callbackUrl;
			GENA_INT32        timeoutSec;
		}
		subscribe;

		struct
		{
			GENA_CHAR*        sid;
			GENA_INT32        timeoutSec;
		}
		renew;

		struct
		{
			GENA_CHAR*        sid;
		}
		cancel;
	}
	param;
};

#ifdef __cplusplus
extern "C" {
#endif

GENA_INT32 GENA_ClientInit (
		GENAClientContext* ctx,
		GENAClientCallback callback,
		void* cookie
	);

void GENA_ClientDestroy (
		GENAClientContext* ctx
	);

// this function ties into the HTTP server to handle
// event notifications from the remote host
GENA_INT32 GENA_ClientProcessEvent (
		GENAClientContext* ctx,
		HTTPServerRequestContext* srv,
		HTTPSession* session,
		HTTPRequest* request,
		RTP_NET_ADDR* notifierAddr
	);

GENA_INT32 GENA_SubscribeRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_CHAR* callbackUrl,
		GENA_INT32 timeoutSec,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	);

GENA_INT32 GENA_RenewRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_CHAR* sid,
		GENA_INT32 timeoutSec,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	);

GENA_INT32 GENA_UnsubscribeRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_CHAR* sid,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	);

void GENA_ClientRequestDestroy (
		GENAClientRequest* request
	);

GENA_INT32 GENA_ClientRequestExecute (
		GENAClientRequest* request
	);

GENA_INT32 GENA_ClientRequestAddToSelectList (
		GENAClientRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

GENA_BOOL GENA_ClientRequestProcessState (
		GENAClientRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

#ifdef __cplusplus
}
#endif

#endif /* __GENACLI_H__ */
