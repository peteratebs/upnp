/*
|  CONTROL_EVENT.C -
|
|  EBS -
|
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "controlEvent.h"
#include "controlDescribe.h"
#include "controlTimer.h"
#include "rtptime.h"
#include "rtpscnv.h"
#include "rtpnetsm.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

#define CALLBACK_URL_LENGTH 50
#define IPADDR_LENGTH       17

#define UPNP_CLIENT_ENTER_API(X)   UPNP_RUNTIME_ENTER_CRITICAL((X)->runtime)
#define UPNP_CLIENT_EXIT_API(X)    UPNP_RUNTIME_EXIT_CRITICAL((X)->runtime)

/*****************************************************************************/
/* Types
 *****************************************************************************/

typedef struct s_UPnPControlSubscriptionExpireTimer
{
	UPnPControlTimerNSM baseSm;
	UPnPControlService* service;
	UPNP_BOOL           sentWarning;
}
UPnPControlSubscriptionExpireTimer;

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

void _UPnP_ControlGENARequestStateMachineCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

void _UPnP_ControlResubscribeStateMachineCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

int _UPnP_ControlGENARequestDoneCallback (
		void* callbackData,
		GENAClientResponse* response
	);

UPnPControlSubscriptionExpireTimer* _UPnP_ControlNewSubscriptionExpireTimer (
		UPnPControlService* service,
		UPNP_INT32 timeoutMsec
	);

/* this will handle both the about-to-expire warning event and the expired
   event for subscriptions */
void _UPnP_ControlSubscriptionExpireTimerCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*----------------------------------------------------------------------------*
 *  					UPnP_ControlNewSubscribeRequest						  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */

UPnPControlSubscription* UPnP_ControlNewSubscribeRequest (
		UPnPControlService* service,
		UPNP_INT32 timeoutSec,
		void* userData,
		UPNP_BOOL generateEvent
	)
{
	UPnPControlSubscription* subscribeSm;

	subscribeSm = (UPnPControlSubscription*) rtp_malloc(sizeof(UPnPControlSubscription));
	if (subscribeSm)
	{
		if (rtp_net_sm_init(&subscribeSm->base, _UPnP_ControlGENARequestStateMachineCallback) >= 0)
		{
			subscribeSm->callbackURL[0] = 0;

			if (GENA_SubscribeRequestInit(
					&subscribeSm->genaRequest,
					&service->device->controlPoint->httpClient,
					service->device->controlPoint->runtime->ipType,
					service->eventURL,
					&service->device->baseURL,
					subscribeSm->callbackURL,
					timeoutSec,
					(GENAClientRequestCallback)_UPnP_ControlGENARequestDoneCallback,
					subscribeSm) >= 0)
			{
				subscribeSm->userData = userData;
				subscribeSm->service = service;
				subscribeSm->generateEvent = generateEvent;

				service->state = UPNP_SERVICE_SUBSCRIBING;
				_UPnP_ControlServiceAddReference(service);

				return (subscribeSm);
			}
		}

		rtp_free(subscribeSm);
	}
	return (0);
}

/*----------------------------------------------------------------------------*
 *  					UPnP_ControlNewRenewRequest                           *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPnPControlSubscription* UPnP_ControlNewRenewRequest (
		UPnPControlService* service,
		UPNP_INT32 timeoutSec,
		void* userData,
		UPNP_BOOL generateEvent
	)
{
	UPnPControlSubscription* renewSM;

	renewSM = (UPnPControlSubscription*) rtp_malloc(sizeof(UPnPControlSubscription));
	if (renewSM)
	{
		if (rtp_net_sm_init(&renewSM->base, _UPnP_ControlGENARequestStateMachineCallback) >= 0)
		{
			if(GENA_RenewRequestInit(
					&renewSM->genaRequest,
					&service->device->controlPoint->httpClient,
					service->device->controlPoint->runtime->ipType,
					service->eventURL,
					&service->device->baseURL,
					service->subscriptionId,
					timeoutSec,
					(GENAClientRequestCallback)_UPnP_ControlGENARequestDoneCallback,
					renewSM) >= 0)
			{
				renewSM->userData = userData;
				renewSM->service = service;
				renewSM->generateEvent = generateEvent;

				_UPnP_ControlServiceAddReference(service);

				return (renewSM);
			}
		}
		rtp_free(renewSM);
	}
	return (0);
}

/*----------------------------------------------------------------------------*
 *  					UPnP_ControlNewUnsubscribeRequest                     *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPnPControlSubscription* UPnP_ControlNewUnsubscribeRequest (
		UPnPControlService* service,
		void* userData,
		UPNP_BOOL generateEvent
	)
{
	UPnPControlSubscription* unsubscribeSm;
	unsubscribeSm = (UPnPControlSubscription*) rtp_malloc(sizeof(UPnPControlSubscription));
	if (unsubscribeSm)
	{
		if (rtp_net_sm_init(&unsubscribeSm->base, _UPnP_ControlGENARequestStateMachineCallback) >= 0)
		{
			if (GENA_UnsubscribeRequestInit (
					&unsubscribeSm->genaRequest,
					&service->device->controlPoint->httpClient,
					service->device->controlPoint->runtime->ipType,
					service->eventURL,
					&service->device->baseURL,
					service->subscriptionId,
					(GENAClientRequestCallback)_UPnP_ControlGENARequestDoneCallback,
					unsubscribeSm) >= 0)
		    {
				unsubscribeSm->userData = userData;
				unsubscribeSm->service = service;
				unsubscribeSm->generateEvent = generateEvent;

				_UPnP_ControlServiceAddReference(service);

				return (unsubscribeSm);
			}
		}
		rtp_free(unsubscribeSm);
	}
	return (0);
}

/*----------------------------------------------------------------------------*
 *                      UPnP_ControlNewResubscribeRequest                     *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPnPControlResubscribe* UPnP_ControlNewResubscribeRequest (
		UPnPControlService* service,
		void* userData
	)
{
	UPnPControlResubscribe* resubSm;

	resubSm = (UPnPControlResubscribe*) rtp_malloc(sizeof(UPnPControlResubscribe));
	if (resubSm)
	{
		if (rtp_net_sm_init(&resubSm->base, _UPnP_ControlResubscribeStateMachineCallback) >= 0)
		{
			resubSm->subSm = UPnP_ControlNewUnsubscribeRequest (service, 0, 0);

			if (resubSm->subSm)
			{
				resubSm->service = service;
				resubSm->state   = UPNP_CONTROL_UNSUBSCRIBING;

				service->state = UPNP_SERVICE_UNSUBSCRIBING;
				_UPnP_ControlServiceAddReference(resubSm->service);

				return (resubSm);
			}
		}

		rtp_free(resubSm);
	}

	return (0);
}

/*----------------------------------------------------------------------------*
 *                      _UPnP_ControlResubscribeCallback                      *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
void _UPnP_ControlResubscribeStateMachineCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlResubscribe* resubSm = (UPnPControlResubscribe*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			if (resubSm->state == UPNP_CONTROL_ERROR)
			{
				result->maxTimeoutMsec = 0;
			}
			else
			{
				_UPnP_ControlGENARequestStateMachineCallback(&resubSm->subSm->base, event, result);
			}
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:
			if (resubSm->state == UPNP_CONTROL_ERROR)
			{
				result->done = 1;
				break;
			}

			_UPnP_ControlGENARequestStateMachineCallback(&resubSm->subSm->base, event, result);
			if (result->done)
			{
				switch (resubSm->state)
				{
					case UPNP_CONTROL_UNSUBSCRIBING:
						rtp_net_sm_delete(resubSm->subSm);

						resubSm->subSm = UPnP_ControlNewSubscribeRequest (
								resubSm->service,
								resubSm->newSubTimeoutSec,
								0,
								0
							);

						if (resubSm->subSm)
						{
							resubSm->state = UPNP_CONTROL_SUBSCRIBING;
							result->done = 0;
						}
						else
						{
							resubSm->state = UPNP_CONTROL_ERROR;
						}
						break;

					case UPNP_CONTROL_SUBSCRIBING:
						resubSm->state = UPNP_CONTROL_RESUBSCRIBED;
						break;

					case UPNP_CONTROL_RESUBSCRIBED:
						break;

					case UPNP_CONTROL_ERROR:
						break;
				}
			}
			break;

		case RTP_NET_SM_EVENT_DESTROY:
			if (resubSm->subSm)
			{
				rtp_net_sm_delete(resubSm->subSm);
				resubSm->subSm = 0;
			}
			_UPnP_ControlServiceRemoveReference(resubSm->service);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(resubSm);
			break;
	}
}

/*----------------------------------------------------------------------------*
 *                 _UPnP_ControlGENARequestStateMachineCallback               *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
void _UPnP_ControlGENARequestStateMachineCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlSubscription* subscriptionSm = (UPnPControlSubscription*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			result->maxTimeoutMsec = GENA_ClientRequestAddToSelectList(
					&subscriptionSm->genaRequest,
					event->arg.addToSelectList.readList,
					event->arg.addToSelectList.writeList,
					event->arg.addToSelectList.errList
				);
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:

			/* tbd - instead of doing this, we should substitute the pointer to
			   the callback URL string in the GENA subscribe request initializer
			   with a pointer to a function that builds the string just before
			   it is needed.  the footprint of this function should be:

				long getCallbackUrl (string buffer, buffer length, callbackData, socket handle)

				returns number of chars written to buffer, -1 if failed

			*/
			if (subscriptionSm->genaRequest.state == GENA_REQUEST_CONNECTING)
			{
				RTP_SOCKET sd = HTTP_ManagedClientGetSessionSocket (
						subscriptionSm->genaRequest.httpSession
					);

				if (rtp_fd_isset(event->arg.processState.writeList, sd))
				{
					RTP_NET_ADDR localAddr;

					if (rtp_net_getsockname (
							sd,
							localAddr.ipAddr,
							&localAddr.port,
							&localAddr.type
						) >= 0)
					{
						UPNP_INT32 callbackUrlLen;

						localAddr.port = HTTP_ServerGetPort (
									&subscriptionSm->service->device->controlPoint->runtime->httpServer
								);

						callbackUrlLen = UPnP_ControlGetEventCallbackURL (
								subscriptionSm->service,
								subscriptionSm->callbackURL + 1,
								UPNP_EVENT_CALLBACK_STR_LEN - 2,
								&localAddr
							);

						if (callbackUrlLen > 0)
						{
							subscriptionSm->callbackURL[0] = '<';
							subscriptionSm->callbackURL[callbackUrlLen+1] = '>';
							subscriptionSm->callbackURL[callbackUrlLen+2] = 0;
						}
						else
						{
							subscriptionSm->callbackURL[0] = 0;
						}
					}
				}
			}

			result->done = GENA_ClientRequestProcessState (
					&subscriptionSm->genaRequest,
					event->arg.processState.readList,
					event->arg.processState.writeList,
					event->arg.processState.errList
				);

			if (result->done)
			{
				if (subscriptionSm->genaRequest.state != GENA_REQUEST_ACCEPTED)
				{
					subscriptionSm->service->state = UPNP_SERVICE_NOT_SUBSCRIBED;
				}
			}
			break;

		case RTP_NET_SM_EVENT_DESTROY:
			GENA_ClientRequestDestroy (&subscriptionSm->genaRequest);
			_UPnP_ControlServiceRemoveReference (subscriptionSm->service);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(subscriptionSm);
			break;
	}
}

/*----------------------------------------------------------------------------*
 *  					_UPnP_ControlEventGENACallback                        *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
int _UPnP_ControlEventGENACallback (
		GENAClientContext* ctx,
		GENAClientEvent* genaEvent,
		void* callbackData
	)
{
	UPnPControlEvent event;
	UPnPControlService* service = (UPnPControlService*) callbackData;

	UPNP_CLIENT_ENTER_API(service->device->controlPoint);

    /* check for invalid sid in this event message */
    /* if the service is still subscribed then match its subscription id*/
    if (service->state == UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT ||
		service->state == UPNP_SERVICE_SUBSCRIBED)
    {
	    if (!rtp_strcmp(genaEvent->sid, service->subscriptionId))
	    {
		   	UPNP_BOOL outOfSequence = UPNP_FALSE;

			if (service->state == UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT)
			{
				service->eventSequence = genaEvent->seq;
				service->state = UPNP_SERVICE_SUBSCRIBED;
			}
			else
			{
				if (service->eventSequence + 1 != genaEvent->seq)
				{
					outOfSequence = UPNP_TRUE;
				}

				/* even if we are ignoring the out-of-sequence event, don't
					keep throwing out-of-sequence events */
				service->eventSequence = genaEvent->seq;
			}

			if (outOfSequence)
			{
				event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_OUT_OF_SYNC;
		    	event.data.notify.deviceHandle = service->device;
				event.data.notify.serviceId = service->serviceId;
				event.data.notify.stateVars = genaEvent->propertySetDoc;

				UPNP_CLIENT_EXIT_API(service->device->controlPoint);

				if (service->device->controlPoint->callbackFn (
						service->device->controlPoint,
						&event,
						service->device->controlPoint->callbackData,
						0
					) == 0)
				{
					/* this means re-subscribe to the service; 1 means
					   just ignore and keep receiving events */

					UPnPControlResubscribe* sm;

					UPNP_CLIENT_ENTER_API(service->device->controlPoint);

					sm = UPnP_ControlNewResubscribeRequest (service, 0);

					if (sm)
					{
						rtp_net_aggregate_sm_add(&service->device->controlPoint->requestManager, (RTP_NET_SM*)sm);
						// tbd - signal upnp daemon
						UPNP_CLIENT_EXIT_API(service->device->controlPoint);
					}
					else
					{
					   	event.type = UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED;
						event.data.subscription.deviceHandle = service->device;
						event.data.subscription.serviceId = service->serviceId;
						event.data.subscription.timeoutSec = 0;

						service->state = UPNP_SERVICE_NOT_SUBSCRIBED;

						UPNP_CLIENT_EXIT_API(service->device->controlPoint);

			            service->device->controlPoint->callbackFn (
								service->device->controlPoint,
								&event,
								service->device->controlPoint->callbackData,
								0
							);
					}
				}

				return (0);
			}
			else
			{
		    	event.type = UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE;
		    	event.data.notify.deviceHandle = service->device;
				event.data.notify.serviceId = service->serviceId;
				event.data.notify.stateVars = genaEvent->propertySetDoc;

				UPNP_CLIENT_EXIT_API(service->device->controlPoint);

	            service->device->controlPoint->callbackFn (
						service->device->controlPoint,
						&event,
						service->device->controlPoint->callbackData,
						0
					);

				return(0);
			}
		}
		else
		{
			genaEvent->status = GENA_EVENT_ERROR_INVALID_SID;
		}
	}

	UPNP_CLIENT_EXIT_API(service->device->controlPoint);

	return(-1);
}

/*----------------------------------------------------------------------------*
 *                   _UPnP_ControlGENARequestDoneCallback                     *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
int _UPnP_ControlGENARequestDoneCallback (
		void* callbackData,
		GENAClientResponse* response
	)
{
	UPnPControlSubscription* subscriptionSm = (UPnPControlSubscription*) callbackData;
	GENAClientRequest* request = &subscriptionSm->genaRequest;
	UPnPControlService* service = subscriptionSm->service;

	if (request->requestType == GENA_REQUEST_SUBSCRIBE)
	{
		UPnPControlSubscriptionExpireTimer* timer;

		service->state = UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT;

		if (response->sid && response->sid[0])
		{
			if (service->subscriptionId)
			{
				rtp_strfree(service->subscriptionId);
			}
			service->subscriptionId = rtp_strdup(response->sid);
		}

		service->subscriptionTimeoutSec = response->timeoutSec;

		timer = _UPnP_ControlNewSubscriptionExpireTimer(service, response->timeoutSec * 1000);
		if (timer)
		{
			rtp_net_aggregate_sm_add(&service->device->controlPoint->requestManager, (RTP_NET_SM*)timer);
			// tbd - signal upnp daemon
		}
		/* tbd - else handle this failure somehow, because the app won't get
		   an expiration notice, and it is expecting to */
	}
	else if (request->requestType == GENA_REQUEST_RENEW)
	{
		service->subscriptionTimeoutSec = response->timeoutSec;
	}

	if (subscriptionSm->generateEvent)
	{
		UPnPControlEvent event;

	    if (request->state == GENA_REQUEST_ACCEPTED)
	    {
			event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED;
		}
		else
		{
			event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED;
		}

		if(request->requestType == GENA_REQUEST_UNSUBSCRIBE)
		{
			event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED;
		}

		event.data.subscription.deviceHandle = service->device;
		event.data.subscription.serviceId    = service->serviceId;
		event.data.subscription.timeoutSec   = response->timeoutSec;

		UPNP_CLIENT_EXIT_API(subscriptionSm->service->device->controlPoint);

		subscriptionSm->service->device->controlPoint->callbackFn (
						subscriptionSm->service->device->controlPoint,
						&event,
						subscriptionSm->service->device->controlPoint->callbackData,
						subscriptionSm->userData
					);

		UPNP_CLIENT_ENTER_API(subscriptionSm->service->device->controlPoint);
	}

    return(0);
}

// tbd - move this somewhere that makes sense.
/*----------------------------------------------------------------------------*
 *                      UPnP_ControlGetEventCallbackPath                      *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPNP_INT32 UPnP_ControlGetEventCallbackPath (
		UPnPControlService* service,
		UPNP_CHAR* strOut,
		UPNP_INT32 bufferLen
	)
{
	if (bufferLen >= 24)
	{
		rtp_strcpy(strOut, "/genaEvent-");
		rtp_itoa(
			service->eventCallbackPathId,
			strOut + 11,
			10);

		return (rtp_strlen(strOut));
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *                      UPnP_GetRelativeLocalAddr                             *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
int UPnP_GetRelativeLocalAddr (
		RTP_NET_ADDR* localAddr,
		RTP_NET_ADDR* remoteAddr
	)
{
	RTP_SOCKET tempSock;

	if (rtp_net_socket_datagram_dual(&tempSock, remoteAddr->type) >= 0)
	{
		if (rtp_net_connect_dual (
				tempSock,
                RTP_NET_DATAGRAM,
				remoteAddr->ipAddr,
				remoteAddr->port,
				remoteAddr->type
			) >= 0)
		{
			if (rtp_net_getsockname (
					tempSock,
					localAddr->ipAddr,
					&localAddr->port,
					&localAddr->type
				) >= 0)
			{
				rtp_net_closesocket(tempSock);
				return (0);
			}
		}
		rtp_net_closesocket(tempSock);
	}

	return (-1);
}


/*----------------------------------------------------------------------------*
 *                      UPnP_ControlGetEventCallbackURL                       *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPNP_INT32 UPnP_ControlGetEventCallbackURL (
		UPnPControlService* service,
		UPNP_CHAR* strOut,
		UPNP_INT32 bufferLen,
		RTP_NET_ADDR* localAddr
	)
{
	UPNP_INT32 tempLen;
	UPNP_INT32 len = 0;

	/* protocol is always HTTP */
	rtp_strcpy(strOut, "http://");
	strOut += 7;
	bufferLen -= 7;
	len += 7;

	if (rtp_net_ip_to_str (
		strOut,
		localAddr->ipAddr,
		localAddr->type) >= 0)
	{
		/* append out IP address (relative to the remote host) */
		tempLen = rtp_strlen(strOut);
		strOut += tempLen;
		bufferLen -= tempLen;
		len += tempLen;

		/* append ':' */
		strOut[0] = ':';
		strOut++;
		bufferLen--;
		len++;

		/* append the port number */
		rtp_itoa(localAddr->port, strOut, 10);
		tempLen = rtp_strlen(strOut);
		strOut += tempLen;
		bufferLen -= tempLen;
		len += tempLen;

		/* append the callback server path */
		tempLen = UPnP_ControlGetEventCallbackPath(service, strOut, bufferLen);
		if (tempLen > 0)
		{
			len += tempLen;
			return (len);
		}
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *                 _UPnP_ControlNewSubscriptionExpireTimer                    *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
UPnPControlSubscriptionExpireTimer* _UPnP_ControlNewSubscriptionExpireTimer (
		UPnPControlService* service,
		UPNP_INT32 timeoutMsec
	)
{
	UPnPControlSubscriptionExpireTimer*	expireSm;

	expireSm = (UPnPControlSubscriptionExpireTimer*) rtp_malloc(sizeof(UPnPControlSubscriptionExpireTimer));

	if (expireSm)
	{
		if (UPnP_ControlTimerNSMInit (
				&expireSm->baseSm,
				timeoutMsec,
				_UPnP_ControlSubscriptionExpireTimerCallback
			) >= 0)
		{
			expireSm->sentWarning = UPNP_FALSE;
			expireSm->service = service;

			_UPnP_ControlServiceAddReference(service);

			return (expireSm);
		}

		rtp_free(expireSm);
	}

	return (0);
}

/*----------------------------------------------------------------------------*
 *            _UPnP_ControlSubscriptionExpireTimerCallback                    *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return
 */
void _UPnP_ControlSubscriptionExpireTimerCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlSubscriptionExpireTimer* expireSm = (UPnPControlSubscriptionExpireTimer*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			UPnP_ControlTimerNSMCallback(sm, event, result);
			if (!expireSm->sentWarning && result->maxTimeoutMsec >= expireSm->service->expireWarningMsec)
			{
				result->maxTimeoutMsec -= expireSm->service->expireWarningMsec;
			}
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			UPNP_INT32 msecsLeft = expireSm->baseSm.expiresAtMsec - rtp_get_system_msec();

			if (!expireSm->sentWarning && (msecsLeft <= expireSm->service->expireWarningMsec))
			{
				/* send a warning that the subscription on this service is about to expire */

				UPnPControlEvent event;

				event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION;
				event.data.subscription.deviceHandle = expireSm->service->device;
				event.data.subscription.serviceId    = expireSm->service->serviceId;
				event.data.subscription.timeoutSec   = (msecsLeft + 999) / 1000;

				expireSm->sentWarning = UPNP_TRUE;

				UPNP_CLIENT_EXIT_API(expireSm->service->device->controlPoint);

				expireSm->service->device->controlPoint->callbackFn (
						expireSm->service->device->controlPoint,
						&event,
						expireSm->service->device->controlPoint->callbackData,
						0 /* no request data because this is not a request-related event */
					);

				UPNP_CLIENT_ENTER_API(expireSm->service->device->controlPoint);
			}

			if (expireSm->service->state != UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT &&
				expireSm->service->state != UPNP_SERVICE_SUBSCRIBED)
			{
				/* if for whatever reason we are no longer subscribed, delete this timer */
				result->done = 1;
				break;
			}

			UPnP_ControlTimerNSMCallback(sm, event, result);

			if (result->done)
			{
				UPNP_CHAR serverPath[UPNP_EVENT_CALLBACK_STR_LEN];
				UPnPControlEvent event;
				UPnPRuntime* rt = expireSm->service->device->controlPoint->runtime;
				UPnPControlPoint* cp = expireSm->service->device->controlPoint;

				event.type = UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED;
				event.data.subscription.deviceHandle = expireSm->service->device;
				event.data.subscription.serviceId    = expireSm->service->serviceId;
				event.data.subscription.timeoutSec   = 0;

				/* remove ourselves from the HTTP server */
				UPnP_ControlGetEventCallbackPath (
						expireSm->service,
						serverPath,
						UPNP_EVENT_CALLBACK_STR_LEN
					);

				expireSm->service->state = UPNP_SERVICE_NOT_SUBSCRIBED;

				UPNP_CLIENT_EXIT_API(cp);

				UPNP_RUNTIME_ENTER_CRITICAL(rt);
				HTTP_ServerRemovePath(&rt->httpServer, serverPath);
				UPNP_RUNTIME_EXIT_CRITICAL(rt);

				expireSm->service->device->controlPoint->callbackFn (
						cp,
						&event,
						cp->callbackData,
						0 /* no request data because this is not a request-related event */
					);

				UPNP_CLIENT_ENTER_API(expireSm->service->device->controlPoint);
			}
			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			UPnP_ControlTimerNSMCallback(sm, event, result);
			_UPnP_ControlServiceRemoveReference(expireSm->service);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(expireSm);
			break;
	}
}
