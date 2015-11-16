//
// CONTROL_EVENT.H -
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

#ifndef __CONTROL_EVENT_H__
#define __CONTROL_EVENT_H__

typedef struct s_UPnPControlSubscription UPnPControlSubscription;
typedef struct s_UPnPControlResubscribe  UPnPControlResubscribe;

#include "rtpnetsm.h"
#include "upnpcli.h"
#include "genacli.h"

/* NOTE: must be larger for IPv6 */
#define UPNP_EVENT_CALLBACK_STR_LEN   64 

enum e_UPnPControlResubscribeState
{
	UPNP_CONTROL_UNSUBSCRIBING = 0,
	UPNP_CONTROL_SUBSCRIBING,
	UPNP_CONTROL_RESUBSCRIBED,
	UPNP_CONTROL_ERROR,
};

typedef enum e_UPnPControlResubscribeState UPnPControlResubscribeState;

struct s_UPnPControlSubscription
{
	RTP_NET_SM                  base;
	UPnPControlService*         service;
	void*                       userData;
	GENAClientRequest           genaRequest;
	UPNP_CHAR                   callbackURL[UPNP_EVENT_CALLBACK_STR_LEN];
	UPNP_BOOL                   generateEvent;
};

struct s_UPnPControlResubscribe
{
	RTP_NET_SM                  base;
	UPnPControlSubscription*    subSm;
	UPnPControlService*         service;
	UPnPControlResubscribeState state;
	UPNP_INT32                  newSubTimeoutSec;
};


#ifdef __cplusplus
extern "C" {
#endif

UPnPControlSubscription* UPnP_ControlNewSubscribeRequest (
		UPnPControlService* service,
		UPNP_INT32 timeoutSec,
		void* userData,
		UPNP_BOOL generateEvent
	);

UPnPControlSubscription* UPnP_ControlNewRenewRequest (
		UPnPControlService* service,
		UPNP_INT32 timeoutSec,
		void* userData,
		UPNP_BOOL generateEvent
	);

UPnPControlSubscription* UPnP_ControlNewUnsubscribeRequest (
		UPnPControlService* service,
		void* userData,
		UPNP_BOOL generateEvent
	);

UPnPControlResubscribe* UPnP_ControlNewResubscribeRequest (
		UPnPControlService* service,
		void* userData
	);

UPNP_INT32 UPnP_ControlGetEventCallbackPath (
		UPnPControlService* service,
		UPNP_CHAR* strOut,
		UPNP_INT32 bufferLen
	);

UPNP_INT32 UPnP_ControlGetEventCallbackURL (
		UPnPControlService* service,
		UPNP_CHAR* strOut,
		UPNP_INT32 bufferLen,
		RTP_NET_ADDR* remoteHost
	);

int _UPnP_ControlEventGENACallback (
		GENAClientContext* ctx,
		GENAClientEvent* eventMessage,
		void* callbackData
	);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_EVENT_H__ */
