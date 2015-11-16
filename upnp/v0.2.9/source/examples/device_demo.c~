//
// DEVDEMO.C - Device-side UPnP Demo
//
// EBS -
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
/*---------------------------------------------------------------------------*/
//#include "stdio.h"
//#include "conio.h"
#include "upnpdev.h"
#include "rtpnet.h"
#include "rtpthrd.h"
#include "rtpmemdb.h"
#include "rtpstr.h"
#include "rtpterm.h"
#include "rtpprint.h"
#include "rtpscnv.h"
#include "rtplog.h"

#define ANNOUNCE_FREQUENCY_SEC    10
#define REMOTE_CACHE_TIMEOUT_SEC  1800

#if defined(_WIN32) || defined(_WIN64)
#   define ROOTPATH "www-root\\"
#   define DEVICEFILE "www-root\\device.xml"
#elif defined(__linux)
#   define ROOTPATH "www-root/"
#   define DEVICEFILE "www-root/device.xml"
#else
#   error "FIX THE ROOT PATH FOR THIS OS"
#endif

/*---------------------------------------------------------------------------*/
UPnPDeviceRuntime deviceRuntime;

int testDeviceCallback (
		void *userData,
		struct s_UPnPDeviceRuntime *deviceRuntime,
		UPnPRootDeviceHandle rootDevice,
		enum e_UPnPDeviceEventType eventType,
		void *eventStruct);

RTP_MUTEX lightMutex;
int lightStatus = 0;
int levelStatus = 50; /* default light level */
#define MINLEVEL  0
#define MAXLEVEL  100
/*---------------------------------------------------------------------------*/
static int _device_demo_main (void);
int device_demo_main (void)
{
	if (_device_demo_main() < 0)
		rtp_printf("Initialization failed\n");
	return 0;
}

static int _device_demo_main (void)
{
	int count = 0;
	int result;
	RTPXML_Document *xmlDevice;
	UPnPRuntime rt;
	UPnPRootDeviceHandle rootDevice;
    //	unsigned char ipAddr[RTP_NET_NI_MAXHOST] = {192, 168, 0, 5};
    //unsigned char ipAddr[RTP_NET_NI_MAXHOST] = "fe80::20b:dbff:fe2f:c162";

	rtp_net_init();

	//rtp_threads_init();

	if (rtp_sig_mutex_alloc(&lightMutex, "light bulb") < 0)
	{
		return (-1);
	}

	//rtp_printf("Initializing UPnP using ip address %d.%d.%d.%d. recompile if this is incorrect.\n", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);

	rtp_printf("Initializing UPnP device runtime...\n", count);

	result = UPnP_RuntimeInit (
			  &rt,
			    0,    // default
			 5010,    // serverPort: any port
            RTP_NET_TYPE_IPV4,    // ip version type
		     ROOTPATH,    // web server root dir
			   10,    // maxConnections
			    5     // maxHelperThreads (not zero)
		);

	if (result < 0)
	{
		return (-1);
	}

	result = UPnP_DeviceInit (
				&deviceRuntime,
				&rt
			);

	if (result < 0)
	{
		return (-1);
	}

	/* If we want to watch the data flow - see functiona at the end of the file  */
	RTP_LOG_OPEN()

	rtp_printf("Locating device description...\n", count);
	xmlDevice = rtpxmlLoadDocument(DEVICEFILE);


	if (!xmlDevice)
	{
		return (-1);
	}

	rtp_printf("Registering device...\n", count);

	result = UPnP_RegisterRootDevice (
				&deviceRuntime,
				"device.xml",
				xmlDevice,
				1, /* auto address resolution */
				testDeviceCallback,
				0, /* userData for callback */
				&rootDevice,
				1 /* advertise */);

	if (result < 0)
	{
		/* tbd - clean up the device runtime */
		return (-1);
	}
	UPnP_DeviceAdvertise(rootDevice, ANNOUNCE_FREQUENCY_SEC,
						 REMOTE_CACHE_TIMEOUT_SEC);

	rtp_printf("Starting device server daemon...\n");

	/* start the UPnP daemon */
	UPnP_StartDaemon(&rt);
/*
	while (1)
	{
        UPnP_ProcessState (&rt,1000);
		rtp_printf(".");
	}
*/
	rtp_printf("Press return to quit...\n");
	rtp_term_getch();


	UPnP_DeviceFinish(&deviceRuntime);

	rtpxmlDocument_free(xmlDevice);

	rtp_sig_mutex_free(lightMutex);
	//rtp_threads_shutdown();
	rtp_net_exit();
	RTP_LOG_CLOSE()

	return (0);
}

int testDeviceCallback (
		void *userData,
		struct s_UPnPDeviceRuntime *deviceRuntime,
		UPnPRootDeviceHandle rootDevice,
		enum e_UPnPDeviceEventType eventType,
		void *eventStruct)
{
	const UPNP_CHAR* targetDeviceName;
	const UPNP_CHAR* targetServiceId;

	/* extracty the UDN for the request */
	targetDeviceName = UPnP_GetRequestedDeviceName (eventStruct, eventType);

	/* extract the serviceId for a particular service on the device */
	targetServiceId = UPnP_GetRequestedServiceId (eventStruct, eventType);

	/* if the callback is invoked to handle action request or a subscription request */
	switch (eventType)
	{
		case UPNP_DEVICE_EVENT_ACTION_REQUEST:
		{
			const UPNP_CHAR* targetActionName;
			/* To handle action requests, the cookie (eventStruct) is to be cast to be
			   of UPnPActionRequest type */
			UPnPActionRequest *request = (UPnPActionRequest *) eventStruct;

			/* request structure holds the all the action request information */
			/* start by determining if the action request is meant for this device
			   by compairing the unique device name supplied in the request with the
			   UDN of this device */

			if (!rtp_strcmp(targetDeviceName, "uuid:9de82eea-b4a2-41ae-b182-058befd73af8"))
			{
				/* if the action request is intended for this device, check to see which service on this
				   device the action will be performed upon */

				/* extract the actionName for a particular service on the device */
				targetActionName = UPnP_GetRequestedActionName (eventStruct, eventType);

				/* SERVICE : Switch Power */
				if (!rtp_strcmp(targetServiceId, "urn:upnp-org:serviceId:SwitchPower.0001"))
				{
					/* a service may offer multiple actions, next step is to determine the
					   target action */
					if (!rtp_strcmp(targetActionName, "GetStatus"))
					{
						/* create a response to acknowledge the request */
						if(UPnP_CreateActionResponse(request) >=0 )
						{
							UPNP_CHAR temp[5];

							rtp_sig_mutex_claim(lightMutex);
							/* get the value of lightStatus into temp variable*/
							rtp_itoa(lightStatus, temp, 10);
							rtp_sig_mutex_release(lightMutex);

							/* Since this action is a out action, this action is invoked by the  */
							/* control point to query the value of state variable */

							/* For such actions with direction of variables as out, the response needs */
							/* to contain the name and current value of action's out variable. */
							if(UPnP_SetActionResponseArg(request, "ResultStatus", temp) < 0)
							{
								return(-1);
							}
						}
						else
						{
							return (-1); /* create action response failed */
						}

					}
					else if (!rtp_strcmp(targetActionName, "SetTarget"))
					{
						/* If the action contains argument having direction 'in', this means that control point */
						/* will send an action request with a new value of argument that will replace the current */
						/* value of the argument */
						/* The following step extracts the value of the argument if supplied */
						const UPNP_CHAR *newTargetValue = UPnP_GetArgValue(request, "newTargetValue");
						if (newTargetValue)
						{
							int i = rtp_atoi(newTargetValue);
							int changed = 0;

							if (i)
							{
								rtp_printf("Light turned on.\n");
							}
							else
							{
								rtp_printf("Light turned off.\n");
							}

							rtp_sig_mutex_claim(lightMutex);

							if (i != lightStatus)
							{
								changed = 1;
							}

							lightStatus = i;

							rtp_sig_mutex_release(lightMutex);
                            /* if this actions causes the value of a state variable to change */
                            /* a notification to all the subscribed devices will be send */
							if (changed)
							{
								RTPXML_Document *propertySet = 0; /* must be initialized to zero */
								UPNP_CHAR temp[15];

								rtp_itoa(i, temp, 10);

								/* add name and vaule of the changed variable to the property set */
								/* this property set is sent to the subscribers as the event */
								/* notification */
								UPnP_AddToPropertySet(&propertySet, "Status", temp);

								/* send all the subscribers a notification of change of value event */
								UPnP_DeviceNotifyAsync(
										deviceRuntime, rootDevice,
										targetDeviceName,
										targetServiceId,
										propertySet);

								rtpxmlDocument_free(propertySet);
							}
						}

						/* create a response to acknowledge the request */
						if(UPnP_CreateActionResponse(request) < 0 )
						{
							return(-1);
						}
					}
					/* unknown action name */
					else
					{
					    UPnP_SetActionErrorResponse(request, "Invalid Action", 401);
					}
				}
				else
				/* SERVICE : Dimming Service */
				if (!rtp_strcmp(targetServiceId, "urn:upnp-org:serviceId:DimmingService.0001"))
				{
					if (!rtp_strcmp(targetActionName, "GetLoadLevelStatus"))
					{
						/* create a response to acknowledge the request */
						if(UPnP_CreateActionResponse(request) >=0 )
						{
							UPNP_CHAR temp[15];

							rtp_sig_mutex_claim(lightMutex);
							/* get the value of lightStatus into temp variable*/
							rtp_itoa(levelStatus, temp, 10);
							rtp_sig_mutex_release(lightMutex);

							/* Since this action is a out action, this action is invoked by the  */
							/* control point to query the value of state variable */

							/* For such actions with direction of variables as out, the response needs */
							/* to contain the name and current value of action's out variable. */
							UPnP_SetActionResponseArg(request, "RetLoadLevelStatus", temp);
						}
					}

					else if (!rtp_strcmp(targetActionName, "GetMinLevel"))
					{
						/* create a response to acknowledge the request */
						if(UPnP_CreateActionResponse(request) >=0 )
						{
							/* Since this action is a out action, this action is invoked by the  */
							/* control point to query the value of state variable */

							/* For such actions with direction of variables as out, the response needs */
							/* to contain the name and current value of action's out variable. */
							UPnP_SetActionResponseArg(request, "MinLevel", "0");
						}

					}

					else if (!rtp_strcmp(targetActionName, "SetLoadLevelTarget"))
					{

						/* If the action contains argument having direction 'in', this means that control point */
						/* will send an action request with a new value of argument that will replace the current */
						/* value of the argument */
						/* The following step extracts the value of the argument if supplied */
						const UPNP_CHAR *newTargetValue = UPnP_GetArgValue(request, "NewLoadLevelTarget");
						if (newTargetValue)
						{
							int i = rtp_atoi(newTargetValue);
							int changed = 0;

							if(i < MINLEVEL || i > MAXLEVEL)
							{
							    rtp_printf("Error: New lightLevel value out of range\n");
							    UPnP_SetActionErrorResponse(request, "Invalid Action", 402);
							    break;
						    }

					        rtp_printf("New Light Level set to :%d\n", i);

							rtp_sig_mutex_claim(lightMutex);

							if (i != levelStatus)
							{
								changed = 1;
							}

                            levelStatus = i;

							rtp_sig_mutex_release(lightMutex);

                            /* if this actions causes the value of a state variable to change */
                            /* a notification to all the subscribed devices will be send */
							if (changed)
							{
								RTPXML_Document *propertySet = 0; /* must be initialized to zero */
								UPNP_CHAR temp[5];

								rtp_itoa(i, temp, 10);

								/* add name and vaule of the changed variable to the property set */
								/* this property set is sent to the subscribers as the event */
								/* notification */
								UPnP_AddToPropertySet(&propertySet, "LoadLevelStatus", temp);

								/* send all the subscribers a notification of change of value event */
								UPnP_DeviceNotifyAsync(
										deviceRuntime, rootDevice,
										targetDeviceName,
										targetServiceId,
										propertySet);

								rtpxmlDocument_free(propertySet);
							}
						}

						/* create a response to acknowledge the request */
						if(UPnP_CreateActionResponse(request) < 0 )
						{
							return(-1);
						}
					}
					/* unknown action name */
					else
					{
					    UPnP_SetActionErrorResponse(request, "Invalid Action", 401);
					}
				}
			}

			break;
		}

		case UPNP_DEVICE_EVENT_SUBSCRIPTION_REQUEST:
		{
			UPnPSubscriptionRequest *request = (UPnPSubscriptionRequest *) eventStruct;

			if (!rtp_strcmp(targetDeviceName, "uuid:9de82eea-b4a2-41ae-b182-058befd73af8"))
			{
				/* SERVICE : Switch Power */
				if (!rtp_strcmp(targetServiceId, "urn:upnp-org:serviceId:SwitchPower.0001"))
				{
					RTPXML_Document *propertySet = 0; /* must be initialized to zero */
					UPNP_CHAR temp[5];

					rtp_sig_mutex_claim(lightMutex);
					rtp_itoa(lightStatus, temp, 10);
					rtp_sig_mutex_release(lightMutex);

					UPnP_AddToPropertySet(&propertySet, "Status", temp);
					UPnP_AcceptSubscription(request, 0, 0, propertySet, 100);
					rtpxmlDocument_free(propertySet);
				}
				else
				/* SERVICE : Dimming Service */
				if (!rtp_strcmp(targetServiceId, "urn:upnp-org:serviceId:DimmingService.0001"))
				{
					RTPXML_Document *propertySet = 0; /* must be initialized to zero */
					UPNP_CHAR temp[5];

					rtp_sig_mutex_claim(lightMutex);
					rtp_itoa(levelStatus, temp, 10);
					rtp_sig_mutex_release(lightMutex);

					UPnP_AddToPropertySet(&propertySet, "LoadLevelStatus", temp);
					UPnP_AcceptSubscription(request, 0, 0, propertySet, 100);
					rtpxmlDocument_free(propertySet);
				}
			}

			break;
		}
	}

	return (0);
}
