/*
|  MAIN.C -
|
|  EBS -
|
|   $Author: vmalaiya $
|   $Date: 2006/07/17 19:12:25 $
|   $Name:  $
|   $Revision: 1.3 $
|
|  Copyright EBS Inc. , 2005
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "rtpnet.h"
#include "rtpscnv.h"
#include "ssdpcli.h"
//#include "stdio.h"
#include "upnpcli.h"
#include "rtptime.h"
#include "rtpthrd.h"
#include "rtpprint.h"
#include "rtpterm.h"
//#include <conio.h>

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/
int testCallback (
		void* callbackData,
		SSDPSearchResponse* response
	);

int controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	);

void discoverTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void passiveDiscoverTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void deviceOpenTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void deviceOpenTestAsync (UPnPRuntime *rt, UPnPControlPoint *cp);
void controlTest2 (UPnPRuntime *rt, UPnPControlPoint *cp);
void controlTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void genaTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void serviceDescribeTest (UPnPRuntime *rt, UPnPControlPoint *cp);

/*****************************************************************************/
/* Data
 *****************************************************************************/
int numFound = 0;

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/
int control_point_demo_main (int* argc, char* agrv[])
{
	int result;
	UPnPRuntime rt;
	UPnPControlPoint cp;

    rtp_net_init();
	rtp_threads_init();

	result = UPnP_RuntimeInit (
			&rt,
			0,                // serverAddr: IP_ANY_ADDR
			0,                // serverPort: any port
            RTP_NET_TYPE_IPV4,// ip version: ipv4
			"www-root\\",     // web server root dir
			10,               // maxConnections
			5                 // maxHelperThreads
		);

	if (result >= 0)
	{
		/* Initialize the control point */
        if (UPnP_ControlPointInit(&cp, &rt, controlPointCallback, 0) >= 0)
		{

            	discoverTest (&rt, &cp);
            /* listen passively for any announcements. does not actively search for any device. */
	        //  passiveDiscoverTest (&rt, &cp);
            //	deviceOpenTest (&rt, &cp);
            //  deviceOpenTestAsync (&rt, &cp);
            //  controlTest (&rt, &cp);
            //	controlTest2 (&rt, &cp);
            //	genaTest (&rt, &cp);
            //	serviceDescribeTest (&rt, &cp);
 			UPnP_ControlPointDestroy (&cp, 0);
		}

		UPnP_RuntimeDestroy(&rt);
	}
    rtp_printf("hit any key to exit this demo..\n");
	rtp_term_getch();
	rtp_threads_shutdown();
	rtp_net_exit();
	return(0);
}

/*---------------------------------------------------------------------------*/
void discoverTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	RTP_UINT32 startTime;

	rtp_printf("Searching for all...\n\n");
	UPnP_ControlFindAll(cp, 5000, 0, 1);

    rtp_printf("\nSearching for all UPnP devices...\n\n");
	UPnP_ControlFindAllDevices(cp, 5000, 0, 1);

	rtp_printf("\nSearching for devices of type InternetGatewayDevice:1...\n\n");
	UPnP_ControlFindDevicesByType(cp, "InternetGatewayDevice:1", 5000, 0, 1);

	rtp_printf("\nSearching for services of type SwitchPower:1...\n\n");
	UPnP_ControlFindDevicesByService(cp, "SwitchPower:1", 5000, 0, 1);

	rtp_printf("\nSearching for devices with uuid: upnp-WANDevice-1_0-0090a2777777...\n\n");
	UPnP_ControlFindDevicesByUUID(cp, "upnp-WANDevice-1_0-0090a2777777", 5000, 0, 1);

	rtp_printf("\nSearching Asynchronously for all...\n\n");
//	UPnP_ControlFindAll(cp, 5000, &done, 0);


    startTime = rtp_get_system_msec();

	while (rtp_get_system_msec() - startTime < 5000)
	{
		rtp_printf("(time: %dms)\n", rtp_get_system_msec() - startTime);
		UPnP_ProcessState(rt, 100);
	}
}

/*---------------------------------------------------------------------------*/
void passiveDiscoverTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	RTP_UINT32 startTime;

	rtp_printf ("Listening passively for device announcements.");

	startTime = rtp_get_system_msec();

	while (rtp_get_system_msec() - startTime < 60000)
	{
		rtp_printf(".");
		UPnP_ProcessState(rt, 100);
	}
}

/*---------------------------------------------------------------------------*/
void deviceOpenTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	UPnPControlDeviceHandle dh;
	/* Opening an IPv6 device */
	/*
    rtp_printf("Opening device at http://[fe80::20b:dbff:fe2f:c162]...");
    dh = UPnP_ControlOpenDevice (cp, "http://[fe80::20b:dbff:fe2f:c162]:4087/device.xml");
    */
    /* Opening an IPv4 device */
    rtp_printf("Opening device at http://192.168.0.6...");
    dh = UPnP_ControlOpenDevice (cp, "http://192.168.0.6:4087/device.xml");
	if (dh != 0)
	{
		UPnPControlDeviceInfo info;
		UPnPControlServiceIterator i;

		UPnP_ControlGetDeviceInfo(dh, &info);

		rtp_printf (" success!\n\n"
				"  deviceType:       %.60s\n"
				"  friendlyName:     %.60s\n"
				"  manufacturer:     %.60s\n"
				"  manufacturerURL:  %.60s\n"
				"  modelDescription: %.60s\n"
				"  modelName:        %.60s\n"
				"  modelNumber:      %.60s\n"
				"  modelURL:         %.60s\n"
				"  serialNumber:     %.60s\n"
				"  UDN:              %.60s\n"
				"  UPC:              %.60s\n"
				"  presentationURL:  %.60s\n\n",
				info.deviceType,
				info.friendlyName,
	 			info.manufacturer,
				info.manufacturerURL,
				info.modelDescription,
				info.modelName,
				info.modelNumber,
				info.modelURL,
				info.serialNumber,
				info.UDN,
				info.UPC,
				info.presentationURL);

		if (UPnP_ControlGetServices(dh, &i) >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			while (serviceId)
			{
				rtp_printf ("Found Service: %.60s\n", serviceId);
				rtp_printf ("   (type: %.65s)\n", UPnP_ControlGetServiceType(dh, serviceId));
				serviceId = UPnP_ControlNextService(&i);
			}

			UPnP_ControlServiceIteratorDone(&i);
		}

		UPnP_ControlCloseDevice(dh);
	}
	else
	{
		rtp_printf (" open failed.\n");
	}

	rtp_printf("\n");
    genaTest (rt, cp);
}

/*---------------------------------------------------------------------------*/
void deviceOpenTestAsync (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	int done = 0;
    rtp_printf("Asynchronously Opening device at http://192.168.0.6:4754/...");
    UPnP_ControlOpenDeviceAsync(cp, "http://192.168.0.6:4754/device.xml", &done);

	while (!done)
	{
		rtp_printf(".");
		UPnP_ProcessState(rt, 100);
	}
}

/*---------------------------------------------------------------------------*/
void controlTest2(UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
	char* deviceUrl = 0;
    int done = -1;

	rtp_printf("\nSearching for services of type DimmingService:1...\n");
	UPnP_ControlFindDevicesByService(cp, "DimmingService:1", 2500, &deviceUrl, 1);

	if (deviceUrl)
	{
		rtp_printf("\nOpening device at %s...", deviceUrl);
		dh = UPnP_ControlOpenDevice(cp, deviceUrl);
		if (dh != 0)
		{
			UPnPControlServiceIterator i;

			rtp_printf("device open.\n\nLooking for DimmingService:1 service...");

			/* find the DimmingService:1 service and invoke an action on it */
			if (UPnP_ControlGetServicesByType(dh, &i, "DimmingService:1") >= 0)
			{
				UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

				if (serviceId)
				{
                    RTPXML_Document* action;
                    /* Action 'GetMinLevel' on DimmingService:1 does not
                       contain any 'in' arguments. Invoking 'GetMinLevel' action
                       in order to get the value of MinLevel ('out') variable */
                    action = UPnP_CreateAction(
                        UPnP_ControlGetServiceType(dh,serviceId),"GetMinLevel");
                    if (action)
					{
						/* Since action 'GetMinLevel' does not contain any 'in'
                           argument, there is no need to set any argument value in
                           this action request
                           i.e. no need for UPnP_SetActionArg ( )*/

                        if (UPnP_ControlInvokeAction (dh, serviceId, "GetMinLevel", action, &done, 1) >= 0)
						{
							while (done < 0)
	                        {
		                        UPnP_ProcessState(rt, 200);
	                        }
                            rtp_printf ("\nControl Test PASSED.\n");
						}
                        /* free the xml document */
                        rtpxmlDocument_free (action);
					}
					else
					{
						rtp_printf (" could not create action!\n");
					}
                }
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void controlTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	UPnPControlDeviceHandle dh;
	char* deviceUrl = 0;
    int done = -1;

	rtp_printf("\nSearching for services of type SwitchPower:1...\n");
	UPnP_ControlFindDevicesByService(cp, "SwitchPower:1", 2500, &deviceUrl, 1);

	if (deviceUrl)
	{
		rtp_printf("\nOpening device at %s...", deviceUrl);
		dh = UPnP_ControlOpenDevice(cp, deviceUrl);
		if (dh != 0)
		{
			UPnPControlServiceIterator i;

			rtp_printf("device open.\n\nLooking for SwitchPower:1 service...");

			/* find the SwitchPower:1 service and invoke an action on it */
			if (UPnP_ControlGetServicesByType(dh, &i, "SwitchPower:1") >= 0)
			{
				UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

				if (serviceId)
				{
					RTPXML_Document* action;

					rtp_printf("\n    Found (serviceId: %.40s)\n\nInvoking action: SetTarget(true)...", serviceId);

					action = UPnP_CreateAction(UPnP_ControlGetServiceType(dh, serviceId), "SetTarget");

					if (action)
					{
                        UPnP_SetActionArg (action, "newTargetValue", "1");
						if (UPnP_ControlInvokeAction (dh, serviceId, "SetTarget", action, &done, 1) >= 0)
						{
							while (done < 0)
	                        {
		                        UPnP_ProcessState(rt, 200);
	                        }
                            rtp_printf ("\nControl Test PASSED.\n");
						}
                        /* free the xml document */
                        rtpxmlDocument_free (action);
					}
					else
					{
						rtp_printf (" could not create action!\n");
					}
				}
				else
				{
					rtp_printf (" service not found!\n");
				}

				UPnP_ControlServiceIteratorDone(&i);
			}
			else
			{
				rtp_printf (" search init failed!\n");
			}

			UPnP_ControlCloseDevice(dh);
		}
		else
		{
			rtp_printf (" open failed.\n");
		}

		rtp_strfree(deviceUrl);
	}
}

/*---------------------------------------------------------------------------*/
void genaTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	int done = 0;
	UPnPControlDeviceHandle dh;
	char* deviceUrl = 0;
    rtp_printf("\nSearching for services of type SwitchPower:1...\n");

    UPnP_ControlFindDevicesByService(cp, "SwitchPower:1", 2500, &deviceUrl, 1);
	if (deviceUrl)
	{
		rtp_printf("\nOpening device at %s...", deviceUrl);
		dh = UPnP_ControlOpenDevice(cp, deviceUrl);
		if (dh != 0)
		{
			UPnPControlServiceIterator i;

			rtp_printf("device open.\n\nLooking for SwitchPower:1 service...");

			/* find the tvcontrol:1 service and subscribe it */
			if (UPnP_ControlGetServicesByType(dh, &i, "SwitchPower:1") >= 0)
			{
				UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

				if (serviceId)
				{
					rtp_printf("found.\n\nSubscribing to %s...\n\n", serviceId);
					if (UPnP_ControlSubscribe (
							dh,
							serviceId,
							//500,
                            0,
							0,
							0
						) >=0)
					{
						while(!done)
						{
							rtp_printf("\t hit u to unsubscribe\n\n");
							rtp_printf("\t hit s to subscribe\n\n");
							rtp_printf("\t hit x to exit\n\n");
							while (!rtp_term_kbhit())
							{
								UPnP_ProcessState(rt, 100);
							}
							switch(rtp_term_getch())
							{
								case 85:
								case 117:
									/* unsubscribe test */
									rtp_printf("processing Unsubscription .......\n\n");
									if (UPnP_ControlUnsubscribe (
											dh,
											serviceId,
											0,
											0
										) >=0)
									{
										rtp_printf ("unsubscribed successfully ...\n\n");
									}
									else
									{
										rtp_printf("UPnP_Control Unsubscribe failed.\n");
									}

									break;

								case 83:
								case 115:
									/* subscribe test */
									rtp_printf("processing subscription .......\n\n");
									if (UPnP_ControlSubscribe (
											dh,
											serviceId,
											500,
											0,
											0
										) >=0)
									{
										rtp_printf ("Waiting for events...\n\n");
									}
									else
									{
										rtp_printf("UPnP_Control subscribe failed.\n");
										done = 1;
									}
									break;

								case 88:
								case 120:
									done = 1;
									/* exit test */
									break;
							}

						}
					}
					else
					{
						rtp_printf("UPnP_ControlSubscribe failed.\n");
					}
				}
				else
				{
					rtp_printf (" service not found!\n");
				}

				UPnP_ControlServiceIteratorDone(&i);
			}
			else
			{
				rtp_printf (" search init failed!\n");
			}

			UPnP_ControlCloseDevice(dh);
		}
		else
		{
			rtp_printf (" open failed.\n");
		}

		rtp_strfree(deviceUrl);
	}
	else
	{
		rtp_printf (" no device found.\n");
	}

}

/*---------------------------------------------------------------------------*/
void serviceDescribeTest (UPnPRuntime *rt, UPnPControlPoint *cp)
{
	UPnPControlDeviceHandle dh;
	char* deviceUrl = 0;

	rtp_printf("\nSearching for any device...\n");
	UPnP_ControlFindAllDevices(cp, 2500, &deviceUrl, 1);

	if (deviceUrl)
	{
		// In case you want to open a specific device, you can use the 2 lines below
        //rtp_printf("\nOpening device at %s...", "http://192.168.0.6:1085");
        //dh = UPnP_ControlOpenDevice(cp, "http://192.168.0.6:1085/light.xml");
        dh = UPnP_ControlOpenDevice(cp, deviceUrl);
		if (dh != 0)
		{
			UPnPControlServiceIterator i;

			rtp_printf("device open.\n\nLooking for services...");

			/* find the SwitchPower:1 service and invoke an action on it */
			if (UPnP_ControlGetServices(dh, &i) >= 0)
			{
				UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

				if (serviceId)
				{
					RTPXML_Document *xmlDoc;
					int done = 0;
					UPnPControlDeviceInfo deviceInfo;

					rtp_printf("\n    Found (serviceId: %.40s)\n\n", serviceId);

					if (UPnP_ControlGetServiceOwnerDeviceInfo (dh, serviceId, &deviceInfo) >= 0)
					{
						rtp_printf (" Owner Device info for %.40s\n\n"
								"  deviceType:       %.60s\n"
								"  friendlyName:     %.60s\n"
								"  manufacturer:     %.60s\n"
								"  manufacturerURL:  %.60s\n"
								"  modelDescription: %.60s\n"
								"  modelName:        %.60s\n"
								"  modelNumber:      %.60s\n"
								"  modelURL:         %.60s\n"
								"  serialNumber:     %.60s\n"
								"  UDN:              %.60s\n"
								"  UPC:              %.60s\n"
								"  presentationURL:  %.60s\n\n",
								serviceId,
								deviceInfo.deviceType,
								deviceInfo.friendlyName,
								deviceInfo.manufacturer,
								deviceInfo.manufacturerURL,
								deviceInfo.modelDescription,
								deviceInfo.modelName,
								deviceInfo.modelNumber,
								deviceInfo.modelURL,
								deviceInfo.serialNumber,
								deviceInfo.UDN,
								deviceInfo.UPC,
								deviceInfo.presentationURL);
					}

					rtp_printf("Getting Service Description...", serviceId);

					xmlDoc = UPnP_ControlGetServiceInfo(dh, serviceId);
					if (xmlDoc)
					{
						DOMString str = rtpxmlPrintDocument(xmlDoc);

						rtp_printf("done.\n\n");

						if (str)
						{
							rtp_printf("%s\n\n", str);
							rtpxmlFreeDOMString(str);
						}
						rtpxmlDocument_free(xmlDoc);
					}


					rtp_printf("Getting Service Description (Asynchronous call).", serviceId);
					UPnP_ControlGetServiceInfoAsync(dh, serviceId, &done);
					while (!done)
					{
						rtp_printf(".");
						UPnP_ProcessState(rt, 500);
					}
				}
				else
				{
					rtp_printf (" service not found!\n");
				}

				UPnP_ControlServiceIteratorDone(&i);
			}
			else
			{
				rtp_printf (" search init failed!\n");
			}

			UPnP_ControlCloseDevice(dh);
		}
		else
		{
			rtp_printf (" open failed.\n");
		}

		rtp_strfree(deviceUrl);
	}
}

/*---------------------------------------------------------------------------*/
int controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	)
{
	int result = 0;
	switch (event->type)
	{
		case UPNP_CONTROL_EVENT_DEVICE_FOUND:
		{
			char** str = (char**) perRequest;

			rtp_printf ("   Device found at %.50s\n", event->data.discover.url);
            if(str) /* if callback data is passed */
            {
			    *str = rtp_strdup(event->data.discover.url);
            }
			result = 1; // if 1 is returned no further searches will be performed
			break;
		}

		case UPNP_CONTROL_EVENT_SEARCH_COMPLETE:
		{
			rtp_printf ("   Search Complete.\n\n\n");
			result = 1; // perform no furthur searches
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_ALIVE:
		{
			rtp_printf ("\n\n    Device alive at %.50s"
			        "\n\n       ST:  %.55s"
					"\n       USN: %.55s", event->data.discover.url, event->data.discover.type, event->data.discover.usn);
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_BYE_BYE:
		{
			rtp_printf ("\n\n    Device bye-bye "
			        "\n\n       ST:  %.55s"
					"\n       USN: %.55s", event->data.discover.type, event->data.discover.usn);
			break;
		}

		case UPNP_CONTROL_EVENT_ACTION_COMPLETE:
		{
			int* done = (int*) perRequest;
            const UPNP_CHAR* outValue;
            if(event->data.action.success == 0)
            {
                rtp_printf("Action cannot be executed....\n");
                rtp_printf("Description: %s\n", event->data.action.errorDescription);
            }
            rtp_printf ("Action Complete.\n");
            if(done)
            {
			    if(*done == -1)
                {
                    *done = 1;
                }
            }
            /* Response to action 'GetMinLevel' contains output value for
               variable "MinLevel". */
            outValue = UPnP_GetPropertyValueByName(event->data.action.response, "MinLevel");
            if (outValue)
            {
                *done = rtp_atoi(outValue);
            }
			break;
		}

		case UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED:
		{
			int* done = (int*) perRequest;
			rtp_printf ("UPnP_ControlGetServiceInfoAsync failed!");
            if(done)
            {
			    *done = 1;
            }
			break;
		}

		case UPNP_CONTROL_EVENT_SERVICE_INFO_READ:
		{
			int* done = (int*) perRequest;
			DOMString str = rtpxmlPrintDocument(event->data.service.scpdDoc);

			rtp_printf ("service info read:\n\n");

			if (str)
			{
				rtp_printf("%s\n\n", str);
				rtpxmlFreeDOMString(str);
			}
			rtpxmlDocument_free(event->data.service.scpdDoc);
            if(done)
            {
			    *done = 1;
            }
			break;
		}

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED:
			rtp_printf("subscription accepted\n\n");
			rtp_printf("subscription id is: %s\n", event->data.subscription.serviceId);
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED:
            rtp_printf("service Unsubscribed\n\n");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED:
			rtp_printf("subscription rejected\n\n");
			break;

		case UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE:
		{
			const UPNP_CHAR* str = UPnP_GetPropertyValue(event->data.notify.stateVars, "LoadLevelStatus");
			if (!str)
			{
				str = "[unknown]";
			}
			rtp_printf ("Notification: Load level is =%s\n", str);
			break;
		}

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION:
			rtp_printf ("Subscription Expiration Warning!\n");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED:
			rtp_printf ("Subscription Expired.\n");
			break;

        case UPNP_CONTROL_EVENT_DEVICE_OPEN:
        {
            int* done = (int*) perRequest;

            if(event->data.device.handle)
	        {
		        UPnPControlDeviceInfo info;
		        UPnPControlServiceIterator i;

		        UPnP_ControlGetDeviceInfo(event->data.device.handle, &info);

		        rtp_printf (" success!\n\n"
				        "  deviceType:       %.60s\n"
				        "  friendlyName:     %.60s\n"
				        "  manufacturer:     %.60s\n"
				        "  manufacturerURL:  %.60s\n"
				        "  modelDescription: %.60s\n"
				        "  modelName:        %.60s\n"
				        "  modelNumber:      %.60s\n"
				        "  modelURL:         %.60s\n"
				        "  serialNumber:     %.60s\n"
				        "  UDN:              %.60s\n"
				        "  UPC:              %.60s\n"
				        "  presentationURL:  %.60s\n\n",
				        info.deviceType,
				        info.friendlyName,
	 			        info.manufacturer,
				        info.manufacturerURL,
				        info.modelDescription,
				        info.modelName,
				        info.modelNumber,
				        info.modelURL,
				        info.serialNumber,
				        info.UDN,
				        info.UPC,
				        info.presentationURL);

		        if (UPnP_ControlGetServices(event->data.device.handle, &i) >= 0)
		        {
			        UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			        while (serviceId)
			        {
				        rtp_printf ("Found Service: %.60s\n", serviceId);
				        rtp_printf ("  (type: %.65s)\n", UPnP_ControlGetServiceType
                                (event->data.device.handle,
                                serviceId)
                            );
				        serviceId = UPnP_ControlNextService(&i);
			        }

			        UPnP_ControlServiceIteratorDone(&i);
		        }

		        UPnP_ControlCloseDevice(event->data.device.handle);
	        }
	        rtp_printf("\n");
            if(done)
            {
			    *done = 1;
            }

            break;
        }
        case UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED:
        {
            int* done = (int*) perRequest;
            rtp_printf("Device Open Failed\n");
            if(done)
            {
			    *done = 1;
            }

            break;
        }
	}

	return (result);
}
