//
// UPNPCLI.C -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:08 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/
#include "rtpstr.h"
#include "upnpcli.h"
#include "upnpdom.h"
#include "controlDiscover.h"
#include "controlDescribe.h"
#include "controlAction.h"

#include "controlEvent.h"
#include "rtpnetsm.h"
#include "rtptime.h"
#include "rtpstdup.h"
#include "urlparse.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define UPNP_CLIENT_ENTER_API(X)   UPNP_RUNTIME_ENTER_CRITICAL((X)->runtime)
#define UPNP_CLIENT_EXIT_API(X)    UPNP_RUNTIME_EXIT_CRITICAL((X)->runtime)

#define UPNP_CONTROL_SUB_EXPIRE_WARNING_MSEC 15000

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

UPnPControlDevice* _UPnP_ControlNewDevice (
		UPnPControlPoint* cp
	);

void _UPnP_ControlDeviceAddReference (
		UPnPControlDevice* device
	);

void _UPnP_ControlDeviceRemoveReference (
		UPnPControlDevice* device
	);

void _UPnP_ControlDeleteDevice (
		UPnPControlDevice* device
	);

void _UPnP_ControlServiceAddReference (
		UPnPControlService* service
	);

void _UPnP_ControlServiceRemoveReference (
		UPnPControlService* service
	);

void _UPnP_ControlDeleteService (
		UPnPControlService* service
	);

GENA_INT32 _UPnP_ControlGENAListenerCallback (
		GENAClientContext* ctx,
		GENAClientEvent* event,
		void* userData
	);

UPnPControlService* _UPnP_ControlDeviceGetService (
		UPnPControlDevice* device,
		const UPNP_CHAR* serviceId
	);

/* API-level call; will lock the control point mutex */
void UPnP_ControlExecuteNSM (
		UPnPControlPoint* cp,
		void* v_sm,
		UPNP_INT32 maxTimeoutMsec
	);

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
// the network state machine callback for UPnPControlPoint::requestManager
//  this is needed to make sure we've locked the control point context
//  before processing state machines to protect against race conditions
void UPnP_ControlRequestManagerCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlPoint* cp = (UPnPControlPoint*) sm;

	UPNP_CLIENT_ENTER_API(cp);
	rtp_net_aggregate_sm_callback(sm, event, result);
	UPNP_CLIENT_EXIT_API(cp);
}


/*---------------------------------------------------------------------------
  UPnP_ControlPointInit
  ---------------------------------------------------------------------------*/
/**
	@memo Initialize a UPnPControlPoint

	@doc
		Initializes all control point state data in a \Ref{UPnPControlPoint}
		struct (allocated by the calling application), and binds the
		control point to the specified \Ref{UPnPRuntime}. The \Ref{UPnPRuntime}
		must be initialized via \Ref{UPnP_RuntimeInit} before this function
		is called.  Only one control point may be bound to a single
		\Ref{UPnPRuntime} at once.  This function must be called before
		all other control point related functions.

	@see UPnP_ControlPointDestroy

	@return error code
 */
 int UPnP_ControlPointInit (
		UPnPControlPoint* cp,                /** pointer to control point
		                                         context instance */
		UPnPRuntime* rt,                     /** UPnP runtime to associate with
		                                         this control point */
		UPnPControlPointCallback callbackFn, /** callback for this control
		                                         point */
		void* callbackData                   /** application-specific data
		                                         which will be passed into the
		                                         callback */
	)
{
	// make sure that requestManager is the first member of struct
	//  s_UPnPControlPoint
	if( (void*) cp == (void*) &cp->requestManager )
	{
	if (rtp_net_aggregate_sm_init (
			&cp->requestManager,
			UPnP_ControlRequestManagerCallback
		) >= 0)
	{
		if (HTTP_ManagedClientInit (
				&cp->httpClient,
				"EBS Control Point" /* User-Agent */,
				0                      /* Accept (mime types) */,
				0                      /* KeepAlive (bool) */,
				0                      /* Cookie context */,
				0                      /* Authentication context */,
				0                      /* SSL context */,
				0                      /* SSL enabled? (bool) */,
				8192                   /* write buffer size */,
				0                      /* max connections (0 == no limit) */,
				0                      /* max connections per host
				                          (0 == no limit) */
			) >= 0)
		{
			cp->runtime = rt;
			cp->callbackFn = callbackFn;
			cp->callbackData = callbackData;
			cp->controlSSDPCallback = UPnP_ControlSSDPCallback; // dynamic
			                                                    // binding

			  	// make sure you've not already initialized a control point
			  	//  context for this UPnPRuntime
			  	if(rt->controlPoint == 0)
			  	{
		  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
			rt->controlPoint = cp;
			UPNP_RUNTIME_EXIT_CRITICAL(rt);

			return (0);
		}
			}

			rtp_net_sm_delete(&cp->requestManager);
		}
	}
	return (-1);
}

/*---------------------------------------------------------------------------
  UPnP_ControlPointDestroy
  ---------------------------------------------------------------------------*/
/**
	@memo Destroy a UPnPControlPoint

	@doc
		Cleans up all data associated with a \Ref{UPnPControlPoint} structure.
		Once this function has been called, it is safe to free the memory used
		by the \Ref{UPnPControlPoint} structure.

	@see UPnP_ControlPointInit

	@return error code
 */
void UPnP_ControlPointDestroy (
		UPnPControlPoint* cp,          /** the control point to destroy */
		UPNP_INT32 gracefulTimeoutMsec /** if this control point has any
		                                   outstanding operations, wait for
		                                   this many milliseconds to allow them
		                                   to complete gracefully.  After
		                                   timeout expires, do hard close. */
	)
{
	UPnPRuntime* rt = cp->runtime;

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
	UPNP_ASSERT(rt->controlPoint == cp, "Unable to locate the control point to destroy ");
	rt->controlPoint = 0;
	UPNP_RUNTIME_EXIT_CRITICAL(rt);

	if (gracefulTimeoutMsec != 0)
	{
		UPnP_ControlExecuteNSM(cp, &cp->requestManager, gracefulTimeoutMsec);
	}

	// order is important here; there may be state machines referenced
	//  by requestManager that have HTTPManagedClientSession's to close down
	rtp_net_sm_delete(&cp->requestManager);
	HTTP_ManagedClientDestroy(&cp->httpClient);
}

/*---------------------------------------------------------------------------*/
// Discovery

/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
// internal utility function used by UPnP_ControlFindAll,
//   UPnP_ControlFindAllDevices, UPnP_ControlFindDevicesByType,
//   UPnP_ControlFindDevicesByUUID, and UPnP_ControlFindDevicesByService
//  This function should not normally be called directly from the application
int UPnP_ControlSearch (
		UPnPControlPoint* cp,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_CHAR* st,
		UPNP_BOOL waitForCompletion
	)
{
	UPnPControlMSearch* search;

	UPNP_CLIENT_ENTER_API(cp);

	search = UPnP_ControlMSearchNew (
			cp,
			st,
			(timeoutMsec + 999) / 1000, /* search duration */
			userData /* userData */
		);

	if (search)
	{
		if (waitForCompletion)
		{
			UPNP_CLIENT_EXIT_API(cp);

			UPnP_ControlExecuteNSM(cp, search, RTP_TIMEOUT_INFINITE);

			UPNP_CLIENT_ENTER_API(cp);
			rtp_net_sm_delete(search);
		}
		else
		{
			/* add to the requestManager aggregate SM for processing */
			rtp_net_aggregate_sm_add(&cp->requestManager, (RTP_NET_SM*)search);
			// tbd - signal upnp daemon
		}

		UPNP_CLIENT_EXIT_API(cp);
		return (0);
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (-1);
}

/*---------------------------------------------------------------------------
  UPnP_ControlFindAll
  ---------------------------------------------------------------------------*/
/**
	@memo Search for UPnP devices.

	@doc
		Sends a request for all UPnP devices on the network to make their
		presence known to the control point.  If this search method is
		used, then seperate UPNP_CONTROL_EVENT_DEVICE_FOUND events will be
		generated for each root device, embedded device, and service that
		responds.

		When the timeout has been reached, a UPNP_CONTROL_EVENT_SEARCH_COMPLETE

		event will be sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_FOUND
			UPNP_CONTROL_EVENT_SEARCH_COMPLETE

	@see UPnP_ControlFindAllDevices

	@return error code
 */
int UPnP_ControlFindAll (
		UPnPControlPoint* cp,       /** control point, initialized by
		                                \Ref{UPnP_ControlPointInit} */
		UPNP_INT32 timeoutMsec,     /** total duration of time, in milliseconds
		                                , for the search */
		void* userData,             /** passed to callback as userRequestData
		                                for UPNP_CONTROL_EVENT_DEVICE_FOUND
		                                event */
		UPNP_BOOL waitForCompletion /** if UPNP_TRUE, this function will not
		                                return until the search completes; else
		                                the function will return immediately
		                                after sending the multicast search
		                                request. */
	)
{
	return (UPnP_ControlSearch(cp, timeoutMsec, userData, "ssdp:all", waitForCompletion));
}

/*---------------------------------------------------------------------------
  UPnP_ControlFindAllDevices
  ---------------------------------------------------------------------------*/
/**
	@memo Search for UPnP devices.

	@doc
		Sends a request for all UPnP devices on the network to make their
		presence known to the control point.  Only one
		UPNP_CONTROL_EVENT_DEVICE_FOUND event per root device will be generated
		if this function is used.

		When the timeout has been reached, a UPNP_CONTROL_EVENT_SEARCH_COMPLETE

		event will be sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_FOUND
			UPNP_CONTROL_EVENT_SEARCH_COMPLETE

	@see UPnP_ControlFindAll

	@return error code
 */
int UPnP_ControlFindAllDevices (
		UPnPControlPoint* cp,       /** control point, initialized by
		                                \Ref{UPnP_ControlPointInit} */
		UPNP_INT32 timeoutMsec,     /** total duration of time, in milliseconds
		                                , for the search */
		void* userData,             /** passed to callback as userRequestData
		                                for UPNP_CONTROL_EVENT_DEVICE_FOUND
		                                event */
		UPNP_BOOL waitForCompletion /** if UPNP_TRUE, this function will not
		                                return until the search completes; else
		                                the function will return immediately
		                                after sending the multicast search
		                                request. */
	)
{
	return (UPnP_ControlSearch(cp, timeoutMsec, userData, "upnp:rootdevice", waitForCompletion));
}

/*---------------------------------------------------------------------------
  UPnP_ControlFindDevicesByType
  ---------------------------------------------------------------------------*/
/**
	@memo Search for UPnP devices of a certain type.

	@doc
		Sends a request for all UPnP devices of a certain type on the network
		to make their presence known to the control point.  One
		UPNP_CONTROL_EVENT_DEVICE_FOUND event per device that matches the
		search type will be generated if this function is used.

		When the timeout has been reached, a UPNP_CONTROL_EVENT_SEARCH_COMPLETE

		event will be sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_FOUND
			UPNP_CONTROL_EVENT_SEARCH_COMPLETE

	@see UPnP_ControlFindDevicesByUUID, UPnP_ControlFindDevicesByService

	@return error code
 */
int UPnP_ControlFindDevicesByType (
		UPnPControlPoint* cp,       /** control point, initialized by
		                                \Ref{UPnP_ControlPointInit} */
		UPNP_CHAR* deviceType,      /** device type to search for, as specified
		                                by the UPnP Forum. */
		UPNP_INT32 timeoutMsec,     /** total duration of time, in milliseconds
		                                , for the search */
		void* userData,             /** passed to callback as userRequestData
		                                for UPNP_CONTROL_EVENT_DEVICE_FOUND
		                                event */
		UPNP_BOOL waitForCompletion /** if UPNP_TRUE, this function will not
		                                return until the search completes; else
		                                the function will return immediately
		                                after sending the multicast search
		                                request. */
	)
{
	UPNP_CHAR st[256 + 28];

	rtp_strcpy(st, "urn:schemas-upnp-org:device:");
	rtp_strncpy(st + 28, deviceType, 255);
	st[255 + 28] = 0;

	return (UPnP_ControlSearch(cp, timeoutMsec, userData, st, waitForCompletion));
}

/*---------------------------------------------------------------------------
  UPnP_ControlFindDevicesByUUID
  ---------------------------------------------------------------------------*/
/**
	@memo Search for a particular UPnP device.

	@doc
		Sends a request for UPnP devices with the given UUID(unique identifier)
		to make their presence known to the control point.  One
		UPNP_CONTROL_EVENT_DEVICE_FOUND event per device that matches the
		search type will be generated if this function is used.

		When the timeout has been reached, a UPNP_CONTROL_EVENT_SEARCH_COMPLETE

		event will be sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_FOUND
			UPNP_CONTROL_EVENT_SEARCH_COMPLETE

	@see UPnP_ControlFindDevicesByType

	@return error code
 */
int UPnP_ControlFindDevicesByUUID (
		UPnPControlPoint* cp,       /** control point, initialized by
		                                \Ref{UPnP_ControlPointInit} */
		UPNP_CHAR* uuid,            /** uuid to search for, as specified by the
		                                UPnP Forum. */
		UPNP_INT32 timeoutMsec,     /** total duration of time, in milliseconds
		                                , for the search */
		void* userData,             /** passed to callback as userRequestData
		                                for UPNP_CONTROL_EVENT_DEVICE_FOUND
		                                event */
		UPNP_BOOL waitForCompletion /** if UPNP_TRUE, this function will not
		                                return until the search completes; else
		                                the function will return immediately
		                                after sending the multicast search
		                                request. */
	)
{
	UPNP_CHAR st[256 + 5];

	rtp_strcpy(st, "uuid:");
	rtp_strncpy(st + 5, uuid, 255);
	st[255 + 5] = 0;

	return (UPnP_ControlSearch(cp, timeoutMsec, userData, st, waitForCompletion));
}

/*---------------------------------------------------------------------------
  UPnP_ControlFindDevicesByService
  ---------------------------------------------------------------------------*/
/**
	@memo Search for UPnP devices that offer a certain service.

	@doc
		Sends a request for UPnP devices with one or more services of the given
		type to make their  presence known to the control point.  One
		UPNP_CONTROL_EVENT_DEVICE_FOUND event per device that matches the
		search type will be generated if this function is used.

		When the timeout has been reached, a UPNP_CONTROL_EVENT_SEARCH_COMPLETE

		event will be sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_FOUND
			UPNP_CONTROL_EVENT_SEARCH_COMPLETE

	@see UPnP_ControlFindDevicesByType

	@return error code
 */
int UPnP_ControlFindDevicesByService (
		UPnPControlPoint* cp,       /** control point, initialized by
		                                \Ref{UPnP_ControlPointInit} */
		UPNP_CHAR* serviceType,     /** service type to search for, as
		                                specified by the UPnP Forum. */
		UPNP_INT32 timeoutMsec,     /** total duration of time, in milliseconds
		                                ,for the search */
		void* userData,             /** passed to callback as userRequestData
		                                for UPNP_CONTROL_EVENT_DEVICE_FOUND
		                                event */
		UPNP_BOOL waitForCompletion /** if UPNP_TRUE, this function will not
		                                return until the search completes; else
		                                the function will return immediately
		                                after sending the multicast search
		                                request. */
	)
{
	UPNP_CHAR st[256 + 29];

	rtp_strcpy(st, "urn:schemas-upnp-org:service:");
	rtp_strncpy(st + 29, serviceType, 255);
	st[255 + 29] = 0;

	return (UPnP_ControlSearch(cp, timeoutMsec, userData, st, waitForCompletion));
}

/*---------------------------------------------------------------------------*/
// Description

/*-------------------------------------------------------------------*/
// device-related

/*---------------------------------------------------------------------------*/
// internal utility function used by UPnP_ControlOpenDevice and
//   UPnP_ControlOpenDeviceAsync.
//  This function should not normally be called directly from the application
int _UPnP_ControlOpenDevice (
		UPnPControlPoint* cp,
		UPNP_CHAR* url,
		void* userData,
		UPNP_BOOL waitForCompletion,
		UPnPControlDeviceHandle* handle
	)
{
	UPnPControlDevice* device;

	UPNP_CLIENT_ENTER_API(cp);

	device = _UPnP_ControlNewDevice(cp);
	if (device)
	{
		UPnPControlOpenDevice* sm;

		if (handle)
		{
			*handle = device;
		}

		if (waitForCompletion)
		{
            sm = UPnP_ControlNewOpenDevice(cp, device, url, userData, 0);
        }
        else
        {
            sm = UPnP_ControlNewOpenDevice(cp, device, url, userData, 1);
        }
		if (sm)
		{
			if (waitForCompletion)
			{
				/* this ensures device will still be around below */
				_UPnP_ControlDeviceAddReference(device);

				UPNP_CLIENT_EXIT_API(cp);

				UPnP_ControlExecuteNSM(cp, sm, RTP_TIMEOUT_INFINITE);

				UPNP_CLIENT_ENTER_API(cp);
				rtp_net_sm_delete(sm);

				/* device is still valid; see comment above */
				if (device->descDoc && device->deviceElement)
				{
					_UPnP_ControlDeviceRemoveReference(device);
					UPNP_CLIENT_EXIT_API(cp);
					return (0);
				}

				/* device open failed */
				if (handle)
				{
					*handle = 0;
				}
			}
			else
			{
				/* add to the requestManager aggregate SM for processing */
				rtp_net_aggregate_sm_add(&cp->requestManager, (RTP_NET_SM*)sm);
				// tbd - signal upnp daemon
				UPNP_CLIENT_EXIT_API(cp);
				return (0);
			}
		} // else failed: could not create "open device" state machine

		_UPnP_ControlDeleteDevice(device);
	}
	// else failed: could not create device object

	UPNP_CLIENT_EXIT_API(cp);

	return (-1);
}

/*---------------------------------------------------------------------------
  UPnP_ControlOpenDevice
  ---------------------------------------------------------------------------*/
/**
	@memo Open a remote device for description, control, and eventing.

	@doc
		Opens the device at the specified URL.  This function is used to
		obtain a UPnPControlDeviceHandle, which is used for most functions
		which operate within the UPnP description, control, and eventing
		phases.

		This function will block until the device open has completed or failed.

		Generates:
			(no events)

	@see UPnP_ControlOpenDeviceAsync

	@return UPnPControlDeviceHandle for the open device, or 0 on failure
 */
UPnPControlDeviceHandle UPnP_ControlOpenDevice (
		UPnPControlPoint* cp, /** control point, initialized by
		                          \Ref{UPnP_ControlPointInit} */
		UPNP_CHAR* url        /** url of device to open */
	)
{
	UPnPControlDeviceHandle handle;

	if (_UPnP_ControlOpenDevice(cp, url, 0, 1, &handle) >= 0)
	{
		return (handle);
	}

	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlOpenDeviceAsync
  ---------------------------------------------------------------------------*/
/**
	@memo Asynchronously open a remote device for description, control, and
	      eventing.

	@doc
		Opens the device at the specified URL.  This function is used to
		obtain a UPnPControlDeviceHandle, which is used for most functions
		which operate within the UPnP description, control, and eventing
		phases.

		This function will return immediately if the control point is able to
		initiate the device open successfully.  One of the events listed
		below will be sent to the control point once the open completes (and
		the UPnPControlDeviceHandle is available) or an error occurs.

		Generates:
			UPNP_CONTROL_EVENT_DEVICE_OPEN
			UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED

	@see UPnP_ControlOpenDeviceAsync

	@return UPnPControlDeviceHandle for the open device, or 0 on failure
 */
int UPnP_ControlOpenDeviceAsync (
		UPnPControlPoint* cp,  /** control point, initialized by
		                           \Ref{UPnP_ControlPointInit} */
		UPNP_CHAR* url,        /** url of device to open */
		void* userData         /** passed to callback as userRequestData for
		                           UPNP_CONTROL_EVENT_DEVICE_OPEN event */
	)
{
	return (_UPnP_ControlOpenDevice(cp, url, userData, 0, 0));
}

/*---------------------------------------------------------------------------
  UPnP_ControlCloseDevice
  ---------------------------------------------------------------------------*/
/**
	@memo Close an open device handle

	@doc
		Closes a device opened with \Ref{UPnP_ControlOpenDevice} or
		\Ref{UPnP_ControlOpenDeviceAsync}.  The device handle passed in may
		not be used after this function is called.

		Generates:
			(no events)

	@see UPnP_ControlOpenDevice, UPnP_ControlOpenDeviceAsync

	@return UPnPControlDeviceHandle for the open device, or 0 on failure
 */
void UPnP_ControlCloseDevice (
		UPnPControlDeviceHandle deviceHandle /** handle returned by
		                                         \Ref{UPnP_ControlOpenDevice}*/
	)
{
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);
	_UPnP_ControlDeviceRemoveReference(deviceHandle);
	UPNP_CLIENT_EXIT_API(cp);
}

/*---------------------------------------------------------------------------*/
// utility function used by UPnP_ControlGetDeviceInfo and
//   UPnP_ControlGetServiceOwnerDeviceInfo
int _UPnP_ControlGetDeviceInfo (
		RTPXML_Element* deviceElement,
		UPnPControlDeviceInfo* info
	)
{
    info->deviceType       = UPnP_DOMGetElemTextInElem (deviceElement, "deviceType", 0);
  	info->friendlyName     = UPnP_DOMGetElemTextInElem (deviceElement, "friendlyName", 0);
  	info->manufacturer     = UPnP_DOMGetElemTextInElem (deviceElement, "manufacturer", 0);
  	info->manufacturerURL  = UPnP_DOMGetElemTextInElem (deviceElement, "manufacturerURL", 0);
  	info->modelDescription = UPnP_DOMGetElemTextInElem (deviceElement, "modelDescription", 0);
	info->modelName        = UPnP_DOMGetElemTextInElem (deviceElement, "modelName", 0);
	info->modelNumber      = UPnP_DOMGetElemTextInElem (deviceElement, "modelNumber", 0);
	info->modelURL         = UPnP_DOMGetElemTextInElem (deviceElement, "modelURL", 0);
	info->serialNumber     = UPnP_DOMGetElemTextInElem (deviceElement, "serialNumber", 0);
	info->UDN              = UPnP_DOMGetElemTextInElem (deviceElement, "UDN", 0);
	info->UPC              = UPnP_DOMGetElemTextInElem (deviceElement, "UPC", 0);
	info->presentationURL  = UPnP_DOMGetElemTextInElem (deviceElement, "presentationURL", 0);

	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetDeviceInfo
  ---------------------------------------------------------------------------*/
/**
	@memo Get information for an open device.

	@doc
		Populates the fields of the specified \Ref{UPnPControlDeviceInfo}
		structure with various information about the given device, such as
		device type, UDN, manufacturer, model number, etc.

		Generates:
			(no events)

	@see UPnP_ControlGetServiceOwnerDeviceInfo

	@return error code
 */
int UPnP_ControlGetDeviceInfo (
		UPnPControlDeviceHandle device, /** handle returned by
		                                    \Ref{UPnP_ControlOpenDevice} */
		UPnPControlDeviceInfo* info     /** \Ref{UPnPControlDeviceInfo} to populate */
	)
{
	int result;
	UPnPControlPoint* cp = device->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);
	result = _UPnP_ControlGetDeviceInfo(device->deviceElement, info);
	UPNP_CLIENT_EXIT_API(cp);

	return (result);
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetServiceOwnerDeviceInfo
  ---------------------------------------------------------------------------*/
/**
	@memo Get information for the parent device of a service

	@doc
		Populates the fields of the specified \Ref{UPnPControlDeviceInfo}
		structure with various information about the parent device of the
		given service.  By contrast, \Ref{UPnP_ControlGetDeviceInfo} gets
		information about the root device ONLY.  This function is useful for
		gathering information about embedded UPnP devices.

		Generates:
			(no events)

	@see UPnP_ControlGetDeviceInfo, UPnP_ControlGetServiceType

	@return error code
 */
int UPnP_ControlGetServiceOwnerDeviceInfo (
		UPnPControlDeviceHandle handle,  /** handle returned by
		                                     \Ref{UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,            /** serviceId of service for whose
		                                     parent to get info */
		UPnPControlDeviceInfo* info      /** \Ref{UPnPControlDeviceInfo} to
		                                     populate */
	)
{
	int result = -1;
	UPnPControlService* service;
	UPnPControlPoint* cp = handle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(handle, serviceId);

	if (service)
	{
		RTPXML_Node* serviceListNode = rtpxmlNode_getParentNode((RTPXML_Node*) service->serviceElement);

		if (serviceListNode)
		{
			RTPXML_Node* deviceNode = rtpxmlNode_getParentNode(serviceListNode);

			if (deviceNode)
			{
				if (rtpxmlNode_getNodeType(deviceNode) == rtpxeELEMENT_NODE)
				{
					RTPXML_Element* deviceElement = (RTPXML_Element*) deviceNode;
					const DOMString tagName = rtpxmlElement_getTagName (deviceElement);

					if (tagName && !rtp_strcmp(tagName, "device"))
					{
						result = _UPnP_ControlGetDeviceInfo(deviceElement, info);
					}
				}
			}
		}
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (result);
}

/*-------------------------------------------------------------------*/
// service-related

/*---------------------------------------------------------------------------
  UPnP_ControlGetServiceType
  ---------------------------------------------------------------------------*/
/**
	@memo Get the type of a service

	@doc
		Returns the service type (as defined by the UPnP Forum) of the given
		service instance on an open device.  The string passed back by this
		function must not be modified in any way.  It is valid until
		\Ref{UPnP_ControlDeviceClose} is called on the given device handle.

		Generates:
			(no events)

	@see UPnP_ControlDeviceClose, UPnP_ControlGetServiceOwnerDeviceInfo

	@return error code
 */
UPNP_CHAR* UPnP_ControlGetServiceType (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId                  /** serviceId of service to get
		                                          type for */
	)
{
	int n = 0;
	RTPXML_Element* serviceElement;
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	if (serviceId)
	{

	UPNP_CLIENT_ENTER_API(cp);

	serviceElement = UPnP_DOMGetElemInDoc (
			deviceHandle->descDoc,
			"service",
			n
		);

	while (serviceElement)
	{
		UPNP_CHAR* searchServiceId = UPnP_DOMGetElemTextInElem (
				serviceElement,
				"serviceId",
				0
			);

		if (searchServiceId && !rtp_strcmp(serviceId, searchServiceId))
		{
			UPNP_CHAR* serviceType = UPnP_DOMGetElemTextInElem (
					serviceElement,
					"serviceType",
					0
				);

			UPNP_CLIENT_EXIT_API(cp);

			return (serviceType);
		}

		n++;

		serviceElement = UPnP_DOMGetElemInDoc (
				deviceHandle->descDoc,
				"service",
				n
			);
	}

	UPNP_CLIENT_EXIT_API(cp);
    }
	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetServices
  ---------------------------------------------------------------------------*/
/**
	@memo Enumerate the services on a device

	@doc
		Initializes a \Ref{UPnPControlServiceIterator} to enumerate all the
		services on all embedded devices on the given device.

		Generates:
			(no events)

	@see UPnP_ControlGetServicesByType, UPnP_ControlNextService,
	     UPnP_ControlServiceIteratorDone

	@return error code
 */
int UPnP_ControlGetServices (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPnPControlServiceIterator* i         /** uninitialized \Ref
		                                          {UPnPControlServiceIterator}
		                                          to use for this enumeration*/
	)
{
	return (UPnP_ControlGetServicesByType(deviceHandle, i, 0));
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetServicesByType
  ---------------------------------------------------------------------------*/
/**
	@memo Enumerate the services of a certain type on a device

	@doc
		Initializes a \Ref{UPnPControlServiceIterator} to enumerate all the
		services of the given type (as defined by the UPnP Forum) on the given
		device.

		Generates:
			(no events)

	@see UPnP_ControlGetServices, UPnP_ControlNextService,
	     UPnP_ControlServiceIteratorDone

	@return error code
 */
int UPnP_ControlGetServicesByType (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPnPControlServiceIterator* i,        /** uninitialized \Ref
		                                          {UPnPControlServiceIterator}
		                                          to use for this enumeration*/
		UPNP_CHAR* serviceType                /** type of service to
		                                          enumerate */
	)
{
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	if (!deviceHandle->descDoc)
	{
		UPNP_CLIENT_EXIT_API(cp);
		return (-1);
	}

	i->device = deviceHandle;
	i->n = 0;
	i->filterByType = serviceType;

	_UPnP_ControlDeviceAddReference(deviceHandle);

	UPNP_CLIENT_EXIT_API(cp);

	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlNextService
  ---------------------------------------------------------------------------*/
/**
	@memo Enumerate the next service on the device.

	@doc
		Returns the unique serviceId of the next service instance in the
		given enumeration (initialized by \Ref{UPnP_ControlGetServices} or
		\Ref{UPnP_ControlGetServicesByType}).  The string returned by this
		function must not be modified in any way, and is valid until
		\Ref{UPnP_ControlDeviceClose} is called for this device.

		Generates:
			(no events)

	@see UPnP_ControlGetServices, UPnP_ControlGetServicesByType,
	     UPnP_ControlServiceIteratorDone

	@return serviceId if there is a next service; otherwise NULL
 */
UPNP_CHAR* UPnP_ControlNextService (
		UPnPControlServiceIterator* i  /** \Ref{UPnPControlServiceIterator}
		                                   initialized by \Ref
		                                   {UPnP_ControlGetServices} or \Ref
		                                   {UPnP_ControlGetServicesByType} */
	)
{
	UPnPControlPoint* cp = i->device->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	if (i->device->descDoc)
	{
		RTPXML_Element* serviceElement = UPnP_DOMGetElemInDoc (
				i->device->descDoc,
				"service",
				i->n
			);

		while (serviceElement)
		{
			UPNP_CHAR* serviceType = 0;
			UPNP_CHAR* serviceId = UPnP_DOMGetElemTextInElem (
					serviceElement,
					"serviceId",
					0
				);

			i->n++;

			if (serviceId)
			{
				if (i->filterByType)
				{
					serviceType = UPnP_DOMGetElemTextInElem (
							serviceElement,
							"serviceType",
							0
						);

					if (serviceType &&
					    (!rtp_strcmp(serviceType, i->filterByType) ||
					     (!rtp_strncmp(serviceType, "urn:schemas-upnp-org:service:", 29) &&
						  !rtp_strcmp(serviceType + 29, i->filterByType))))
					{
						UPNP_CLIENT_EXIT_API(cp);
						return (serviceId);
					}
				}
				else
				{
					UPNP_CLIENT_EXIT_API(cp);
					return (serviceId);
				}
			}

			serviceElement = UPnP_DOMGetElemInDoc (
					i->device->descDoc,
					"service",
					i->n
				);
		}
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlServiceIteratorDone
  ---------------------------------------------------------------------------*/
/**
	@memo Clean up when done enumerating services.

	@doc
		Must be called when done enumerating services using
		\Ref{UPnP_ControlNextService}.

		Generates:
			(no events)

	@see UPnP_ControlGetServices, UPnP_ControlGetServicesByType,
	     UPnP_ControlNextService

	@return nothing
 */
void UPnP_ControlServiceIteratorDone (
		UPnPControlServiceIterator* i /** \Ref{UPnPControlServiceIterator}
		                                  initialized by \Ref
		                                  {UPnP_ControlGetServices} or \Ref
		                                  {UPnP_ControlGetServicesByType} */
	)
{
	UPnPControlPoint* cp = i->device->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);
	_UPnP_ControlDeviceRemoveReference(i->device);
	UPNP_CLIENT_EXIT_API(cp);
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetServiceInfo
  ---------------------------------------------------------------------------*/
/**
	@memo Get detailed information about a service.

	@doc
		This function retrieves the service description document(SCPD) from the
		given device and parses it into an XML DOM structure.  The resulting
		RTPXML_Document returned by this function is owned by the caller of this
		function.  See the UPnP Forum SCPD definition for more information on
		the content and structure of this document.

		When this function returns, the document has either successfully loaded,
		or an error has occurred

		Generates:
			(no events)

	@see UPnP_ControlGetServiceInfoAsync

	@return XML DOM tree for the given document, or NULL on error
 */
RTPXML_Document* UPnP_ControlGetServiceInfo (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId                  /** id of service to get detailed
		                                          info for */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		UPnPControlGetServiceInfo* sm;

		sm = UPnP_ControlNewGetServiceInfo (
				deviceHandle->controlPoint,
				service,
				0,          /* userData */
				UPNP_FALSE  /* generateEvent */
			);

		if (sm)
		{
			RTPXML_Document* descDoc = 0;

			_UPnP_ControlServiceAddReference(service);

			UPNP_CLIENT_EXIT_API(cp);

			UPnP_ControlExecuteNSM(cp, sm, RTP_TIMEOUT_INFINITE);

			UPNP_CLIENT_ENTER_API(cp);

			descDoc = sm->scpdDoc;
			rtp_net_sm_delete(sm);

			_UPnP_ControlServiceRemoveReference(service);

			UPNP_CLIENT_EXIT_API(cp);

			return (descDoc);
		}

		// no need to delete the service here since _UPnP_ControlDeviceGetService
		//  doesn't actually create it, it just finds it if it already
		//  exists
	}
	// else fail: service not found

	UPNP_CLIENT_EXIT_API(cp);

	return (0);
}

/*---------------------------------------------------------------------------
  UPnP_ControlGetServiceInfoAsync
  ---------------------------------------------------------------------------*/
/**
	@memo Asynchronous get detailed information about a service.

	@doc
		This function retrieves the service description document (SCPD) from
		the given device and parses it into an XML DOM structure.  The
		RTPXML_Document is passed back to the control point through a
		UPNP_CONTROL_EVENT_SERVICE_INFO_READ event.

		This function will return immediately; an event from the list below
		is sent to the control point when the SCPD has been downloaded, or the
		operation fails due to some error.

		Generates:
			UPNP_CONTROL_EVENT_SERVICE_INFO_READ,
			UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED

	@see UPnP_ControlGetServiceInfo

	@return error code
 */
int UPnP_ControlGetServiceInfoAsync  (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,                 /** id of service to get detailed
		                                          info for */
		void* userData                        /** passed to callback as
		                                          userRequestData for generated
		                                          events */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		UPnPControlGetServiceInfo* sm;

		sm = UPnP_ControlNewGetServiceInfo (
				deviceHandle->controlPoint,
				service,
				userData,  /* userData */
				UPNP_TRUE  /* generateEvent */
			);

		if (sm)
		{
			rtp_net_aggregate_sm_add(&deviceHandle->controlPoint->requestManager, (RTP_NET_SM*)sm);
			// tbd - signal upnp daemon
			UPNP_CLIENT_EXIT_API(cp);
			return (0);
		}

		// no need to delete the service here since _UPnP_ControlDeviceGetService
		//  doesn't actually create it, it just finds it if it already
		//  exists
	}
	// else fail: service not found

	UPNP_CLIENT_EXIT_API(cp);

	return (-1);
}

/*---------------------------------------------------------------------------*/
// Control

/*---------------------------------------------------------------------------
  UPnP_ControlInvokeAction
  ---------------------------------------------------------------------------*/
/**
	@memo Invoke an action on a remote service

	@doc
		This function will send the given action to a service on a remote
		device and generate an event that contains the response to that action.
		If waitForCompletion is UPNP_TRUE, then UPnP_ControlInvokeAction will
		not return until the UPNP_CONTROL_EVENT_ACTION_COMPLETE event is
		passed to the control point callback.  Otherwise, it will return
		immediately and the event is sent to the control point when the
		action completes or an error occurs.

		The action passed into this function must be generated using
		\Ref{UPnP_CreateAction} and \Ref{UPnP_SetActionArg}.

		Generates:
			UPNP_CONTROL_EVENT_ACTION_COMPLETE

	@see

	@return error code
 */
int UPnP_ControlInvokeAction (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,                 /** id of service to invoke
		                                          action on */
		const UPNP_CHAR* actionName,          /** name of action to invoke */
		RTPXML_Document* action,                /** SOAP action message created
		                                          using \Ref{UPnP_CreateAction}
		                                          and \Ref{UPnP_SetActionArg} */
		void* userData,                       /** passed to callback as
		                                          userRequestData for generated
		                                          events */
		UPNP_BOOL waitForCompletion           /** see above description */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;
	int result = -1;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		UPnPControlInvokeAction* sm;

		sm = UPnP_ControlNewInvokeAction(service, actionName, action, userData);
		if (sm)
		{
			if (waitForCompletion)
			{
				_UPnP_ControlServiceAddReference(service);

				UPNP_CLIENT_EXIT_API(cp);

				UPnP_ControlExecuteNSM(cp, sm, RTP_TIMEOUT_INFINITE);

				UPNP_CLIENT_ENTER_API(cp);

				if (sm->soapCtx.state == SOAP_ACTION_RESPONSE_READ)
				{
					result = 0;
				}

				rtp_net_sm_delete(sm);

				_UPnP_ControlServiceRemoveReference(service);
			}
			else
			{
				/* add to the requestManager aggregate SM for processing */
				rtp_net_aggregate_sm_add(&deviceHandle->controlPoint->requestManager, (RTP_NET_SM*)sm);
				// tbd - signal upnp daemon

				UPNP_CLIENT_EXIT_API(cp);

				return (0); // tbd - return -2 for so-far-so-good?
			}
		}
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (result);
}

/*---------------------------------------------------------------------------*/
// Eventing

/*---------------------------------------------------------------------------
  UPnP_ControlSubscribe
  ---------------------------------------------------------------------------*/
/**
	@memo Subscribe to a service or renew a subscription

	@doc
		Subscribes to a service on a remote device to receive notifications
		when the service's state changes.  If the control point is already
		subscribed to the given service, this function has the effect of
		renewing the existing subscription for the given period of time.

		If waitForCompletion is UPNP_TRUE, this function will wait until the
		subscription request has been either accepted or rejected.  Otherwise
		it returns immediately and a UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED
		or UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED event is generated once
		the device responds.

		Once the control point is subscribed to service,
		UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE	events may be generated to
		indicate the service's state has been updated. If such a notification
		is dropped, a UPNP_CONTROL_EVENT_SUBSCRIPTION_OUT_OF_SYNC event will be
		sent to the control point, which gives the application the opportunity
		to indicate through the callback return value whether to drop the
		subscription or attempt to re-subscribe.  If the application chooses to
		re-subscribe (synchronize), a UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED
		event may be generated, if the synchronization fails.  If the
		UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED is not handled by the control
		point callback, the default action is to try to synchronize the
		subscription by re-subscribing.

		When the subscription is close to expiring, the control point will
		receive a UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION event, to
		give the application a chance to renew the subscription.

		Once subscription expires, a UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED
		event is sent to the control point.

		Generates:
			UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED,
			UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED,
			UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE,
			UPNP_CONTROL_EVENT_SUBSCRIPTION_OUT_OF_SYNC,
			UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED,
			UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION,
			UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED

	@see UPnP_ControlUnsubscribe, UPnP_ControlSubscribedToService

	@return error code
 */
int UPnP_ControlSubscribe (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,                 /** id of service to subscribe
		                                          to */
		UPNP_INT32 timeoutSec,                /** duration in seconds for the
		                                          subscription */
		void* userData,                       /** passed to callback as
		                                          userRequestData for generated
		                                          events */
		UPNP_BOOL waitForCompletion           /** see above description */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;
	RTP_NET_SM* sm = 0;
	int result = -1;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		switch (service->state)
		{
			case UPNP_SERVICE_NOT_SUBSCRIBED:
			{
				// first time subscription
				sm = (RTP_NET_SM*) UPnP_ControlNewSubscribeRequest (service, timeoutSec, userData, UPNP_TRUE);
				if (sm)
				{
					int serverBindResult;
					UPnPRuntime* rt = service->device->controlPoint->runtime;
					UPNP_CHAR serverPath[UPNP_EVENT_CALLBACK_STR_LEN];

					UPnP_ControlGetEventCallbackPath (
							service,
							serverPath,
							UPNP_EVENT_CALLBACK_STR_LEN
						);

					UPNP_CLIENT_EXIT_API(cp);

					/* set the http server path to handle event callpack */
					UPNP_RUNTIME_ENTER_CRITICAL(rt);

					serverBindResult = HTTP_ServerAddPath (
							&rt->httpServer,
							serverPath,              /* server path */
							HTTP_TRUE,               /* exactMatch */
							(HTTPRequestHandler)GENA_ClientProcessEvent,
							                         /* callback function */
							(HTTPServerPathDestructor)HTTP_NULL,
							                         /* destructor callback */
							&service->genaListener   /* callback data */
						);

					UPNP_RUNTIME_EXIT_CRITICAL(rt);

					if (serverBindResult < 0)
					{
						rtp_net_sm_delete(sm);
						return (-1);
					}

					UPNP_CLIENT_ENTER_API(cp);
				}
				break;
			}

			case UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT: // intentional fall-through
			case UPNP_SERVICE_SUBSCRIBED:
			{
				// renew existing subscription
				sm = (RTP_NET_SM*) UPnP_ControlNewRenewRequest (service, timeoutSec, userData, UPNP_TRUE);
				if (!sm)
				{
					UPNP_CLIENT_EXIT_API(cp);
					return (-3); /* error: could not initiate renew request; still subscribed & listening for events */
				}
				break;
			}

			case UPNP_SERVICE_CANNOT_SUBSCRIBE:
			     /* service does not offer subscriptions */
			default:
				UPNP_CLIENT_EXIT_API(cp);
				return (-1); /* error: state not compatible with subscribe */
		}

		if (sm)
		{
			if (waitForCompletion)
			{
				UPNP_CLIENT_EXIT_API(cp);

				UPnP_ControlExecuteNSM(cp, sm, RTP_TIMEOUT_INFINITE);

				UPNP_CLIENT_ENTER_API(cp);
				rtp_net_sm_delete(sm);

				if (service->state == UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT ||
				    service->state == UPNP_SERVICE_SUBSCRIBED)
				{
					result = 0;
				}
			}
			else
			{
				/* add to the requestManager aggregate SM for processing */
				rtp_net_aggregate_sm_add(&deviceHandle->controlPoint->requestManager, sm);
				// tbd - signal upnp daemon
				// tbd - return -2 for so-far-so-good?
				//result = -2;
                result = 0; //tbd
			}
		}// else fail: state machine create error
	}// else fail: service not found

	UPNP_CLIENT_EXIT_API(cp);
	return (result);
}

/*---------------------------------------------------------------------------
  UPnP_ControlSubscribedToService
  ---------------------------------------------------------------------------*/
/**
	@memo Return whether or not the control point is subscribed to the given
	      service

	@doc

	@see UPnP_ControlSubscribe, UPnP_ControlUnsubscribe

	@return UPNP_TRUE if subscribed, UPNP_FALSE otherwise
 */
UPNP_BOOL UPnP_ControlSubscribedToService (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId                  /** id of service */
	)
{
	UPnPControlService* service;
    UPnPControlPoint* cp = deviceHandle->controlPoint;
    UPNP_BOOL isSubscribed = UPNP_FALSE;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		isSubscribed = (service->state == UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT ||
		                service->state == UPNP_SERVICE_SUBSCRIBED);
	}

	UPNP_CLIENT_EXIT_API(cp);

	return (isSubscribed);
}

/*---------------------------------------------------------------------------
  UPnP_ControlSetServiceSubscriptionExpireWarning
  ---------------------------------------------------------------------------*/
/**
	@memo Sets the time offset before subscription expiration at which a
	      warning event will be generated.

	@doc  This function should be called, if desired, before subscribing to
	      the given service.

	@see  UPnP_ControlSubscribe, UPnP_ControlUnsubscribe

	@return nothing
 */
void UPnP_ControlSetServiceSubscriptionExpireWarning (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,                 /** id of service */
		UPNP_INT32 warningMsec                /** time offset to generate
		                                          warning, in milliseconds */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		service->expireWarningMsec = warningMsec;
	}

	UPNP_CLIENT_EXIT_API(cp);
}

/*---------------------------------------------------------------------------
  UPnP_ControlUnsubscribe
  ---------------------------------------------------------------------------*/
/**
	@memo Cancel a subscribed service

	@doc
		If waitForCompletion is UPNP_TRUE, this function waits for the
		unsubscribe operation to complete before returning.  Otherwise, it
		returns immediately and a UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED
		event is generated when the operation completes.

		Generates:
			UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED

	@see

	@return error code
 */
int UPnP_ControlUnsubscribe (
		UPnPControlDeviceHandle deviceHandle, /** handle returned by \Ref
		                                          {UPnP_ControlOpenDevice} */
		UPNP_CHAR* serviceId,                 /** id of service to unsubscribe
		                                          from */
		void* userData,                       /** passed to callback as
		                                          userRequestData for generated
		                                          events */
		UPNP_BOOL waitForCompletion           /** see above description */
	)
{
	UPnPControlService* service;
	UPnPControlPoint* cp = deviceHandle->controlPoint;
	int result = -1;

	UPNP_CLIENT_ENTER_API(cp);

	service = _UPnP_ControlDeviceGetService(deviceHandle, serviceId);
	if (service)
	{
		if (service->state != UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT &&
		    service->state != UPNP_SERVICE_SUBSCRIBED)
		{
			/* error: not subscribed to this service */
			result = -3;
		}
		else
		{
			UPnPControlSubscription* sm;
			UPnPRuntime* rt = service->device->controlPoint->runtime;
			UPNP_CHAR serverPath[UPNP_EVENT_CALLBACK_STR_LEN];

			UPnP_ControlGetEventCallbackPath (
					service,
					serverPath,
					UPNP_EVENT_CALLBACK_STR_LEN
				);

			service->state = UPNP_SERVICE_NOT_SUBSCRIBED;

			_UPnP_ControlServiceAddReference(service);
			UPNP_CLIENT_EXIT_API(cp);

			UPNP_RUNTIME_ENTER_CRITICAL(rt);
			HTTP_ServerRemovePath(&rt->httpServer, serverPath);
			UPNP_RUNTIME_EXIT_CRITICAL(rt);

			UPNP_CLIENT_ENTER_API(cp);

			sm = UPnP_ControlNewUnsubscribeRequest (service, userData, UPNP_TRUE /* generate event */);

			_UPnP_ControlServiceRemoveReference(service);

			if (sm)
			{
				if (waitForCompletion)
				{
					UPNP_CLIENT_EXIT_API(cp);

					UPnP_ControlExecuteNSM(cp, sm, RTP_TIMEOUT_INFINITE);

					UPNP_CLIENT_ENTER_API(cp);

					// tbd - check state here
					result = 0;

					rtp_net_sm_delete(sm);
				}
				else
				{
					/* add to the requestManager aggregate SM for processing */
					rtp_net_aggregate_sm_add(&deviceHandle->controlPoint->requestManager, (RTP_NET_SM*)sm);
					// tbd - signal upnp daemon
					// tbd - return -2 for so-far-so-good
					result = 0;
				}
			}
			else
			{
				 // fail: could not create state machine
				 result = -1;
			}
		}
	}// else fail: service not found

	UPNP_CLIENT_EXIT_API(cp);
	return (result);
}

/*---------------------------------------------------------------------------*/
UPnPControlDevice* _UPnP_ControlNewDevice (
		UPnPControlPoint* cp
	)
{
	UPnPControlDevice* device;

	device = (UPnPControlDevice*) rtp_malloc(sizeof(UPnPControlDevice));

	if (device)
	{
		device->controlPoint = cp;
		URL_Init(&device->baseURL);
		device->descDoc = 0;
		device->deviceElement = 0;
		DLLIST_INIT(&device->serviceList);
		device->refCount = 0;
	}

	return (device);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlDeleteDevice (
		UPnPControlDevice* device
	)
{
	UPnPControlService* service;

	UPNP_ASSERT(device->refCount == 0, "Deleting a device which is in use");

	while (device->serviceList.next != &device->serviceList)
	{
		service = (UPnPControlService*) device->serviceList.next;
		DLLIST_REMOVE(&service->node);
		_UPnP_ControlDeleteService(service);
	}

	URL_Free(&device->baseURL);
	if (device->descDoc)
	{
		rtpxmlDocument_free(device->descDoc);
	}

	rtp_free(device);
}

/*---------------------------------------------------------------------------*/
UPnPControlService* _UPnP_ControlNewService (
		UPnPControlPoint* cp,
		UPnPControlDevice* device,
		RTPXML_Element* serviceElement
	)
{
	UPnPControlService* service;

	service = (UPnPControlService*) rtp_malloc(sizeof(UPnPControlService));

	if (service)
	{
		DLLIST_INSERT_BEFORE(&device->serviceList, &service->node);

		service->device = device;
		service->refCount = 0;

	    /*--- From Device Description -----------------------*/
		service->scpdURL     = UPnP_DOMGetElemTextInElem (serviceElement, "SCPDURL", 0);
		service->controlURL  = UPnP_DOMGetElemTextInElem (serviceElement, "controlURL", 0);
		service->eventURL    = UPnP_DOMGetElemTextInElem (serviceElement, "eventSubURL", 0);
		service->serviceType = UPnP_DOMGetElemTextInElem (serviceElement, "serviceType", 0);
		service->serviceId   = UPnP_DOMGetElemTextInElem (serviceElement, "serviceId", 0);
		service->serviceElement = serviceElement;

		/* determine is the service contains the mandatory fields */
        if (service->scpdURL && service->controlURL &&
		    service->serviceType && service->serviceId)
		{
		   if(service->eventURL)
		   {
			    /*--- Eventing Related ------------------------------*/
				service->state = UPNP_SERVICE_NOT_SUBSCRIBED;
				service->subscriptionId = 0;
				service->subscriptionTimeoutSec = 0;
				service->eventCallbackPathId = 0;
				service->eventSequence = 0;
				service->expireWarningMsec = UPNP_CONTROL_SUB_EXPIRE_WARNING_MSEC;

				if (GENA_ClientInit (
						&service->genaListener,
						(GENAClientCallback)_UPnP_ControlEventGENACallback,
						service
					) >= 0)
				{
					return (service);
				}
			}
			else
			{
            	service->state = UPNP_SERVICE_CANNOT_SUBSCRIBE;
            	return(service);
            }
		}

        /* delete this node */
	    DLLIST_REMOVE(&service->node);
		rtp_free(service);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlDeleteService (
		UPnPControlService* service
	)
{
	UPNP_ASSERT(service->refCount == 0, "Deleting a service which is in use ");

	if (service->state != UPNP_SERVICE_NOT_SUBSCRIBED)
	{
		/* tbd - start unsubscribing from the device */
	}
	else
	{
		if (service->subscriptionId)
		{
			rtp_strfree(service->subscriptionId);
		}

		rtp_free(service);
	}
}

/*---------------------------------------------------------------------------*/
UPnPControlService* _UPnP_ControlDeviceGetService (
		UPnPControlDevice* device,
		const UPNP_CHAR* serviceId
	)
{
	UPnPControlService* service;

	service = (UPnPControlService*) device->serviceList.next;
	while (service != (UPnPControlService*) &device->serviceList)
	{
		if (serviceId == service->serviceId ||
		    !rtp_strcmp(serviceId, service->serviceId))
		{
			return (service);
		}

		service = (UPnPControlService*) service->node.next;
	}

	return (service);
}

/*---------------------------------------------------------------------------*/
void UPnP_ControlExecuteNSM (
		UPnPControlPoint* cp,
		void* v_sm,
		UPNP_INT32 maxTimeoutMsec
	)
{
	RTP_NET_SM* sm = (RTP_NET_SM*) v_sm;
	RTP_BOOL   done = 0;
	RTP_FD_SET readList;
	RTP_FD_SET writeList;
	RTP_FD_SET errList;
	RTP_INT32  timeoutMsec;
	RTP_INT32  timeLeftMsec;
	RTP_UINT32 timeStartedMsec = rtp_get_system_msec();

	UPNP_CLIENT_ENTER_API(cp);

	timeLeftMsec = (RTP_INT32) (rtp_get_system_msec() - timeStartedMsec);

	while (!done &&
	      (maxTimeoutMsec == RTP_TIMEOUT_INFINITE ||
	       timeLeftMsec < maxTimeoutMsec))
	{
		rtp_fd_zero(&readList);
		rtp_fd_zero(&writeList);
		rtp_fd_zero(&errList);

		timeoutMsec = rtp_net_sm_add_to_select_list(sm, &readList, &writeList, &errList);

		if (maxTimeoutMsec != RTP_TIMEOUT_INFINITE &&
		    timeoutMsec > timeLeftMsec)
		{
			timeoutMsec = timeLeftMsec;
		}

		UPNP_CLIENT_EXIT_API(cp);

		rtp_net_select(&readList, &writeList, &errList, timeoutMsec);

		UPNP_CLIENT_ENTER_API(cp);
		done = rtp_net_sm_process_state(sm, &readList, &writeList, &errList);

		timeLeftMsec = (RTP_INT32) (rtp_get_system_msec() - timeStartedMsec);
	}

	UPNP_CLIENT_EXIT_API(cp);
}
