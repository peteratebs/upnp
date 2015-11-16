//
// UPNPCLI.H -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __UPNPCLI_H__
#define __UPNPCLI_H__

typedef struct s_UPnPControlPoint		    UPnPControlPoint;
typedef struct s_UPnPControlEvent           UPnPControlEvent;
typedef struct s_UPnPControlDevice          UPnPControlDevice;
typedef struct s_UPnPControlService         UPnPControlService;
typedef struct s_UPnPControlDeviceInfo      UPnPControlDeviceInfo;
typedef struct s_UPnPControlServiceIterator UPnPControlServiceIterator;

#include "upnp.h"
#include "httpmcli.h"
#include "upnpcliman.h"
#include "genacli.h"
#include "rtpnetsm.h"
#include "urlparse.h"

/*****************************************************************************/
typedef int (*UPnPControlPointCallback) (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void* userControlPointData,
		void* userRequestData
	);

struct s_UPnPControlPoint
{
	RTP_NET_AGGREGATE_SM     requestManager; /* MUST BE FIRST !!! */
	UPnPRuntime*             runtime;
	HTTPManagedClient        httpClient;
	SSDPCallback             controlSSDPCallback; // used to prevent binary dependency from UPnP common code to the control point SDK
	UPnPControlPointCallback callbackFn;
	void*                    callbackData;
};


// PASSIVE events are NOT the result of a locally initiated action
// ACTIVE events ARE  "   "    "     "  ...
enum e_UPnPControlEventType
{
	UPNP_CONTROL_EVENT_ACTION_COMPLETE = 0,           // (ACTIVE)  an action was invoked and response received from a device.
	                                                  //           could indicate success or failure
	UPNP_CONTROL_EVENT_DEVICE_ALIVE,                  // (PASSIVE) a device has come up on the network and advertised its presence
	UPNP_CONTROL_EVENT_DEVICE_BYE_BYE,                // (PASSIVE) a device that was available on the network is going down
	UPNP_CONTROL_EVENT_DEVICE_FOUND,                  // (ACTIVE)  a search was initiated by the control point and a response
	                                                  //           received from a device matching the search criteria
	UPNP_CONTROL_EVENT_DEVICE_OPEN,                   // (ACTIVE)  a device open operation was initiated and has now completed.
	                                                  //           this event indicates the open was successful
	UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED,            // (ACTIVE)  a device open operation was initiated and has now failed
	UPNP_CONTROL_EVENT_NONE,                          //           used internally - should never passed to a callback
	UPNP_CONTROL_EVENT_SEARCH_COMPLETE,               // (ACTIVE)  a search was initiated by the control point; the time interval
	                                                  //           specified for the search has now elapsed, indicating that the
	                                                  //           search is complete
	UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED,       // (ACTIVE)  a service get info operation was initiated by the control point;
	                                                  //           this event indicates the operation failed
	UPNP_CONTROL_EVENT_SERVICE_INFO_READ,             // (ACTIVE)  a service get info operation was initiated by the control point;
	                                                  //           this event indicates success - the SCPD xml document is now
	                                                  //           available to the application
	UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE,          // (PASSIVE) the state of a service that this control point is subscribed
	                                                  //           to has been updated
	UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED,         // (ACTIVE)  a subscribe operation was initiated by the control point; the
	                                                  //           subscription has now been successfully established, and state
	                                                  //           update events will now begin to arrive from the subscribed
	                                                  //           service
	UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED,        // (ACTIVE)  a subsciption cancel operation was initiated by the control
	                                                  //           point; it has now completed successfully
	UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED,          // (PASSIVE) the expiration timeout for a subscription has been reached and
	                                                  //           therefore the control point has automatically been unsubscribed
	UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED,            // (ACTIVE)  an out-of-sync event was generated and the callback indicated
	                                                  //           that it wanted to try to re-synchronize the subscription; this
	                                                  //           even indicates the re-syncrhonization has failed.  the
	                                                  //           subscription is no longer valid
	UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION,  // (PASSIVE) the expiration timeout for a subscription has almost been
	                                                  //           reached; this is the control point's opportunity to renew the
	                                                  //           subscription if it wants to
	UPNP_CONTROL_EVENT_SUBSCRIPTION_OUT_OF_SYNC,      // (PASSIVE) the sequence numbers for events from a subscribed service are
	                                                  //           out of order; control point must choose to a) ignore the error,
	                                                  //           b) cancel the subscription, or c) allow UPnP to attempt to
	                                                  //           re-synchronize
	UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED,         // (ACTIVE)  a subscribe operation was initiated by the control point; this
	                                                  //           event indicates the request failed
	UPNP_NUM_CONTROL_EVENT_TYPES
};
typedef enum   e_UPnPControlEventType       UPnPControlEventType;

struct s_UPnPControlDevice
{
	UPnPControlPoint*           controlPoint;         // used to tie into the control point's state machine and the upnp runtime's HTTP server (for capturing events)
	URLDescriptor               baseURL;              // used to resolve relative URL's
	RTPXML_Document*              descDoc;              // used for a bunch of stuff (duh)
	RTPXML_Element*               deviceElement;        // to get at the root device element quickly
	DLListNode                  serviceList;          // to keep track of service subscriptions & action URL's
	UPNP_INT32				    refCount;             // to know when it is safe to free this object
};

typedef UPnPControlDevice*  UPnPControlDeviceHandle;

// struct used to pass output from various API calls
//  all values are valid until UPnP_ControlCloseDevice is
//  called.  These values are READ-ONLY!  They MUST NOT
//  be modified by the application!
struct s_UPnPControlDeviceInfo
{
    UPNP_CHAR*                  deviceType;
  	UPNP_CHAR*                  friendlyName;
  	UPNP_CHAR*                  manufacturer;
  	UPNP_CHAR*                  manufacturerURL;
  	UPNP_CHAR*                  modelDescription;
	UPNP_CHAR*                  modelName;
	UPNP_CHAR*                  modelNumber;
	UPNP_CHAR*                  modelURL;
	UPNP_CHAR*                  serialNumber;
	UPNP_CHAR*                  UDN;
	UPNP_CHAR*                  UPC;
	UPNP_CHAR*                  presentationURL;
};

// used internally only - application need not worry about this enum
typedef enum e_UPnPControlServiceState
{
	UPNP_SERVICE_NOT_SUBSCRIBED = 0,
	UPNP_SERVICE_SUBSCRIBING,
	UPNP_SERVICE_SUBSCRIBED_WAITING_FIRST_EVENT,
	UPNP_SERVICE_SUBSCRIBED,
	UPNP_SERVICE_UNSUBSCRIBING,    /* only used when correcting out-of-sequence error */
	UPNP_SERVICE_CANNOT_SUBSCRIBE, /* used when service is not eventable */
	UPNP_NUM_SERVICE_STATES
}
UPnPControlServiceState;

struct s_UPnPControlService
{
	DLListNode                  node;            // for (this)->device->serviceList
	UPnPControlDevice*          device;          // to get at the parent device, control point, and upnp runtime
	UPNP_INT32				    refCount;        // to know when it is safe to free this object

    /*--- From Device Description -----------------------*/
    // these all point into the XML DOM tree, (this)->device->descDoc;
    //  they are stored here as shortcuts to speed some things up.
	UPNP_CHAR*                  scpdURL;
	UPNP_CHAR*                  controlURL;
	UPNP_CHAR*                  eventURL;
	UPNP_CHAR*                  serviceType;
	UPNP_CHAR*                  serviceId;
	RTPXML_Element*               serviceElement;

    /*--- Eventing Related ------------------------------*/
	UPnPControlServiceState     state;
	GENAClientContext           genaListener;
	UPNP_CHAR*                  subscriptionId;
	UPNP_UINT32                 subscriptionTimeoutSec;
	UPNP_INT32                  expireWarningMsec;
	UPNP_UINT32                 eventCallbackPathId; // the actual HTTP server path that catches events for this service is generated from this int.
	UPNP_UINT32                 eventSequence;       // to tell when the subscription has gotten out-of-sync (see UPNP_CONTROL_EVENT_SUBSCRIPTION_OUT_OF_SYNC above)
};

struct s_UPnPControlEvent
{
	UPnPControlEventType            type;

	union
	{
		// used by:
		//  UPNP_CONTROL_EVENT_DEVICE_ALIVE
		//  UPNP_CONTROL_EVENT_DEVICE_BYE_BYE
		//  UPNP_CONTROL_EVENT_DEVICE_FOUND
		struct
		{
			const UPNP_CHAR*        url;           // valid until event handler returns
			const UPNP_CHAR*        type;          // valid until event handler returns
			const UPNP_CHAR*        usn;           // valid until event handler returns
			SSDP_INT32              cacheTimeoutSec;// valid until event handler

		}
		discover;

		// used by:
		//  UPNP_CONTROL_EVENT_DEVICE_OPEN
		struct
		{
			UPnPControlDeviceHandle handle;      // valid until UPnP_ControlCloseDevice is called
			const UPNP_CHAR*        url;         // valid until event handler returns
		}
		device;

		// used by:
		//  UPNP_CONTROL_EVENT_SERVICE_INFO_READ
		//  UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED
		struct
		{
			UPnPControlDeviceHandle deviceHandle;
			const UPNP_CHAR*        id;
			RTPXML_Document*          scpdDoc;      // now owned by the application
		}
		service;

		// used by:
		//  UPNP_CONTROL_EVENT_ACTION_COMPLETE
		struct
		{
			UPNP_BOOL               success;
			RTPXML_Document*          response;         // valid until event handler returns
			UPNP_INT32              errorCode;
			UPNP_CHAR*              errorDescription; // valid until event handler returns
		}
		action;

		// used by:
		//  UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED
		//  UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED
		//  UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION
		//  UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED
		//  UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED
		//  UPNP_CONTROL_EVENT_SYNCHRONIZE_FAILED
		struct
		{
			UPnPControlDeviceHandle deviceHandle;  // valid until event handler returns
			const UPNP_CHAR*        serviceId;     // valid until event handler returns
			UPNP_INT32              timeoutSec;
		}
		subscription;

		// used by:
		//  UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE
		struct
		{
			RTPXML_Document*          stateVars;      // now owned by the event handler
			UPnPControlDeviceHandle deviceHandle;   // valid until event handler returns
			const UPNP_CHAR*        serviceId;      // valid until event handler returns
		}
		notify;
	}
	data;
};

// opaque structure; these members MUST NOT be accessed
//  by the application
struct s_UPnPControlServiceIterator
{
	UPnPControlDevice*  device;
	UPNP_INT32          n;
	UPNP_CHAR*          filterByType;
};

#ifdef __cplusplus
extern "C" {
#endif

// needs to change for event queue model: remove callback
int UPnP_ControlPointInit (
		UPnPControlPoint* cp,
		UPnPRuntime* rt,
		UPnPControlPointCallback callbackFn,
		void* callbackData
	);

// if gracefulTimeoutMsec is nonzero, then UPnP_ControlPointDestroy
//  will block if needed for at max gracefulTimeoutMsec waiting for
//  any outstanding asynchronous operations to complete (such as
//  subscription cancellations)
void UPnP_ControlPointDestroy (
		UPnPControlPoint* cp,
		UPNP_INT32 gracefulTimeoutMsec
	);

/*---------------------------------------------------------------------------*/
// Discovery

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
int UPnP_ControlFindAll (
		UPnPControlPoint* cp,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
int UPnP_ControlFindAllDevices (
		UPnPControlPoint* cp,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
int UPnP_ControlFindDevicesByType (
		UPnPControlPoint* cp,
		UPNP_CHAR* deviceType,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
int UPnP_ControlFindDevicesByUUID (
		UPnPControlPoint* cp,
		UPNP_CHAR* uuid,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_DEVICE_FOUND (url) */
int UPnP_ControlFindDevicesByService (
		UPnPControlPoint* cp,
		UPNP_CHAR* serviceType,
		UPNP_INT32 timeoutMsec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

/*---------------------------------------------------------------------------*/
// Description

/*-------------------------------------------------------------------*/
// device-related
UPnPControlDeviceHandle UPnP_ControlOpenDevice (
		UPnPControlPoint* cp,
		UPNP_CHAR* url
	);

/* generates: UPNP_CONTROL_EVENT_DEVICE_OPEN (DeviceHandle) */
int UPnP_ControlOpenDeviceAsync (
		UPnPControlPoint* cp,
		UPNP_CHAR* url,
		void* userData
	);

void UPnP_ControlCloseDevice (
		UPnPControlDeviceHandle deviceHandle
	);

int UPnP_ControlGetDeviceInfo (
		UPnPControlDeviceHandle handle,
		UPnPControlDeviceInfo* info
	);

/*-------------------------------------------------------------------*/
// service-related

int UPnP_ControlGetServices (
		UPnPControlDeviceHandle deviceHandle,
		UPnPControlServiceIterator* i
	);

int UPnP_ControlGetServicesByType (
		UPnPControlDeviceHandle deviceHandle,
		UPnPControlServiceIterator* i,
		UPNP_CHAR* serviceType
	);

UPNP_CHAR* UPnP_ControlNextService (
		UPnPControlServiceIterator* i
	);

void UPnP_ControlServiceIteratorDone (
		UPnPControlServiceIterator* i
	);

UPNP_CHAR* UPnP_ControlGetServiceType (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId
	);

int UPnP_ControlGetServiceOwnerDeviceInfo (
		UPnPControlDeviceHandle handle,
		UPNP_CHAR* serviceId,
		UPnPControlDeviceInfo* info
	);

RTPXML_Document* UPnP_ControlGetServiceInfo (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId
	);

/* generates: UPNP_CONTROL_EVENT_SERVICE_INFO_READ (xmlDoc: SCPD) */
int UPnP_ControlGetServiceInfoAsync  (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		void* userData
	);

/*---------------------------------------------------------------------------*/
// Control

// needs to change for event queue model: waitForCompletion is always FALSE
/* generates: UPNP_CONTROL_EVENT_ACTION_COMPLETE;
   returns:   0,
  */
int UPnP_ControlInvokeAction (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

/*---------------------------------------------------------------------------*/
// Eventing

// needs to change for event queue model: waitForCompletion is always FALSE
/*  generates: UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED,
               UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED */
//  0 = subscribed
// -1 = request failed: not subscribed
// -2 = request started: so far so good (not YET subscribed)
// -3 = request failed: still subscribed (renew failed)
int UPnP_ControlSubscribe (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		UPNP_INT32 timeoutSec,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

UPNP_BOOL UPnP_ControlSubscribedToService (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId
	);

void UPnP_ControlSetServiceSubscriptionExpireWarning (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		UPNP_INT32 warningMsec
	);

// needs to change for event queue model: waitForCompletion is always FALSE
/*  generates: UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED */
//  0 = unsubscribe succeeded
// -1 = unsubscribe request failed
// -2 = unsubscribe request started; so far so good
// -3 = not currently subscribed
// in all cases, but service is not listening for events when UPnP_ControlUnsubscribe returns
int UPnP_ControlUnsubscribe (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

#ifdef __cplusplus
}
#endif

#endif //__UPNPDEV_H__
