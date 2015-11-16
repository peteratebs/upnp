/*
|  CONTROL_TIMER.C -
| 
|  EBS -
| 
|   $Author: vmalaiya $
|   $Date: 2006/11/29 18:53:08 $
|   $Name:  $
|   $Revision: 1.1 $
| 
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "controlTimer.h"
#include "rtptime.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

static UPNP_INT32 _msecRemaining (UPNP_UINT32 expirationMsec);
	
/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*---------------------------------------------------------------------------*/
int UPnP_ControlTimerNSMInit (
		UPnPControlTimerNSM* timerSm,
		UPNP_INT32 msecInterval,
		RTP_NET_SM_CALLBACK cb
	)
{
	if (rtp_net_sm_init(&timerSm->baseSm, cb) >= 0)
	{
		timerSm->expiresAtMsec = rtp_get_system_msec() + msecInterval;		
		return (0);
	}
	
	return (-1);
}

/*---------------------------------------------------------------------------*/
void UPnP_ControlTimerNSMCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlTimerNSM* timerSm = (UPnPControlTimerNSM*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			result->maxTimeoutMsec = _msecRemaining(timerSm->expiresAtMsec);
			if (result->maxTimeoutMsec < 0)
			{
				result->maxTimeoutMsec = 0;
			}
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:
			if (_msecRemaining(timerSm->expiresAtMsec) <= 0)
			{
				result->done = 1;
			}
			else
			{
				result->done = 0;
			}
			break;

		case RTP_NET_SM_EVENT_DESTROY:
			break;
			
		case RTP_NET_SM_EVENT_FREE:
			break;
	}	
}	

/*---------------------------------------------------------------------------*/
UPNP_INT32 _msecRemaining (UPNP_UINT32 expirationMsec)
{
	UPNP_UINT32 currentMsec = rtp_get_system_msec();
	
	return ( (UPNP_INT32) (expirationMsec - currentMsec) );
}
