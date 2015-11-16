/*
|  CONTROL_ACTION.C -
|
|  EBS -
|
|   $Author: vmalaiya $
|   $Date: 2006/11/29 18:53:08 $
|   $Name:  $
|   $Revision: 1.1 $
|
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "controlAction.h"
#include "upnpdom.h"
#include "rtpscnv.h"
#include "rtpstdup.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

#define UPNP_CLIENT_ENTER_API(X)   UPNP_RUNTIME_ENTER_CRITICAL((X)->runtime)
#define UPNP_CLIENT_EXIT_API(X)    UPNP_RUNTIME_EXIT_CRITICAL((X)->runtime)

/*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

void _UPnP_ControlInvokeActionCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

int _UPnP_ControlSOAPActionCallback (
		void* callbackData,
		SOAPActionResponse* response
	);

UPnPControlInvokeAction* _UPnP_ControlNewInvokeAction (
		UPnPControlService* service,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData,
		SOAPActionCallback soapCallback
	);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*---------------------------------------------------------------------------*/
UPnPControlInvokeAction* UPnP_ControlNewInvokeAction (
		UPnPControlService* service,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData
	)
{
	return (_UPnP_ControlNewInvokeAction (
			service,
			actionName,
			action,
			userData,
			_UPnP_ControlSOAPActionCallback
		));
}

/*---------------------------------------------------------------------------*/
UPnPControlInvokeAction* _UPnP_ControlNewInvokeAction (
		UPnPControlService* service,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData,
		SOAPActionCallback soapCallback
	)
{
	UPnPControlInvokeAction* invokeSm;
	UPnPControlPoint* cp = service->device->controlPoint;

	if (!service->serviceType)
	{
		return (0);
	}

	invokeSm = (UPnPControlInvokeAction*) rtp_malloc(sizeof(UPnPControlInvokeAction));
	if (invokeSm)
	{
		if (rtp_net_sm_init(&invokeSm->base, _UPnP_ControlInvokeActionCallback) >= 0)
		{
			URLDescriptor controlURL;

			if (URL_Parse(&controlURL, service->controlURL, URL_PROTOCOL_HTTP, cp->runtime->ipType) >= 0)
			{
				UPNP_CHAR soapAction[SOAP_ACTION_MAX_LEN];
				UPNP_INT32 soapActionLen = 0;

				rtp_strncpy(soapAction, service->serviceType, SOAP_ACTION_MAX_LEN-1);
				soapAction[SOAP_ACTION_MAX_LEN-1] = 0;
				soapActionLen = rtp_strlen(soapAction);

				if (soapActionLen < SOAP_ACTION_MAX_LEN - 3)
				{
					soapAction[soapActionLen] = '#';
					soapActionLen++;

					rtp_strncpy (
							soapAction + soapActionLen,
							actionName,
							SOAP_ACTION_MAX_LEN - soapActionLen - 1
						);

					soapAction[SOAP_ACTION_MAX_LEN-1] = 0;

					invokeSm->bodyStr = rtpxmlPrintDocument(action);
					if (invokeSm->bodyStr)
					{
						invokeSm->bodyLen = rtp_strlen(invokeSm->bodyStr);

						if (SOAP_ActionInitEx (
								&invokeSm->soapCtx,
								&cp->httpClient,
								cp->runtime->ipType,
								&controlURL,
								&service->device->baseURL,
								soapAction,
								0 /* headerStr */,
								0 /* headerLen */,
								invokeSm->bodyStr,
								invokeSm->bodyLen,
								soapCallback,
								invokeSm
							) >= 0)
						{
							invokeSm->userData = userData;
							invokeSm->controlPoint = cp;

                            URL_Free(&controlURL);
							return (invokeSm);
						}

						rtpxmlFreeDOMString(invokeSm->bodyStr);
					}
				}

				URL_Free(&controlURL);
			}

			/* tbd - rtp_net_sm_destroy? */
		}

		rtp_free(invokeSm);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlInvokeActionCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlInvokeAction* invokeSm = (UPnPControlInvokeAction*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
		{
			result->maxTimeoutMsec = SOAP_ActionAddToSelectList (
					&invokeSm->soapCtx,
					event->arg.addToSelectList.readList,
					event->arg.addToSelectList.writeList,
					event->arg.addToSelectList.errList
				);
			break;
		}

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			result->done = SOAP_ActionProcessState (
					&invokeSm->soapCtx,
					event->arg.processState.readList,
					event->arg.processState.writeList,
					event->arg.processState.errList
				);
			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			SOAP_ActionDestroy(&invokeSm->soapCtx);
			rtpxmlFreeDOMString(invokeSm->bodyStr);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(invokeSm);
			break;
	}
}

/*---------------------------------------------------------------------------*/
int _UPnP_ControlSOAPActionCallback (
		void* callbackData,
		SOAPActionResponse* response
	)
{
	UPnPControlInvokeAction* invokeSm = (UPnPControlInvokeAction*) callbackData;
	UPnPControlEvent event;

	event.type = UPNP_CONTROL_EVENT_ACTION_COMPLETE;

	switch (response->status)
	{
		case SOAP_RESPONSE_NORMAL:
			event.data.action.success = 1;
 			event.data.action.response = response->envelope.doc;
 			event.data.action.errorCode = 0;
 			event.data.action.errorDescription = 0;
			break;

		case SOAP_RESPONSE_FAULT:
		{
			UPNP_CHAR* temp;

 			event.data.action.success = 0;
 			event.data.action.response = 0;
			/* Change: Get u:error code and u:errorDescription was missing u:	*/
			/* Try upnp error code. if not found get fault messages */
		    event.data.action.errorDescription = 0;
			temp = UPnP_DOMGetElemTextInElem(response->fault.detailElem, "u:errorCode", 0);
			if (temp)
			{
	 			event.data.action.errorCode = rtp_atoi(temp);
	 			event.data.action.errorDescription = UPnP_DOMGetElemTextInElem (
						response->fault.detailElem,
						"u:errorDescription",
					0
				);
			}
			else
			{
				/* Not exactly sure what to do here. If no upnp error set error to
				   non zero falu and try to get other strings */
	 			event.data.action.errorCode = 1;
			}
		    if (!event.data.action.errorDescription)
			{
	 			event.data.action.errorDescription = UPnP_DOMGetElemTextInElem (
							response->fault.detailElem,	"faultstring",0);
	 			if (!event.data.action.errorDescription)
	 				event.data.action.errorDescription = UPnP_DOMGetElemTextInElem (
						response->fault.detailElem,	"faultcode",0);
	 			if (!event.data.action.errorDescription)
	 				event.data.action.errorDescription = "Fault";
			}
			break;
		}
		case SOAP_REQUEST_ERROR:
// HEREHERE
//			event.data.action.success = 0;
// 			event.data.action.response = 0;
// 			event.data.action.errorCode = 0;
// 			event.data.action.errorDescription = 0;
 			event.data.action.errorCode = 500; // HEREHERE - how do we get the error code (can not be zero)
 			event.data.action.errorDescription = (UPNP_CHAR *) response->fault.string;
			event.data.action.success = 0;
 			event.data.action.response = 0;
			break;
	}

	UPNP_CLIENT_EXIT_API(invokeSm->controlPoint);

	invokeSm->controlPoint->callbackFn (
			invokeSm->controlPoint,
			&event,
			invokeSm->controlPoint->callbackData,
			invokeSm->userData
		);

	UPNP_CLIENT_ENTER_API(invokeSm->controlPoint);

	return (0);
}
