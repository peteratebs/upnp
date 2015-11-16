//
// CONTROL_ACTION.H -
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

#ifndef __CONTROL_ACTION_H__
#define __CONTROL_ACTION_H__

#include "rtpnetsm.h"
#include "upnpcli.h"
#include "soapcli.h"

struct s_UPnPControlInvokeAction
{
	RTP_NET_SM        base;
	UPnPControlPoint* controlPoint;
	void*             userData;
	SOAPAction        soapCtx;
	UPNP_CHAR*        bodyStr;
	UPNP_INT32        bodyLen;
};

typedef struct s_UPnPControlInvokeAction UPnPControlInvokeAction;

#ifdef __cplusplus
extern "C" {
#endif

UPnPControlInvokeAction* UPnP_ControlNewInvokeAction (
		UPnPControlService* service,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData
	);

UPnPControlInvokeAction* UPnP_ControlNewQueryStateVar (
		UPnPControlService* service,
		const UPNP_CHAR* varName,
		void* userData
	);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_ACTION_H__ */
