//
// UPNPCLIMAN.H - 
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

#ifndef __UPNPCLIMAN_H__
#define __UPNPCLIMAN_H__

typedef struct s_UPnPControlManager UPnPControlManager;

#include "upnpclireq.h"

struct s_UPnPControlManager
{
	DLListNode requestList;
};

#ifdef __cplusplus
extern "C" {
#endif

int UPnP_ControlManagerInit (
		UPnPControlManager* manager
	);

void UPnP_ControlManagerDestroy (
		UPnPControlManager* manager
	);

UPNP_INT32 UPnP_ControlManagerAddToSelectList (
		UPnPControlManager* manager,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList		
	);

UPNP_BOOL UPnP_ControlManagerProcessState (
		UPnPControlManager* manager,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList		
	);
	
int UPnP_ControlManagerAddRequest (
		UPnPControlManager* manager,
		UPnPControlRequest* request
	);

#ifdef __cplusplus
}
#endif

#endif /* __UPNPCLIMAN_H__ */
