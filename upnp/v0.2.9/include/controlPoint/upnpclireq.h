//
// UPNPCLIREQ.H - 
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

#ifndef __UPNPCLIREQ_H__
#define __UPNPCLIREQ_H__

typedef struct s_UPnPControlRequestClass UPnPControlRequestClass;
typedef struct s_UPnPControlRequest      UPnPControlRequest;

#include "upnp.h"

struct s_UPnPControlRequestClass
{
	// return max timeout?
	UPNP_INT32 (*addToSelectList) (
			void* request,
			RTP_FD_SET* readList,
			RTP_FD_SET* writeList,
			RTP_FD_SET* errList		
		);
	
	// return true if completed
	UPNP_BOOL (*searchProcessState) (
			void* request,
			RTP_FD_SET* readList,
			RTP_FD_SET* writeList,
			RTP_FD_SET* errList		
		);

	// duh
	void (*destroy) (
			void* request
		);
};

struct s_UPnPControlRequest
{
	DLListNode               node;
	UPnPControlRequestClass* requestClass;
	UPnPControlPoint*        cp;
	void*                    userData;
};

#ifdef __cplusplus
extern "C" {
#endif

void UPnP_ControlRequestInit (
		UPnPControlRequest* request,
		UPnPControlRequestClass* requestClass,
		UPnPControlPoint* cp,
		void* userData
	);

UPNP_INT32 UPnP_ControlRequestAddToSelectList (
		UPnPControlRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList		
	);

UPNP_BOOL UPnP_ControlRequestProcessState (
		UPnPControlRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList		
	);

UPNP_BOOL UPnP_ControlRequestDestroy (
		UPnPControlRequest* request
	);

#ifdef __cplusplus
}
#endif

#endif /* __UPNPCLIREQ_H__ */
