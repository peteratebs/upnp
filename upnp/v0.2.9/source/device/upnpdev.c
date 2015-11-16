//
// UPNPDEV.C - Device side API functions and utilities
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "upnp.h"
#include "upnpdom.h"
#include "dllist.h"
#include "deviceDiscover.h"
#include "deviceDescribe.h"
#include "deviceControl.h"
#include "deviceEvent.h"
#include "rtpstdup.h"
#include "rtpthrd.h"
#include "rtpstr.h"
#include "rtpmem.h"
#include "rtptime.h"
#include "rtpprint.h"
#include "assert.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/


/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_DeviceNotifyContext
{
	DOMString          message;
	UPnPService*       service;
	GENAServerNotifier notifierClone;
	UPNP_INT32         firstNotifyDelayMsec;
}
DeviceNotifyContext;

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

void _UPnP_DeviceNotifyThread (void *);
static void releaseMutex(void *p);

static int _UPNP_XmlFileRequestHandler (void *userData,
                                 HTTPServerRequestContext *ctx,
                                 HTTPSession *session,
                                 HTTPRequest *request,
                                 RTP_NET_ADDR *clientAddr);

static void _UPNP_XmlFileDestructor (void *p);



/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
                              UPnP_DeviceInit
 *----------------------------------------------------------------------------*/
/** @memo Initialize a UPnPDeviceRuntime

	@doc
		Initializes all device state data in a \Ref{UPnPDeviceRuntime}
		struct (allocated by the calling application), and binds the
		device to the specified \Ref{UPnPRuntime}.  The \Ref{UPnPRuntime}
		must be initialized via \Ref{UPnP_RuntimeInit} before this function
		is called.  Only one device may be bound to a single
		\Ref{UPnPRuntime} at once.  This function must be called before
		all other device related functions.

    @see UPnP_DeviceFinish

    @return error code
 */
int  UPnP_DeviceInit (
		UPnPDeviceRuntime* deviceRuntime, /** pointer to the device runtime buffer*/
		UPnPRuntime* rt                   /** pointer to an initialized upnp runtime
		                                      buffer. */)
{
	rtp_memset(deviceRuntime, 0, sizeof(UPnPDeviceRuntime));

	DLLIST_INIT(&deviceRuntime->rootDeviceList);

	/* do this first so we grab the specified port ahead of SSDP */
 #ifdef UPNP_MULTITHREAD
	if (rtp_sig_mutex_alloc(&deviceRuntime->mutex, 0) >= 0)
	{
 #endif
		deviceRuntime->upnpRuntime = rt;
		deviceRuntime->deviceSSDPCallback = UPnP_DeviceSSDPCallback;

	  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
		rt->deviceRuntime = deviceRuntime;
		UPNP_RUNTIME_EXIT_CRITICAL(rt);

		return (0);

 #ifdef UPNP_MULTITHREAD
 		rtp_sig_mutex_free(deviceRuntime->mutex);
	}
 #endif

	return (-1);
}

/*----------------------------------------------------------------------------*
                              UPnP_DeviceFinish
 *----------------------------------------------------------------------------*/
/** @memo Destroy a UPnPDeviceRuntime

	@doc
		Cleans up all data associated with a \Ref{UPnPDeviceRuntime} structure.
		Once this function has been called, it is safe to free the memory used
		by the \Ref{UPnPDeviceRuntime} structure.

	@see UPnP_DeviceInit

	@return error code
 */
void UPnP_DeviceFinish(
	UPnPDeviceRuntime *deviceRuntime /** address of runtime of device to destroy */)
{
UPnPRootDevice *rootDevice, *next;
UPnPRuntime* rt = deviceRuntime->upnpRuntime;

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
	UPNP_ASSERT(rt->deviceRuntime == deviceRuntime, "Cannot locate device runtime");
	rt->deviceRuntime = 0;
	UPNP_RUNTIME_EXIT_CRITICAL(rt);
    UPnP_DeviceSendAllByeBye(deviceRuntime);
 #ifdef UPNP_MULTITHREAD
	/* stop all running threads */
	rtp_sig_mutex_free(deviceRuntime->mutex);
 #endif

 	/* Unregister all root devices */
	rootDevice = (UPnPRootDevice *) deviceRuntime->rootDeviceList.next;
	while (rootDevice != (UPnPRootDevice *) &deviceRuntime->rootDeviceList)
	{
		next = (UPnPRootDevice *) rootDevice->device.node.next;
		UPnP_UnRegisterRootDevice(rootDevice);
		rootDevice = next;
	}
}

/*----------------------------------------------------------------------------*
                            UPnP_RegisterRootDevice
 *----------------------------------------------------------------------------*/
 /** @memo Configures the root device and its serives for UPnP
     @doc
		Sets up the device to serve UPnP requests from the clients; set up
		devcive for ssdp announcements if deviceAdvertise is turned on.

    @return error code
 */
int  UPnP_RegisterRootDevice   (
		UPnPDeviceRuntime *deviceRuntime, /** device runtime information */
		const UPNP_CHAR *descDocURL,      /** relative url of device
		                                      description document */
		RTPXML_Document *description,       /** address of DOM representation
		                                      of the device description
		                                      document */
		UPNP_BOOL autoAddr,               /** Select swtich for Auto IP
		                                      if 1 - uses AutoIP
		                                      if 0 - extracts address from the
		                                      device description document */
		UPnPDeviceCallback callback,      /** pointer to the callback function
		                                      for the device */
		void *userData,                   /** user data for callback */
		UPnPRootDeviceHandle *retHandle,  /** handle to the current root device */
		UPNP_BOOL deviceAdvertise         /** Switch to turn ON and OFF device advertising
		                                      If 1 - device will be set up to send periodic
		                                      SSDP announcements.
		                                      If 0 - no ssdp announcements will be send*/)
{
UPnPRootDevice *rootDevice;
UPnPService *service;
int result;
#ifdef UPNP_MULTITHREAD
RTP_MUTEX mutex;

	if (rtp_sig_mutex_alloc(&mutex, 0) < 0)
	{
		return (-1);
	}

#endif

	/* describe the device */
	rootDevice = UPnP_DeviceDescribeRootDevice(description, 32);
	if (!rootDevice)
	{
	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_free(mutex);
	 #endif
		return (-1);
	}

	*retHandle = rootDevice;

	DLLIST_INSERT_BEFORE(&deviceRuntime->rootDeviceList, &rootDevice->device.node);

	/* create the full URL for the root device description document */
	if (descDocURL)
	{
		DOMString urlBase = UPnP_DOMGetElemTextInDoc(description, "URLBase", 0);
		if (urlBase)
		{
			long len = rtp_strlen(descDocURL) + rtp_strlen(urlBase);

			/* dynamically allocate around 32 (in test cases) bytes memory */
			rootDevice->descLocation = (UPNP_CHAR *) rtp_malloc(sizeof(UPNP_CHAR) * (len + 1));
			if (rootDevice->descLocation)
			{
				rtp_strcpy(rootDevice->descLocation, urlBase);
				rtp_strcat(rootDevice->descLocation, descDocURL);
			}
		}
	}

	if (autoAddr)
	{
		HTTP_CHAR *serverPath = rtp_strstr(rootDevice->descLocation, "//");
		if (serverPath)
		{
			serverPath = rtp_strstr(serverPath + 2, "/");
			if (serverPath)
			{
				if (HTTP_ServerAddPath (
						&deviceRuntime->upnpRuntime->httpServer,
						serverPath,
						1,
						_UPNP_XmlFileRequestHandler,
						_UPNP_XmlFileDestructor,
						rootDevice
					) >= 0)
				{
					rootDevice->autoAddr = autoAddr;
				}
			}
		}
	}

    rootDevice->deviceRuntime = deviceRuntime;
    rootDevice->deviceRuntime->announceAll = UPnP_DeviceSendAllAlive;
	rootDevice->userCallback = callback;
	rootDevice->userData = userData;

 #ifdef UPNP_MULTITHREAD
	rootDevice->mutex = mutex;
 #endif

	if(deviceAdvertise)
	{
	    rootDevice->sendingAnnouncements = 1;
	}

	/* add server paths for all services (GENA and SOAP) */
	service = (UPnPService *) rootDevice->serviceList.next;
	while (service != (UPnPService *) &rootDevice->serviceList)
	{
		result = UPnP_DeviceControlBindService (deviceRuntime, service);
		result = UPnP_DeviceEventBindService   (deviceRuntime, service);
		service = (UPnPService *) service->node.next;
	}

	return (0);
}

/*----------------------------------------------------------------------------*
                            UPnP_UnRegisterRootDevice
 *----------------------------------------------------------------------------*/
/** @memo Free root device from its server bindings
     @doc
		Unregisters the root device from the internal server, so that the future
		UPnP requests will not be served for this root device.


    @return error code
 */
int  UPnP_UnRegisterRootDevice (
		UPnPRootDeviceHandle rootDevice /** Handle to root device */
	)
{
UPnPService *service;
int result;

	/* remove server paths for all services (GENA and SOAP) */
	service = (UPnPService *) rootDevice->serviceList.next;
	while (service != (UPnPService *) &rootDevice->serviceList)
	{
		result = UPnP_DeviceControlUnBindService (rootDevice->deviceRuntime, service);
		result = UPnP_DeviceEventUnBindService   (rootDevice->deviceRuntime, service);
		service = (UPnPService *) service->node.next;
	}

 #ifdef UPNP_MULTITHREAD
	rtp_sig_mutex_free(rootDevice->mutex);
 #endif

	if (rootDevice->autoAddr)
	{
		HTTP_CHAR *serverPath = rtp_strstr(rootDevice->descLocation, "//");
		if (serverPath)
		{
			serverPath = rtp_strstr(serverPath + 2, "/");
			if (serverPath)
			{
				HTTP_ServerRemovePath(&rootDevice->deviceRuntime->upnpRuntime->httpServer, serverPath);
			}
		}
	}

	if (rootDevice->descLocation)
	{
		rtp_free(rootDevice->descLocation);
	}

	DLLIST_REMOVE(&rootDevice->device.node);

	UPnP_DeviceFreeRootDevice(rootDevice);

	return (0);
}

/*----------------------------------------------------------------------------*
                               UPnP_DeviceAdvertise
 *----------------------------------------------------------------------------*/
/** @memo set up the device to send periodic SSDP announcements
    @doc
		The function prepares the device to send periodic announcements every
		frequecySec seconds.
    @return error code
 */
int  UPnP_DeviceAdvertise      (
        UPnPRootDeviceHandle rootDevice, /** handle to the device */
        UPNP_INT32 frequencySec,         /** interval in seconds
                                             between two announcements */
        UPNP_INT32 remoteTimeoutSec      /** time in seconds for which the remote
                                             client will cache the information in
                                             the announcement */)
{
UPnPDeviceRuntime *deviceRuntime = rootDevice->deviceRuntime;

    rootDevice->sendingAnnouncements = 1;
    deviceRuntime->announceFrequencySec = frequencySec;
    deviceRuntime->nextAnnounceTimeSec = rtp_get_system_sec();
    deviceRuntime->remoteCacheTimeoutSec = remoteTimeoutSec;
	return (0);
}

/*----------------------------------------------------------------------------*
                                 UPnP_DeviceNotify
 *----------------------------------------------------------------------------*/
/** Sends an event notification message to all the subscribers of the service.

    @return error code
 */
int  UPnP_DeviceNotify         (
        UPnPDeviceRuntime *deviceRuntime, /** device runtime information */
        UPnPRootDeviceHandle rootDevice,  /** handle to the device */
        const UPNP_CHAR *deviceUDN,       /** unique device identifier (UUID in the
                                              device description document) for
                                              the device */
        const UPNP_CHAR *serviceId,       /** unique service identifier (serviceID
                                              in the device description document)
                                              for the service */
        RTPXML_Document *propertySet        /** contains the evented variable and
                                              its value in XML format. */)
{
UPnPService *service;
DOMString message;

	/* first find the specified service */
	service = (UPnPService *) rootDevice->serviceList.next;
	while (service != (UPnPService *) &rootDevice->serviceList)
	{
		if (!rtp_strcmp(service->serviceId, serviceId) && service->device &&
		    !rtp_strcmp(service->device->UDN, deviceUDN))
		{
			break;
		}
		service = (UPnPService *) service->node.next;
	}

	if (service == (UPnPService *) &rootDevice->serviceList)
	{
		/* unknown service/device */
		return (-1);
	}

	message = rtpxmlPrintDocument(propertySet);

	if (message)
	{
		GENAServerNotifier clone;
		GENAServerEvent event;
		GENA_UINT8 *parts[2] = {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n", message};
		GENA_INT32 partLen[2] = {40, rtp_strlen(message)};

		event.contentType   = "text/xml";
		event.notifySubType = "upnp:propchange";
		event.notifyType    = "upnp:event";
		event.numParts      = 2;
		event.parts         = (const GENA_CHAR**)parts;
		event.partLen       = (GENA_UINT32 *) partLen;

	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_claim(service->genaMutex);
	 #endif

		GENA_ServerCloneAllForNotify(&service->genaContext, &service->genaNotifier, &clone);

	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_release(service->genaMutex);
	 #endif

		GENA_ServerSendNotify (
				&service->genaContext,
				&clone,
				&event,
				UPNP_EVENT_SEND_TIMEOUT_SEC * 1000
			);

		GENA_ServerKillNotifier(&service->genaContext, &clone);

		rtpxmlFreeDOMString(message);
	}

	return (0);
}

/*----------------------------------------------------------------------------*
                              UPnP_DeviceNotifyAsync
 *----------------------------------------------------------------------------*/
/** Sends a non blocking event notification message to all the subscribers of
    the service.

    @return error code
 */
int  UPnP_DeviceNotifyAsync (
		UPnPDeviceRuntime *deviceRuntime, /** the runtime for the device*/
		UPnPRootDeviceHandle rootDevice,  /** the root device */
		const UPNP_CHAR *deviceUDN,       /** the UDN of the specific device */
		const UPNP_CHAR *serviceId,       /** the ID of the service notifying */
		RTPXML_Document *propertySet        /** property set to send as new values */
	)
{
UPnPService *service;

	/* first find the specified service */
	service = (UPnPService *) rootDevice->serviceList.next;
	while (service != (UPnPService *) &rootDevice->serviceList)
	{
		if (!rtp_strcmp(service->serviceId, serviceId) && service->device &&
		    !rtp_strcmp(service->device->UDN, deviceUDN))
		{
			break;
		}
		service = (UPnPService *) service->node.next;
	}

	if (service != (UPnPService *) &rootDevice->serviceList) /* service found */
	{
		/* dynamically allocate 28 bytes memory */
		DeviceNotifyContext *ctx = (DeviceNotifyContext *) rtp_malloc(sizeof(DeviceNotifyContext));

		if (ctx)
		{
			ctx->service = service;
			ctx->message = rtpxmlPrintDocument(propertySet);

			if (ctx->message)
			{
				RTP_HANDLE threadHandle;

			 #ifdef UPNP_MULTITHREAD
				rtp_sig_mutex_claim(ctx->service->genaMutex);
			 #endif

				GENA_ServerCloneAllForNotify(&ctx->service->genaContext,
					                   &ctx->service->genaNotifier,
					                   &ctx->notifierClone);

			 #ifdef UPNP_MULTITHREAD
				rtp_sig_mutex_release(ctx->service->genaMutex);
			 #endif

				if (rtp_thread_spawn(&threadHandle,
				                     _UPnP_DeviceNotifyThread,
				                     0, /* thread name */
				                     0, /* stack size (0 == normal) */
				                     0, /* priority (0 == normal) */
				                     ctx) >= 0)
				{
					return (0);
				}

				GENA_ServerKillNotifier(&ctx->service->genaContext, &ctx->notifierClone);

				rtpxmlFreeDOMString(ctx->message);
			}

			rtp_free(ctx);
		}
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                            UPnP_AcceptSubscription
 *----------------------------------------------------------------------------*/
 /** @memo Accept a new subscription request.
     @doc
		This function adds a new subscriber device's internal subscriber's list,
		generates a unique subscription Id for this subscriber, sets a duration
		in seconds for this subscription to be valid and sends a subscription
		response indicating success or failure to subscription request.

    @return error code
 */
int  UPnP_AcceptSubscription   (
        UPnPSubscriptionRequest *subReq, /** address of structure containing subscription
                                             request information */
        const GENA_CHAR *subscriptionId, /** subscription identifier for the subscriber */
        UPNP_INT32 timeoutSec,           /** duration in seconds for which the subscription
                                             is valid */
        RTPXML_Document *propertySet,      /** pointer to response message in XML format.*/
        UPNP_INT32 firstNotifyDelayMsec  /** delay in milliseconds before sending the
                                             first event notification to the new subscriber */
	)
{
GENAServerNotifier firstNotify;
DOMString message;

 #ifdef UPNP_MULTITHREAD
	rtp_sig_mutex_claim(subReq->hidden.service->genaMutex);

	if (GENA_ServerSendAccept (
			&subReq->hidden.service->genaContext,
			&subReq->hidden.service->genaNotifier,
			subReq->hidden.genaRequest, subscriptionId,
			timeoutSec, &firstNotify, releaseMutex,
			&subReq->hidden.service->genaMutex
		) < 0)
	{
		rtp_sig_mutex_release(subReq->hidden.service->genaMutex);

     #else
    	if (GENA_ServerSendAccept (
			&subReq->hidden.service->genaContext,
			&subReq->hidden.service->genaNotifier,
			subReq->hidden.genaRequest, subscriptionId,
			timeoutSec, &firstNotify, 0, 0 ) < 0)
	{
     #endif
		return (-1);
	}

	if (firstNotifyDelayMsec > 0)
	{
		rtp_thread_sleep(firstNotifyDelayMsec);
	}

	message = rtpxmlPrintDocument(propertySet);

	if (message)
	{
		GENAServerEvent event;
		GENA_UINT8* parts[2] = {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n", message};
		GENA_INT32 partLen[2] = {40, rtp_strlen(message)};

		event.contentType   = "text/xml";
		event.notifySubType = "upnp:propchange";
		event.notifyType    = "upnp:event";
		event.numParts      = 2;
		event.parts         = (const GENA_CHAR**)parts;
		event.partLen       = (GENA_UINT32 *) partLen;

		GENA_ServerSendNotify (
				&subReq->hidden.service->genaContext,
				&firstNotify,
				&event,
				UPNP_EVENT_SEND_TIMEOUT_SEC * 1000
			);

		rtpxmlFreeDOMString(message);
	}

	GENA_ServerKillNotifier(&subReq->hidden.service->genaContext, &firstNotify);

	return (0);
}

/*----------------------------------------------------------------------------*
                          UPnP_AcceptSubscriptionAsync
 *----------------------------------------------------------------------------*/
/** Send Subscription Accept asynchronously. Optional parameters may be given
    a value of zero to indicate use default.

    @return error code
 */

int  UPnP_AcceptSubscriptionAsync (
		UPnPSubscriptionRequest *subReq, /** the request being accepted */
		const GENA_CHAR *subscriptionId, /** alternate subscription ID (optional) */
		UPNP_INT32 timeoutSec,           /** remote cache timeout value in
		                                     seconds (optional) */
		RTPXML_Document *propertySet,      /** property set for initial notify */
		UPNP_INT32 firstNotifyDelayMsec  /** delay in milliseconds before sending the
                                             first event notification to the new subscriber */
	)
{
	/* dynamically allocate 28 bytes memory */
	DeviceNotifyContext *ctx = (DeviceNotifyContext*) rtp_malloc(sizeof(DeviceNotifyContext));

	if (ctx)
	{
		GENAServerSubscriber* newSub = 0;

		ctx->service = subReq->hidden.service;
		ctx->firstNotifyDelayMsec = firstNotifyDelayMsec;

	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_claim(subReq->hidden.service->genaMutex);
		if (GENA_ServerSendAccept (
				&subReq->hidden.service->genaContext,
				&subReq->hidden.service->genaNotifier,
				subReq->hidden.genaRequest,
				subscriptionId,
				timeoutSec,
				&ctx->notifierClone,
				releaseMutex,
				&subReq->hidden.service->genaMutex
			) < 0)
		{
			rtp_sig_mutex_release(subReq->hidden.service->genaMutex);
			return (-1);
		}
     #else
        if (GENA_ServerSendAccept (
				&subReq->hidden.service->genaContext,
				&subReq->hidden.service->genaNotifier,
				subReq->hidden.genaRequest,
				subscriptionId,
				timeoutSec,
				&ctx->notifierClone,
				0, 0) < 0)
		{
			return (-1);
		}
     #endif

		ctx->message = rtpxmlPrintDocument(propertySet);

		if (ctx->message)
		{
			RTP_HANDLE threadHandle;

			if (rtp_thread_spawn(&threadHandle,
								 _UPnP_DeviceNotifyThread,
								 0, /* thread name */
								 0, /* stack size (0 == normal) */
								 0, /* priority (0 == normal) */
								 ctx) >= 0)
			{
				return (0);
			}

			rtpxmlFreeDOMString(ctx->message);
		}

		GENA_ServerKillNotifier(&ctx->service->genaContext, &ctx->notifierClone);

		rtp_free(ctx);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                          _UPnP_DeviceNotifyThread
 *----------------------------------------------------------------------------*/
void _UPnP_DeviceNotifyThread (void *p)
{
	DeviceNotifyContext *ctx = 	(DeviceNotifyContext *) p;
	GENAServerEvent event;
	GENA_UINT8 *parts[2] = {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n", ctx->message};
	GENA_INT32 partLen[2] = {40, rtp_strlen(ctx->message)};

	event.contentType   = "text/xml";
	event.notifySubType = "upnp:propchange";
	event.notifyType    = "upnp:event";
	event.numParts      = 2;
	event.parts         = (const GENA_CHAR**)parts;
	event.partLen       = (GENA_UINT32 *) &partLen[0];

	if (ctx->firstNotifyDelayMsec > 0)
	{
		rtp_thread_sleep(ctx->firstNotifyDelayMsec);
	}

	GENA_ServerSendNotify (
			&ctx->service->genaContext,
			&ctx->notifierClone,
			&event,
			UPNP_EVENT_SEND_TIMEOUT_SEC * 1000
		);

	GENA_ServerKillNotifier(&ctx->service->genaContext, &ctx->notifierClone);

	rtpxmlFreeDOMString(ctx->message);

	rtp_free(ctx);
}

/*---------------------------------------------------------------------------*/
void releaseMutex(void *p)
{
	rtp_sig_mutex_release(*((RTP_MUTEX*) p));
}

/*----------------------------------------------------------------------------*
                         _UPNP_XmlFileRequestHandler
 *----------------------------------------------------------------------------*/
/*

    @return error code
 */
int _UPNP_XmlFileRequestHandler (void *userData,
                                 HTTPServerRequestContext *ctx,
                                 HTTPSession *session,
                                 HTTPRequest *request,
                                 RTP_NET_ADDR *clientAddr)
{
	UPnPRootDevice *rootDevice = (UPnPRootDevice *) userData;
	HTTPServerContext *server = ctx->server;

	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			HTTP_UINT8 tempBuffer[256];
			HTTP_INT32 tempBufferSize = 255;
			DOMString xmlString;
			char *temp;

		 #ifdef UPNP_MULTITHREAD
			rtp_sig_mutex_claim(rootDevice->mutex);
		 #endif

			if (rootDevice->autoAddr)
			{
				RTP_NET_ADDR serverAddr;

				HTTP_ServerRequestGetLocalAddr(ctx, &serverAddr);

				temp = UPnP_DOMSubstituteAddr(
							UPnP_DOMGetElemTextInDoc(
									rootDevice->descDoc, "URLBase", 0),
							&serverAddr);
				if (temp)
				{
					UPnP_DOMSetElemTextInDoc(
							rootDevice->descDoc, "URLBase", 0, temp);
					rtp_free(temp);
				}
			}

			xmlString = rtpxmlPrintDocument(rootDevice->descDoc);

		 #ifdef UPNP_MULTITHREAD
			rtp_sig_mutex_release(rootDevice->mutex);
		 #endif

			HTTP_ServerReadHeaders(
					ctx,
					session,
					HTTP_ServerProcessHeaderStub,
					0,
					tempBuffer,
					tempBufferSize
				);

			if (xmlString)
			{
				HTTP_INT32 size = rtp_strlen(xmlString);

				HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
				HTTP_ServerSetDefaultHeaders(ctx, &response);
				HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/xml; charset=\"utf-8\"");
				HTTP_SetResponseHeaderInt(&response, "Content-Length", size + 40);

				HTTP_WriteResponse(session, &response);
				HTTP_Write(session, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n", 40);
				HTTP_Write(session, xmlString, size);
				HTTP_WriteFlush(session);

				HTTP_FreeResponse(&response);

				rtpxmlFreeDOMString(xmlString);

				return (HTTP_REQUEST_STATUS_DONE);
			}

			break;
		}

		case HTTP_METHOD_HEAD: /* tbd - implement HEAD method */
		default:
			break;
	}

	return (HTTP_REQUEST_STATUS_CONTINUE);

}

/*----------------------------------------------------------------------------*
                        _UPNP_XmlFileDestructor
 *----------------------------------------------------------------------------*/
/*

    @return nothing
 */
void _UPNP_XmlFileDestructor (
		void *p)
{
}


/*----------------------------------------------------------------------------*
                            UPnP_GetRequestedDeviceName
 *----------------------------------------------------------------------------*/
/** Extracts Unique device name for the target device from control/subscription request

    @return error code
 */
const UPNP_CHAR* UPnP_GetRequestedDeviceName (
		void *eventStruct,                   /** pointer to request structure */
		enum e_UPnPDeviceEventType eventType /** type of event */
	)
{
	const UPNP_CHAR* deviceName;

	switch (eventType)
	{
		case UPNP_DEVICE_EVENT_ACTION_REQUEST:
		{
			UPnPActionRequest *request = (UPnPActionRequest *) eventStruct;
			deviceName = request->in.deviceUDN;
			break;
		}

		case UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST:
		{
			UPnPSubscriptionRequest *request = (UPnPSubscriptionRequest *) eventStruct;
			deviceName = request->in.deviceUDN;
			break;
		}
	}
	return(deviceName);
}


/*----------------------------------------------------------------------------*
                            UPnP_GetRequestedServiceId
 *----------------------------------------------------------------------------*/
/** Extracts service identifier from a control/subscription request

    @return error code
 */
const UPNP_CHAR* UPnP_GetRequestedServiceId (
		void *eventStruct,                   /** pointer to request structure */
		enum e_UPnPDeviceEventType eventType /** type of event */
	)
{
	const UPNP_CHAR* serviceId;
	switch (eventType)
	{
		case UPNP_DEVICE_EVENT_ACTION_REQUEST:
		{
			UPnPActionRequest *request = (UPnPActionRequest *) eventStruct;
			serviceId = request->in.serviceId;
			break;
		}

		case UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST:
		{
			UPnPSubscriptionRequest *request = (UPnPSubscriptionRequest *) eventStruct;
			serviceId = request->in.serviceId;
			break;
		}
	}
	return(serviceId);
}


/*----------------------------------------------------------------------------*
                            UPnP_GetRequestedActionName
 *----------------------------------------------------------------------------*/
/** Extracts name of the target action from action/subscription request

    @return pointer to action name is available
 */
const UPNP_CHAR* UPnP_GetRequestedActionName (
		void *eventStruct,                   /** pointer to request structure */
		enum e_UPnPDeviceEventType eventType /** type of event */
	)
{
	const UPNP_CHAR* actionName;
	switch (eventType)
	{
		case UPNP_DEVICE_EVENT_ACTION_REQUEST:
		{
			UPnPActionRequest *request = (UPnPActionRequest *) eventStruct;
			actionName = request->in.actionName;
			break;
		}

		case UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST:
		{
			actionName = 0;
			break;
		}
	}
	return(actionName);
}

/*----------------------------------------------------------------------------*
                            UPnP_SetActionErrorResponse
 *----------------------------------------------------------------------------*/
/** Sets error code and error description for a response to an action request

    @return none
 */
void UPnP_SetActionErrorResponse (
		UPnPActionRequest* request, /* pointer to action request */
		UPNP_CHAR *description,     /* error description */
		UPNP_INT32 value            /* error code */
	)
{
	request->out.errorCode = value;
    rtp_strcpy(request->out.errorDescription, description);
}

/*----------------------------------------------------------------------------*
  UPnP_GetArgValue
 *----------------------------------------------------------------------------*/
/** Extracts the value of a given argument from an action. Action information is
    stored in form of IXML element.

    @return error code
 */
const UPNP_CHAR * UPnP_GetArgValue (UPnPActionRequest* request, /* request containing action
                                                                   and its argument */
                                    const UPNP_CHAR *argName    /* Name of argument who's
                                                                   value is to be extracted */)
{
	RTPXML_Element *actionElem = request->in.actionElement;
	return (UPnP_DOMGetElemTextInElem (actionElem, (char *) argName, 0));
}

/*----------------------------------------------------------------------------*
                            UPnP_SetActionResponseArg
 *----------------------------------------------------------------------------*/
/** Inserts name and value of an argument to an action response message

    @return error code
 */
int  UPnP_SetActionResponseArg (UPnPActionRequest* request, /* pointer to buffer containing request
															   and its response */
                                const UPNP_CHAR *name,      /* name of the argument */
                                const UPNP_CHAR *value      /* value of the argument */
                               )
{
	RTPXML_Document *doc = request->out.result;
	return (UPnP_SetActionArg(doc, name, value));
}
