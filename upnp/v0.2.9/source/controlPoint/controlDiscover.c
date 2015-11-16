/*
|  CONTROL_DISCOVER.C -
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

#include "controlDiscover.h"

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

void _UPnP_ControlMSearchCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

int _UPnP_ControlSSDPSearchCallback (
		void* callbackData,
		SSDPSearchResponse* response
	);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*---------------------------------------------------------------------------*/
UPnPControlMSearch* UPnP_ControlMSearchNew (
		UPnPControlPoint* cp,
		UPNP_CHAR* searchType,
		UPNP_INT32 maxResponseTimeoutSec,
		void* userData
	)
{
	UPnPControlMSearch* searchSm;

	searchSm = (UPnPControlMSearch*) rtp_malloc(sizeof(UPnPControlMSearch));
	if (searchSm)
	{
		if (rtp_net_sm_init(&searchSm->base, _UPnP_ControlMSearchCallback) >= 0)
		{
			if (SSDP_SearchInit (
					&searchSm->search,
					&cp->httpClient,
					cp->runtime->ipType,
					searchType,
					maxResponseTimeoutSec,
					_UPnP_ControlSSDPSearchCallback,
					searchSm
				) >= 0)
			{
				searchSm->userData = userData;
				searchSm->controlPoint = cp;

				return (searchSm);
			}

			/* tbd - rtp_net_sm_destroy? */
		}

		rtp_free(searchSm);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlMSearchCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlMSearch* searchSm = (UPnPControlMSearch*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
		{
			result->maxTimeoutMsec = SSDP_SearchAddToSelectList (
					&searchSm->search,
					event->arg.addToSelectList.readList,
					event->arg.addToSelectList.writeList,
					event->arg.addToSelectList.errList
				);
			break;
		}

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			result->done = SSDP_SearchProcessState (
					&searchSm->search,
					event->arg.processState.readList,
					event->arg.processState.writeList,
					event->arg.processState.errList
				);

			if (result->done)
			{
				/* send a UPNP_CONTROL_EVENT_SEARCH_COMPLETE to the
				   Control Point callback */
				UPnPControlEvent event;

				event.type = UPNP_CONTROL_EVENT_SEARCH_COMPLETE;

				UPNP_CLIENT_EXIT_API(searchSm->controlPoint);

				searchSm->controlPoint->callbackFn (
						searchSm->controlPoint,
						&event,
						searchSm->controlPoint->callbackData,
						searchSm->userData
					);

				UPNP_CLIENT_ENTER_API(searchSm->controlPoint);
			}

			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			SSDP_SearchDestroy(&searchSm->search);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(searchSm);
			break;
	}
}

/*---------------------------------------------------------------------------*/
int _UPnP_ControlSSDPSearchCallback (
		void* callbackData,
		SSDPSearchResponse* response
	)
{
	UPnPControlMSearch* searchSm = (UPnPControlMSearch*) callbackData;
	UPnPControlEvent event;
	int result;

	/* tbd -
	UPnP_ControlCacheDevice (
			searchSm->controlPoint,
			response->location,
			response->usn,
			response->searchType,
			response->cacheTimeoutSec
		);
	*/

	event.type = UPNP_CONTROL_EVENT_DEVICE_FOUND;
	event.data.discover.url  = response->location;
	event.data.discover.type = response->searchType;
	event.data.discover.usn  = response->usn;
	event.data.discover.cacheTimeoutSec  = response->cacheTimeoutSec;

	UPNP_CLIENT_EXIT_API(searchSm->controlPoint);

	result = searchSm->controlPoint->callbackFn (
			searchSm->controlPoint,
			&event,
			searchSm->controlPoint->callbackData,
			searchSm->userData
		);

	UPNP_CLIENT_ENTER_API(searchSm->controlPoint);

	if (result == 1)
	{
		return (1);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int UPnP_ControlSSDPCallback (
		SSDPServerContext* ctx,
		SSDPServerRequest* serverRequest,
		void* cookie
	)
{
	UPnPControlPoint* cp = (UPnPControlPoint*) cookie;

	UPNP_CLIENT_ENTER_API(cp);

	switch (serverRequest->type)
	{
		case SSDP_SERVER_REQUEST_NOTIFY:
		{
			UPnPControlEvent event;

			event.type = UPNP_CONTROL_EVENT_NONE;

			if (!rtp_strcmp(serverRequest->data.notify.subType, "ssdp:alive"))
			{
				event.type = UPNP_CONTROL_EVENT_DEVICE_ALIVE;
			}
			else if (!rtp_strcmp(serverRequest->data.notify.subType, "ssdp:byebye") ||
                     !rtp_strcmp(serverRequest->data.notify.subType, "ssdp:bye-bye"))
			{
				event.type = UPNP_CONTROL_EVENT_DEVICE_BYE_BYE;
			}

			event.data.discover.url  = serverRequest->data.notify.location;
			event.data.discover.type = serverRequest->data.notify.type;
			event.data.discover.usn  = serverRequest->data.notify.usn;
			event.data.discover.cacheTimeoutSec  = serverRequest->data.notify.timeout;

			if (event.type != UPNP_CONTROL_EVENT_NONE)
			{
				UPNP_CLIENT_EXIT_API(cp);
				cp->callbackFn(cp, &event, cp->callbackData, 0);
				UPNP_CLIENT_ENTER_API(cp);
			}
			break;
		}
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (0);
}
