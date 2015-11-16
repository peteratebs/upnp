//
// CONTROL_DISCOVER.H -
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

#ifndef __CONTROL_DISCOVER_H__
#define __CONTROL_DISCOVER_H__

#include "rtpnetsm.h"
#include "ssdpcli.h"
#include "upnpcli.h"

struct s_UPnPControlMSearch
{
	RTP_NET_SM        base;
	UPnPControlPoint* controlPoint;
	void*             userData;
	SSDPClientSearch  search;
};

typedef struct s_UPnPControlMSearch UPnPControlMSearch;

#ifdef __cplusplus
extern "C" {
#endif

UPnPControlMSearch* UPnP_ControlMSearchNew (
		UPnPControlPoint* cp,
		UPNP_CHAR* searchType,
		UPNP_INT32 maxResponseTimeoutSec,
		void* userData
	);

int UPnP_ControlSSDPCallback (
		SSDPServerContext* ctx,
		SSDPServerRequest* serverRequest,
		void* cookie
	);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_DISCOVER_H__ */
