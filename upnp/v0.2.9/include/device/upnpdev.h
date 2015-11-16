//
// UPNPDEV.H -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __UPNPDEV_H__
#define __UPNPDEV_H__

typedef struct s_UPnPRootDevice*    UPnPRootDeviceHandle;
typedef struct s_UPnPDeviceRuntime  UPnPDeviceRuntime;
typedef struct s_UPnPRootDevice     UPnPRootDevice;

enum e_UPnPDeviceEventType
{
	UPNP_UNKNOWN_DEVICE_EVENT = -1,
	UPNP_DEVICE_EVENT_ACTION_REQUEST,
	UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST,
	UPNP_NUM_DEVICE_EVENT_TYPES
};
typedef enum e_UPnPDeviceEventType  UPnPDeviceEventType;

typedef int (*UPnPDeviceCallback) (
		void* userData,
		UPnPDeviceRuntime* deviceRuntime,
		UPnPRootDeviceHandle rootDevice,
		UPnPDeviceEventType eventType,
		void* eventStruct
	);

#include "upnp.h"
#include "soapsrv.h"
#include "genasrv.h"

/*****************************************************************************/
/* Server-side (Device) structures                                           */

struct s_UPnPDeviceRuntime
{
	DLListNode               rootDeviceList;
	UPnPRuntime*             upnpRuntime;
	UPNP_INT32               announceFrequencySec;
	UPNP_INT32               nextAnnounceTimeSec;
	UPNP_INT32               remoteCacheTimeoutSec;
	int                      (*announceAll) (UPnPDeviceRuntime *runtime);
	SSDPCallback             deviceSSDPCallback;

 #ifdef UPNP_MULTITHREAD
	RTP_MUTEX                mutex;
 #endif
};

typedef struct s_UPnPDevice
{
	DLListNode               node;
	UPnPRootDevice*          rootDevice;
	UPNP_CHAR*               deviceType;
	UPNP_CHAR*               UDN;
}
UPnPDevice;

struct s_UPnPRootDevice
{
	UPnPDevice               device;
	UPnPDeviceRuntime*       deviceRuntime;
	RTPXML_Document*           descDoc;
	UPNP_CHAR*               descLocation;
	DLListNode               serviceList;
	DLListNode               deviceList;
    UPNP_BOOL                sendingAnnouncements;
    UPNP_BOOL                autoAddr;
 #ifdef UPNP_MULTITHREAD
    RTP_MUTEX                mutex;
 #endif
	UPnPDeviceCallback       userCallback;
	void                    *userData;
};

typedef struct s_UPnPService
{
	DLListNode               node;
	UPnPDevice*              device;
	UPNP_CHAR*               serviceType;
	UPNP_CHAR*               serviceId;
	RTPXML_Element*            serviceElement;
	SOAPServerContext        soapContext;
	GENAServerContext        genaContext;
	GENAServerNotifier       genaNotifier;
 #ifdef UPNP_MULTITHREAD
	RTP_MUTEX                genaMutex;
 #endif
}
UPnPService;

/*****************************************************************************/
typedef struct s_UPnPSubscriptionRequest
{
	struct
	{
		GENAServerRequest* genaRequest;
		UPnPService*       service;
	}
	hidden;

	struct
	{
		RTP_NET_ADDR       clientAddr;
		const UPNP_CHAR*   serviceId;
		const UPNP_CHAR*   deviceUDN;
		const UPNP_CHAR*   subscriptionId;
		UPNP_UINT32        requestedTimeoutSec;
	}
	in;
}
UPnPSubscriptionRequest;

typedef struct s_UPnPActionRequest
{
	struct
	{
		RTP_NET_ADDR     clientAddr;
		const UPNP_CHAR* actionName;
		const UPNP_CHAR* serviceTypeURI;
		const UPNP_CHAR* serviceId;
		const UPNP_CHAR* deviceUDN;
		RTPXML_Element*    actionElement;
	}
	in;

	struct
	{
		int              errorCode;
		UPNP_CHAR        errorDescription[UPNP_MAX_ERROR_DESC_LEN];
		RTPXML_Document*   result;
	}
	out;
}
UPnPActionRequest;

typedef struct s_UPnPStateVarRequest
{
	struct
	{
		const UPNP_CHAR* name;
	}
	in;

	struct
	{
		UPNP_BOOL        found;
		UPNP_CHAR        value[UPNP_MAX_STATE_VAR_VALUE_LEN];
	}
	out;
}
UPnPStateVarRequest;

/*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int            UPnP_DeviceInit              (UPnPDeviceRuntime* deviceRuntime,
                                             UPnPRuntime* rt);

void           UPnP_DeviceFinish            (UPnPDeviceRuntime *deviceRuntime);

int            UPnP_RegisterRootDevice      (UPnPDeviceRuntime *deviceRuntime,
                                             const UPNP_CHAR *descDocURL,
                                             RTPXML_Document *description,
                                             UPNP_BOOL autoAddr,
                                             UPnPDeviceCallback callback,
                                             void *userData,
                                             UPnPRootDeviceHandle* rootDevice,
                                             UPNP_BOOL deviceAdvertise);

int            UPnP_UnRegisterRootDevice    (UPnPRootDeviceHandle rootDevice);

int            UPnP_DeviceAdvertise         (UPnPRootDeviceHandle rootDevice,
                                             UPNP_INT32 frequencySec,
                                             UPNP_INT32 remoteTimeoutSec);

/*****************************************************************************/
/* these functions are safe to call inside a device callback                 */



int UPnP_DeviceNotify (
		UPnPDeviceRuntime* deviceRuntime,
		UPnPRootDeviceHandle rootDevice,
		const UPNP_CHAR* deviceUDN,
        const UPNP_CHAR* serviceId,
		RTPXML_Document *vars
	);

int UPnP_DeviceNotifyAsync (
		UPnPDeviceRuntime* deviceRuntime,
		UPnPRootDeviceHandle rootDevice,
		const UPNP_CHAR* deviceUDN,
        const UPNP_CHAR* serviceId,
		RTPXML_Document *vars
	);

int UPnP_AcceptSubscription (
		UPnPSubscriptionRequest* subReq,
		const GENA_CHAR* subscriptionId,
		UPNP_INT32 timeoutSec,
		RTPXML_Document* propertySet,
		UPNP_INT32 firstNotifyDelayMsec
	);

int UPnP_AcceptSubscriptionAsync (
		UPnPSubscriptionRequest* subReq,
		const GENA_CHAR* subscriptionId,
		UPNP_INT32 timeoutSec,
		RTPXML_Document* propertySet,
		UPNP_INT32 firstNotifyDelayMsec
	);

int UPnP_CreateActionResponse (
		UPnPActionRequest* request
	);

const UPNP_CHAR* UPnP_GetArgValue (
		UPnPActionRequest* request,
		const UPNP_CHAR* argName
	);

int UPnP_SetActionResponseArg (
		UPnPActionRequest* request,
		const UPNP_CHAR *name,
		const UPNP_CHAR *value
	);

const UPNP_CHAR* UPnP_GetRequestedDeviceName (
		void *eventStruct,
		enum e_UPnPDeviceEventType eventType
	);

const UPNP_CHAR* UPnP_GetRequestedServiceId (
		void *eventStruct,
		enum e_UPnPDeviceEventType eventType
	);

const UPNP_CHAR* UPnP_GetRequestedActionName (
		void *eventStruct,
		enum e_UPnPDeviceEventType eventType
	);

const UPNP_CHAR* UPnP_GetArgValue (
		UPnPActionRequest* request,
		const UPNP_CHAR* argName
	);

void UPnP_SetActionErrorResponse (
		UPnPActionRequest* request,
		UPNP_CHAR *description,
		UPNP_INT32 value
	);



#ifdef __cplusplus
}
#endif

#endif
