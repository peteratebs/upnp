//
// EVENT.C -
//
// EBS -
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "upnpdev.h"
#include "deviceEvent.h"
#include "genasrv.h"
#include "upnpdom.h"
#include "rtpmem.h"
#include "rtpstr.h"
#include "rtpstdup.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

int UPnP_EventGENACallback (
		GENAServerContext *ctx,
		GENAServerRequest *request,
		void *cookie);

static char * _getEventSubURL (
		RTPXML_Document *descDoc,
		RTPXML_Element *serviceElement);

static void releaseMutex(void *p);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  UPnP_DeviceEventBindService   (UPnPDeviceRuntime *deviceRuntime,
                                    UPnPService *service)
{
HTTP_CHAR *path = _getEventSubURL(service->device->rootDevice->descDoc, service->serviceElement);

	if (path)
	{
		if (GENA_ServerInit(&service->genaContext, deviceRuntime->upnpRuntime->ipType, UPnP_EventGENACallback, service) < 0)
		{
			rtp_free(path);
			return (-1);
		}

		if (GENA_ServerInitNotifier(&service->genaContext, &service->genaNotifier) < 0)
		{
			rtp_free(path);
			GENA_ServerShutdown(&service->genaContext);
			return (-1);
		}

		if (HTTP_ServerAddPath(&deviceRuntime->upnpRuntime->httpServer,
		                       path,
		                       1,
                                       (HTTPRequestHandler)GENA_ServerProcessRequest,
                                       0,
		                       &service->genaContext) < 0)
		{
			rtp_free(path);
			GENA_ServerKillNotifier(&service->genaContext, &service->genaNotifier);
			GENA_ServerShutdown(&service->genaContext);
			return (-1);
		}

	 #ifdef UPNP_MULTITHREAD
		if (rtp_sig_mutex_alloc(&service->genaMutex, 0) < 0)
		{
			HTTP_ServerRemovePath(&deviceRuntime->upnpRuntime->httpServer, path);
			rtp_free(path);
			GENA_ServerKillNotifier(&service->genaContext, &service->genaNotifier);
			GENA_ServerShutdown(&service->genaContext);
			return (-1);
		}
	 #endif

		rtp_free(path);
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  UPnP_DeviceEventUnBindService (UPnPDeviceRuntime *deviceRuntime,
                                    UPnPService *service)
{
HTTP_CHAR *path = _getEventSubURL(service->device->rootDevice->descDoc, service->serviceElement);

	if (path)
	{
		/* build the event URL from the baseURL and eventSubURL in the DOM */

		if (HTTP_ServerRemovePath(&deviceRuntime->upnpRuntime->httpServer,
		                          path) < 0)
		{
			rtp_free(path);
			return (-1);
		}

		GENA_ServerKillNotifier(&service->genaContext, &service->genaNotifier);
		GENA_ServerShutdown(&service->genaContext);

	 #ifdef UPNP_MULTITHREAD
	    rtp_sig_mutex_free(service->genaMutex);
	 #endif

		rtp_free(path);
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
void releaseMutex(void *p)
{
	rtp_sig_mutex_release(*((RTP_HANDLE *) p));
}

/*---------------------------------------------------------------------------*/
int UPnP_EventGENACallback (
		GENAServerContext *ctx,
		GENAServerRequest *request,
		void *cookie)
{
UPnPService *service = (UPnPService *) cookie;

	switch (request->in.type)
	{
		case GENA_REQUEST_SUBSCRIBE:
			if (!request->in.callbackURL[0] && request->in.subscriptionId[0])
			{
				/* if the callback URL is absent, but a subscription id is
				   present, then this is a renewal request */

			 #ifdef UPNP_MULTITHREAD
				rtp_sig_mutex_claim(service->genaMutex);
				if (GENA_ServerSendAccept(ctx, &service->genaNotifier, request, 0, 0, 0,
					                releaseMutex, &service->genaMutex) < 0)
				{
				 	rtp_sig_mutex_release(service->genaMutex);
				}
			 #else
			 GENA_ServerSendAccept(ctx, &service->genaNotifier, request, 0, 0,
			                       0, 0, 0);
			 #endif

				break;
			}
			else if (!rtp_strcmp(request->in.notificationType, "upnp:event") &&
			         request->in.callbackURL[0])
			{
				UPnPRootDevice *rootDevice = service->device->rootDevice;

				if (rootDevice->userCallback)
				{
					int result;
					UPnPSubscriptionRequest subRequest;

					rtp_memset(&subRequest, 0, sizeof(UPnPSubscriptionRequest));

					/* call back to the application */
					subRequest.hidden.genaRequest     = request;
					subRequest.hidden.service         = service;
					subRequest.in.clientAddr          = request->in.clientAddr;
					subRequest.in.serviceId           = service->serviceId;
					subRequest.in.deviceUDN           = service->device->UDN;
					subRequest.in.subscriptionId      = request->in.subscriptionId;
					subRequest.in.requestedTimeoutSec = request->in.requestedTimeoutSec;

					result = rootDevice->userCallback(rootDevice->userData,
						                              rootDevice->deviceRuntime,
						                              rootDevice,
						                              UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST,
						                              &subRequest);
				}
			}
			break;

		case GENA_REQUEST_UNSUBSCRIBE:
		 #ifdef UPNP_MULTITHREAD
			rtp_sig_mutex_claim(service->genaMutex);
			if (GENA_ServerSendAccept(ctx, &service->genaNotifier, request, 0, 0, 0,
				                      releaseMutex, &service->genaMutex) < 0)
			{
				rtp_sig_mutex_release(service->genaMutex);
			}
		 #else
		 	GENA_ServerSendAccept(ctx, &service->genaNotifier, request, 0, 0, 0,
				                  0, 0);
		 #endif

			break;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
char * _getEventSubURL (RTPXML_Document *descDoc, RTPXML_Element *serviceElement)
{
DOMString eventSubUrl;

	eventSubUrl = UPnP_DOMGetElemTextInElem(serviceElement, "eventSubURL", 0);

	if (!eventSubUrl)
	{
		return (0);
	}

	return (UPnP_DOMGetServerPath(descDoc, eventSubUrl));
}
