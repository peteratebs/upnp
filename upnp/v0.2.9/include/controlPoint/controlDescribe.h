//
// CONTROL_DESCRIBE.H -
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

#ifndef __CONTROL_DESCRIBE_H__
#define __CONTROL_DESCRIBE_H__

typedef struct s_UPnPControlGetDocument    UPnPControlGetDocument;
typedef struct s_UPnPControlOpenDevice     UPnPControlOpenDevice;
typedef struct s_UPnPControlGetServiceInfo UPnPControlGetServiceInfo;

#include "rtpnetsm.h"
#include "httpmcli.h"
#include "urlparse.h"
#include "upnpcli.h"

typedef enum e_UPnPControlHTTPState
{
	UPNP_CONTROL_HTTP_STATE_CONNECTING,
	UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE,
	UPNP_CONTROL_HTTP_STATE_READING_DATA,
	UPNP_CONTROL_HTTP_STATE_DONE,
	UPNP_CONTROL_HTTP_STATE_ERROR,
	UPNP_CONTROL_NUM_HTTP_STATES,
}
UPnPControlHTTPState;

struct s_UPnPControlGetDocument
{
	RTP_NET_SM                base;
	UPnPControlHTTPState      state;
	HTTPManagedClientSession* httpSession;
	URLDescriptor             urlDesc;
	UPNP_UINT8*               docBuffer;
	UPNP_INT32                docBufferSize;
	UPNP_INT32                docBytesRead;
	FileContentType           contentType;
};

struct s_UPnPControlOpenDevice
{
	UPnPControlGetDocument    base;
	UPnPControlDevice*        device;
	void*                     userData;
	UPNP_BOOL                 generateEvent;
};

struct s_UPnPControlGetServiceInfo
{
	UPnPControlGetDocument    base;
	UPnPControlService*       service;
	RTPXML_Document*            scpdDoc;
	void*                     userData;
	UPNP_BOOL                 generateEvent;
};

#ifdef __cplusplus
extern "C" {
#endif

UPnPControlOpenDevice* UPnP_ControlNewOpenDevice (
		UPnPControlPoint* cp,
		UPnPControlDevice* device,
		UPNP_CHAR* url,
		void* userData,
		UPNP_BOOL generateEvent
	);

UPnPControlGetServiceInfo* UPnP_ControlNewGetServiceInfo (
		UPnPControlPoint* cp,
		UPnPControlService* service,
		void* userData,
		UPNP_BOOL generateEvent
	);

void _UPnP_ControlDeviceAddReference (
		UPnPControlDevice* device
	);

void _UPnP_ControlDeviceRemoveReference (
		UPnPControlDevice* device
	);

void _UPnP_ControlServiceAddReference (
		UPnPControlService* service
	);

void _UPnP_ControlServiceRemoveReference (
		UPnPControlService* service
	);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_DESCRIBE_H__ */
