//
// SOAPCLI.H -
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

#ifndef __SOAPCLI_H__
#define __SOAPCLI_H__

typedef struct s_SOAPAction          SOAPAction;
typedef struct s_SOAPActionResponse  SOAPActionResponse;

#include "soaptypes.h"
#include "httpmcli.h"
#include "urlparse.h"

//#define SOAP_ACTION_MAX_LEN          64
#define SOAP_ACTION_MAX_LEN          96

enum e_SOAPActionState
{
	SOAP_ACTION_CONNECTING_POST = 0,
	SOAP_ACTION_READING_POST_RESPONSE,
	SOAP_ACTION_CONNECTING_M_POST,
	SOAP_ACTION_READING_M_POST_RESPONSE,
	SOAP_ACTION_RESPONSE_READ,
	SOAP_ACTION_ERROR,
	SOAP_NUM_ACTION_STATES
};
typedef enum   e_SOAPActionState     SOAPActionState;

enum e_SOAPResponseStatus
{
	SOAP_RESPONSE_NORMAL = 0,
	SOAP_RESPONSE_FAULT,
	SOAP_REQUEST_ERROR,
	SOAP_NUM_RESPONSES
};
typedef enum   e_SOAPResponseStatus  SOAPResponseStatus;

typedef int (*SOAPActionCallback) (
		void* callbackData,
		SOAPActionResponse* response
	);

struct s_SOAPAction
{
	SOAPActionState           state;
	HTTPManagedClient*        httpClient;
	HTTPManagedClientSession* httpSession;
	URLDescriptor             postTargetURL;
    SOAP_INT16                ipType;
	SOAP_CHAR                 soapAction[SOAP_ACTION_MAX_LEN];
	SOAP_CHAR*                headerStr;
	SOAP_INT32                headerLen;
	SOAP_CHAR*                bodyStr;
	SOAP_INT32                bodyLen;
	SOAPActionCallback        callbackFn;
	void*                     callbackData;
};

struct s_SOAPActionResponse
{
	SOAPResponseStatus   status;
	SOAPEnvelope         envelope;

	struct
	{
		const SOAP_CHAR* code;
		const SOAP_CHAR* string;
		RTPXML_Element*    detailElem;
	}
	fault;
};

#ifdef __cplusplus
extern "C" {
#endif

SOAP_INT32 SOAP_ActionInit (
		SOAPAction* action,
		HTTPManagedClient* httpClient,
		SOAP_INT16 ipType,
		const SOAP_CHAR* destUri,
		const SOAP_CHAR* soapAction,
		SOAP_CHAR* headerStr,
		SOAP_INT32 headerLen,
		SOAP_CHAR* bodyStr,
		SOAP_INT32 bodyLen,
		SOAPActionCallback callbackFn,
		void* callbackData
	);

SOAP_INT32 SOAP_ActionInitEx (
		SOAPAction* action,
		HTTPManagedClient* httpClient,
		SOAP_INT16 ipType,
		URLDescriptor* postURL,
		URLDescriptor* baseURL,
		const SOAP_CHAR* soapAction,
		SOAP_CHAR* headerStr,
		SOAP_INT32 headerLen,
		SOAP_CHAR* bodyStr,
		SOAP_INT32 bodyLen,
		SOAPActionCallback callbackFn,
		void* callbackData
	);

void SOAP_ActionDestroy (
		SOAPAction* action
	);

SOAP_INT32 SOAP_ActionExecute (
		SOAPAction* action
	);

SOAP_INT32 SOAP_ActionAddToSelectList (
		SOAPAction* action,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

SOAP_BOOL SOAP_ActionProcessState (
		SOAPAction* action,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

#ifdef __cplusplus
}
#endif

#endif /* __SOAPCLI_H__ */
