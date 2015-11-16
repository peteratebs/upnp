/*
|  CONTROL_TIMER.H - 
| 
|  EBS - 
| 
|   $Author: vmalaiya $
|   $Date: 2006/11/29 18:53:07 $
|   $Name:  $
|   $Revision: 1.1 $
| 
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/ 

#ifndef __CONTROL_TIMER_H__
#define __CONTROL_TIMER_H__

#include "upnp.h"
#include "rtpnetsm.h"

typedef struct s_UPnPControlTimerNSM UPnPControlTimerNSM;

struct s_UPnPControlTimerNSM
{
	RTP_NET_SM  baseSm;
	UPNP_UINT32 expiresAtMsec;
};

#ifdef __cplusplus
extern "C" {
#endif

int UPnP_ControlTimerNSMInit (
		UPnPControlTimerNSM* timerSm,
		UPNP_INT32 msecInterval,
		RTP_NET_SM_CALLBACK cb
	);

void UPnP_ControlTimerNSMCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_TIMER_H__ */
