/*============================ D I S C O V E R . C ==========================*
*
*   Copyright EBS Inc , 1993-2004
*   All rights reserved.
*   This code may not be redistributed in source or linkable object form
*   without the consent of its author.
*
*   Purpose:    Library of functions related to the discovery of the UPnP
                enabled device
*
*   Changes:    01-21-2004 VM - Created.
******************************************************************************/

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "deviceDiscover.h"
#include "deviceDescribe.h"
#include "rtpxml.h"
#include "upnp.h"
#include "upnpdom.h"
#include "ssdpsrv.h"
#include "rtpmem.h"
#include "rtpstr.h"
/*****************************************************************************/
// Macros
/*****************************************************************************/
#define TIMEOUTSEC      1800

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/
extern unsigned short mcastPort;
extern unsigned char v4mcastAddr[];
extern char IPV6_LINK_LOCAL_ADDRESS[];
/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/
int _UPnP_DeviceSendRootDeviceAlive  (UPnPRootDevice *rootDevice, int deep);
int _UPnP_DeviceSendDeviceAlive      (UPnPDevice *device);
int _UPnP_DeviceSendServiceAlive     (UPnPService *service);
int _UPnP_DeviceSendRootDeviceByeBye (UPnPRootDevice *rootDevice, int deep);
int _UPnP_DeviceSendDeviceByeBye     (UPnPDevice *device);
int _UPnP_DeviceSendServiceByeBye    (UPnPService *service);

static char *_getLocation (unsigned char * remoteAddr, int remotePort,
                           int addrType, UPnPRootDevice *rootDevice);

static void _freeLocation (UPnPRootDevice *rootDevice,
                           char *str);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
                            UPnP_SSDPCallback
 *----------------------------------------------------------------------------*/
/** @memo SSDP callback for UPnP.
    @doc The callback function checks the type of SSDP request and creates a
    corresponding response. This response is queued in a list with a scheduled
    delivery time

    @return error code
 */

int UPnP_DeviceSSDPCallback (
		SSDPServerContext* ctx, /** the SSDP context */
		SSDPServerRequest* serverRequest, /** address of buffer holding ssdp request
                                    information */
		void* cookie            /** cookie holds pointer to device run time
                                    information */
	)
{
	UPnPRootDevice* rootDevice;
    UPnPDevice*     device;
    UPnPService*    service;
    DLListNode*     listNode;
	char*           descLocation;
	SSDPSearch*     search = &serverRequest->data.search;
	UPnPDeviceRuntime* rt = (UPnPDeviceRuntime*) cookie;

	rootDevice = (UPnPRootDevice*) rt->rootDeviceList.next;

	/* search through rootDevice list for a match to search->target */
    while (rootDevice != (UPnPRootDevice *) &rt->rootDeviceList)
    {
		descLocation = _getLocation(search->clientAddr.ipAddr, search->clientAddr.port,
			                        search->clientAddr.type, rootDevice);

   		if (descLocation)
   		{
			if (rtp_strcmp(search->target, "ssdp:all") == 0)
			{
			    //responds for all embedded and root devices
		        listNode = rootDevice->deviceList.next;
		        while (listNode != &rootDevice->deviceList)
		        {
		            device = (UPnPDevice *)listNode;
		  	        if (SSDP_QueueSearchResponse(ctx, search, descLocation,
		   	            device->UDN, TIMEOUTSEC) == -1)
		   	        {
		   	        	break;
		   	        }
		            listNode = listNode->next;
		        }
		    }

		    if (rtp_strcmp(search->target, "upnp:rootdevice") == 0)
			{
		        if (SSDP_QueueSearchResponse(ctx, search, descLocation,
		            rootDevice->device.UDN, TIMEOUTSEC) == -1)
		        {
					_freeLocation(rootDevice, descLocation);
		        	break;
		        }
		    }
		    // Check the available services for search target
			listNode = rootDevice->serviceList.next;
		    while (listNode != &rootDevice->serviceList)
		    {
		        service =  (UPnPService *)listNode;
		        if (rtp_strcmp(search->target, service->serviceType) == 0)
			    {
			        if (SSDP_QueueSearchResponse(ctx, search, descLocation,
			            service->device->UDN, TIMEOUTSEC) == -1)
			        {
						break;
			        }
		        }
		        listNode = listNode->next;
		    }

		    /* Check the available devices for search target */
		    listNode = rootDevice->deviceList.next;

		    while (listNode != &rootDevice->deviceList)
		    {
		        device = (UPnPDevice *) listNode;

		        if (rtp_strcmp(search->target, device->deviceType) == 0)
			    {
			        if (SSDP_QueueSearchResponse(ctx, search, descLocation,
			            device->UDN, TIMEOUTSEC) == -1)
			        {
						break;
			        }
		        }

//rtp_printf("HEREHERE UPnP_DeviceSSDPCallback trying device->UDN |%s|\n", device->UDN);
		        if(rtp_strcmp(search->target, device->UDN) == 0)
			    {
			        if (SSDP_QueueSearchResponse(ctx, search, descLocation,
			            device->UDN, TIMEOUTSEC) == -1)
			        {
						break;
			        }
		        }

		        listNode = listNode->next;
		    }

			_freeLocation(rootDevice, descLocation);
		}

		rootDevice = (UPnPRootDevice *) rootDevice->device.node.next;
	}

    return(0);
}

/*----------------------------------------------------------------------------*
                          UPnP_DeviceSendAllAlive
 *----------------------------------------------------------------------------*/
/** @memo Sends alive advertisements for everything under all the root devices.
    @doc Sends alive advertisements for each device and associated service for
    all the root devices.

    @return error code
 */

int UPnP_DeviceSendAllAlive (
		UPnPDeviceRuntime *runtime /** address of devices current runtime state */
	)
{
    DLListNode *listNode;
    UPnPRootDevice *rootDevice;

    listNode = runtime->rootDeviceList.next;
    while(listNode != &runtime->rootDeviceList)
    {
        rootDevice = (UPnPRootDevice *)listNode;
        if(rootDevice->sendingAnnouncements)
        {
            if(_UPnP_DeviceSendRootDeviceAlive(rootDevice, 0) < 0)
            {
                return(-1);
            }
        }
        listNode = listNode->next;
    }
    return(0);
}

/*----------------------------------------------------------------------------*
                        UPnP_DeviceSendRootDeviceAlive
 *----------------------------------------------------------------------------*/
/** Sends alive advertisements for a root device.

    @return error code
 */
int UPnP_DeviceSendRootDeviceAlive(
     UPnPRootDevice *rootDevice, /** address of the root device */
     int deep                    /** if 1, send advertisements for all embedded dives and services<br>
                                     if 0, send advertisements for the device under the root and its
                                     associated services */)
{
    if(rootDevice->sendingAnnouncements)
    {
        if(_UPnP_DeviceSendRootDeviceAlive(rootDevice, deep) < 0)
        {
            return(-1);
        }
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                        UPnP_DeviceSendDeviceAlive
 *----------------------------------------------------------------------------*/
/** Sends alive notifications for a device

    @return error code
 */
int UPnP_DeviceSendDeviceAlive(
     UPnPDevice *device /** pointer to the device */)
{
    if(_UPnP_DeviceSendDeviceAlive(device) < 0)
    {
        return(-1);
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                         UPnP_DeviceSendServiceAlive
 *----------------------------------------------------------------------------*/
/** Sends alive notifications for a service

    @return error code
 */

int UPnP_DeviceSendServiceAlive(
     UPnPService *service /** pointer to a service */)
{
    if(_UPnP_DeviceSendServiceAlive(service) < 0)
    {
        return(-1);
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                         _UPnP_DeviceSendDeviceAlive
 *----------------------------------------------------------------------------*/
/** Sends alive advertisements for a device.

    @return error code
 */

int _UPnP_DeviceSendDeviceAlive(
     UPnPDevice *device /** pointer to the device */)
{
    char *usn;
    UPnPRootDevice *rootDevice = device->rootDevice;
    char *descLocation;
    int result = -1;
	char destAddr[RTP_NET_NI_MAXHOST];
	int ipType = device->rootDevice->deviceRuntime->upnpRuntime->ipType;

    /* proceed if complete device information is available */
    if(!device->UDN || !device->deviceType || !device->rootDevice->descLocation)
    {
        return (-1);
    }

	if (ipType == RTP_NET_TYPE_IPV4)
    {
	    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
	}
	else if (ipType == RTP_NET_TYPE_IPV6)
	{
		rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
	}

	descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, rootDevice);

    if (descLocation)
    {
	    /* first device message of a specific device */
	    if(SSDP_SendAlive (
				&device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
				device->UDN,
				device->UDN,
				descLocation,
				(SSDP_UINT32*)&device->rootDevice->deviceRuntime->remoteCacheTimeoutSec
			) >= 0)
	    {
		    /* allocate memory to notifytype to hold UDN, devicetype, :: and a string terminator */
			usn = (char *) rtp_malloc((rtp_strlen(device->UDN) + rtp_strlen(device->deviceType) + 3));
			if (usn)
			{
			    /* create a notify type using device UDN and device type */
			    rtp_strcpy(usn, device->UDN);
			    rtp_strcat(usn, "::");
			    rtp_strcat(usn, device->deviceType);

			    /* Second device message of a specific device */
			    if(SSDP_SendAlive (&rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
			                       device->deviceType,
			                       usn,
			                       descLocation,
			                       (SSDP_UINT32*)&rootDevice->deviceRuntime->remoteCacheTimeoutSec) >= 0)

			    {
			    	result = 0;
			    }

			    rtp_free(usn);
			}
	    }

		_freeLocation(rootDevice, descLocation);
    }

    return (result);
}


/*----------------------------------------------------------------------------*
                          _UPnP_DeviceSendServiceAlive
 *----------------------------------------------------------------------------*/
/** Sends alive advertisements for a service.

    @return error code
 */

int _UPnP_DeviceSendServiceAlive(
     UPnPService *service /** pointer to the service */)
{
    char notifySubType[11];  /* holds ssdp:alive */
    char destAddr[RTP_NET_NI_MAXHOST];
    char *usn;
    UPnPRootDevice *rootDevice = service->device->rootDevice;
    char *descLocation;
    int result = -1;
    int ipType = service->device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext.ipType;

    /* proceed if complete device information is available */
    if(!service->serviceId || !service->serviceType || !service->device->rootDevice->descLocation)
    {
        return (-1);
    }

    if (ipType == RTP_NET_TYPE_IPV4)
    {
	    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
	}
	else if (ipType == RTP_NET_TYPE_IPV6)
	{
		rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
	}

	descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, rootDevice);

	if (descLocation)
	{
		/* allocate memory to notifytype to hold UDN, devicetype, :: and a string terminator */
		usn = (char *) rtp_malloc((rtp_strlen(service->serviceId) + rtp_strlen(service->serviceType) + 3));

		if (usn)
		{
			/* create a notify type using device UDN and device type */
			rtp_strcpy(usn, service->serviceId);
			rtp_strcat(usn, "::");
			rtp_strcat(usn, service->serviceType);

			rtp_strcpy(notifySubType, "ssdp:alive");

			/* service advertisement */
			if(SSDP_SendAlive (&rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
			                   service->serviceType,
			                   usn,
			                   descLocation,
			                   (SSDP_UINT32*)&rootDevice->deviceRuntime->remoteCacheTimeoutSec) >= 0)
			{
				result = 0;
			}

			rtp_free(usn);
		}

		_freeLocation(rootDevice, descLocation);
	}

    return (result);
}

/*----------------------------------------------------------------------------*
                          _UPnP_DeviceSendRootDeviceAlive
 *----------------------------------------------------------------------------*/
/** @memo Sends alive advertisements for every thing under all root devices.
    @doc Sends alive advertisements for each device and associated service for all the
    root devices.

    @return error code
 */

int _UPnP_DeviceSendRootDeviceAlive (
     UPnPRootDevice *rootDevice, /** pointer to the root device */
     int deep                    /** if 1, send advertisements for all embedded dives and services<br>
                                     if 0, send advertisements for the device under the root and its
                                     associated services */)
{
	char         notifySubType[11];  /* holds ssdp:alive */
	char         notifyType[16];     /* holds upnp:rootdevice */
	UPnPService *service;
	UPnPDevice  *device;
	DLListNode  *serviceListNode;
	DLListNode  *deviceListNode;
	char        *usn;
	int          result = -1;
	char        *descLocation;
	char destAddr[RTP_NET_NI_MAXHOST];
	int ipType = rootDevice->deviceRuntime->upnpRuntime->ssdpContext.ipType;

	/* allocate memory to usn to hold UDN, upnp:rootdevice, :: and a string terminator */
	usn = (char *) rtp_malloc((rtp_strlen(rootDevice->device.UDN) + 18));

	if (usn)
	{
		if (ipType == RTP_NET_TYPE_IPV4)
	    {
		    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
		}
		else if (ipType == RTP_NET_TYPE_IPV6)
		{
			rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
		}

		descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, rootDevice);

		if (descLocation)
		{
			/* create a notify type using device UDN and device type */
			rtp_strcpy(usn, rootDevice->device.UDN);
			rtp_strcat(usn, "::");
			rtp_strcat(usn, "upnp:rootdevice");

			rtp_strcpy(notifySubType, "ssdp:alive");
			rtp_strcpy(notifyType, "upnp:rootdevice");

			/* send root specific advertisement */
			/* send root specific advertisement */
			if(SSDP_SendAlive (&rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
			                   notifyType,
			                   usn,
			                   descLocation,
			                   (SSDP_UINT32*)&rootDevice->deviceRuntime->remoteCacheTimeoutSec) >= 0)
			{
				if(_UPnP_DeviceSendDeviceAlive(&rootDevice->device) >= 0)
				{
					result = 0;

					if(deep) /* advertise all devices and associated services */
					{
						/* advertise all services associated with this device */
						serviceListNode = rootDevice->serviceList.next;
						while(serviceListNode != &rootDevice->serviceList)
						{
							service = (UPnPService *)serviceListNode;

							/* if this service belongs to the device send an advertisement */
							if(service->device == &rootDevice->device)
							{
								if(_UPnP_DeviceSendServiceAlive(service) < 0)
								{
									result = -1;
									break;
								}
							}
							serviceListNode = serviceListNode->next;
						}

						if (result == 0)
						{
							/* advertise all devices in the device list */
							deviceListNode = rootDevice->deviceList.next;
							while(deviceListNode != &rootDevice->deviceList)
							{
								device = (UPnPDevice *)deviceListNode;

								if(_UPnP_DeviceSendDeviceAlive(device) < 0)
								{
									result = -1;
									break;
								}

								deviceListNode = deviceListNode->next;
							}
						}
					}
				}
			}

			_freeLocation(rootDevice, descLocation);
		}

		rtp_free(usn);
	}

	return(result);
}

/*----------------------------------------------------------------------------*
                          UPnP_DeviceSendAllByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye advertisements for each device and associated service for all the
    root devices.

    @return error code
 */

int UPnP_DeviceSendAllByeBye (
     UPnPDeviceRuntime *runtime /** address of devices current runtime state */)
{
    DLListNode     *listNode;

    /* send messages for all rootdevices and embedded devices and services */
    listNode = runtime->rootDeviceList.next;
    while(listNode != &runtime->rootDeviceList)
    {
        if(_UPnP_DeviceSendRootDeviceByeBye((UPnPRootDevice *)listNode, 0) < 0)
        {
            return(-1);
        }
        listNode = listNode->next;
    }
    return(0);
}

/*----------------------------------------------------------------------------*
                          UPnP_DeviceSendRootDeviceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications for a root device

    @return error code
 */

int UPnP_DeviceSendRootDeviceByeBye (
     UPnPRootDevice *rootDevice, /** address of the root device */
     int deep                    /** if 1, send advertisements for all embedded dives and services<br>
                                     if 0, send advertisements for the device under the root and its
                                     associated services */)
{
    if(_UPnP_DeviceSendRootDeviceByeBye(rootDevice, deep) < 0)
    {
        return(-1);
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                         UPnP_DeviceSendDeviceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications for the device

    @return error code
 */

int UPnP_DeviceSendDeviceByeBye(
     UPnPDevice *device /** pointer to the device */)
{
    if(_UPnP_DeviceSendDeviceByeBye(device) < 0)
    {
        return(-1);
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                          UPnP_DeviceSendServiceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications for the service

    @return error code
 */

int UPnP_DeviceSendServiceByeBye(
     UPnPService *service /** pointer to the service */)
{
    if(_UPnP_DeviceSendServiceByeBye(service) < 0)
    {
        return(-1);
    }
    return (0);
}

/*----------------------------------------------------------------------------*
                         _UPnP_DeviceSendRootDeviceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications for root device.

    @return error code
 */

int _UPnP_DeviceSendRootDeviceByeBye (
     UPnPRootDevice *rootDevice, /** address of the root device */
     int deep                    /** if 1, send advertisements for all embedded devices
                                     and services.<br>if 0, send advertisements for the
                                     device under the root and its associated services */)
{
    char         notifySubType[13];  /* holds ssdp:bye-bye */
    char         notifyType[16];     /* holds upnp:rootdevice */
    UPnPService *service;
    UPnPDevice  *device;
    DLListNode  *serviceListNode;
    DLListNode  *deviceListNode;
    char        *usn;
    char        *descLocation;
    int          result = -1;
	char destAddr[RTP_NET_NI_MAXHOST];
	int ipType = rootDevice->deviceRuntime->upnpRuntime->ssdpContext.ipType;

    if (ipType == RTP_NET_TYPE_IPV4)
    {
	    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
	}
	else if (ipType == RTP_NET_TYPE_IPV6)
	{
		rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
	}

	descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, rootDevice);

    if (descLocation)
    {
		/* allocate memory to usn to hold UDN, upnp:rootdevice, :: and a string terminator */
		usn = (char *) rtp_malloc((rtp_strlen(rootDevice->device.UDN) + 18));

		if (usn)
		{

			/* create a notify type using device UDN and device type */
			rtp_strcpy(usn, rootDevice->device.UDN);
			rtp_strcat(usn, "::");
			rtp_strcat(usn, "upnp:rootdevice");

			rtp_strcpy(notifySubType, "ssdp:bye-bye");
			rtp_strcpy(notifyType, "upnp:rootdevice");

			/* send root specific advertisement */
			if(SSDP_SendByebye (&rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
			                   notifyType, usn) >= 0)
			{
				result = 0;

				if (deep == 0) /* advertise the device under rootdevice and its associated services */
				{
					if (_UPnP_DeviceSendDeviceByeBye(&rootDevice->device) < 0)
					{
						result = -1;
					}

					if (result == 0)
					{
						/* advertise all services associated with this device */
						serviceListNode = rootDevice->serviceList.next;
						while(serviceListNode != &rootDevice->serviceList)
						{
							service = (UPnPService *) serviceListNode;

							if (_UPnP_DeviceSendServiceByeBye(service) < 0)
							{
								result = -1;
								break;
							}

							serviceListNode = serviceListNode->next;
						}

						if (result == 0)
						{
							/* advertise all devices in the device list */
							deviceListNode = rootDevice->deviceList.next;
							while(deviceListNode != &rootDevice->deviceList)
							{
								device = (UPnPDevice *)deviceListNode;

								if(_UPnP_DeviceSendDeviceByeBye(device) < 0)
								{
									result = -1;
									break;
								}

								deviceListNode = deviceListNode->next;
							}
						}
					}
				}
			}

			rtp_free(usn);
		}

		_freeLocation(rootDevice, descLocation);
	}

	return(0);
}

/*----------------------------------------------------------------------------*
                          _UPnP_DeviceSendDeviceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications for the device

    @return error code
 */

int _UPnP_DeviceSendDeviceByeBye (
     UPnPDevice *device /** address of the device */)
{
    char notifySubType[13];  /* holds ssdp:bye-bye */
    char *usn;
	char *descLocation;
	int   result = -1;
	char destAddr[RTP_NET_NI_MAXHOST];
	int ipType = device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext.ipType;

    /* proceed if complete device information is available */
    if(!device->UDN || !device->deviceType || !device->rootDevice->descLocation)
    {
        return (-1);
    }

	if (ipType == RTP_NET_TYPE_IPV4)
    {
	    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
	}
	else if (ipType == RTP_NET_TYPE_IPV6)
	{
		rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
	}

	descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, device->rootDevice);


	if (descLocation)
	{
		rtp_strcpy(notifySubType, "ssdp:bye-bye");

		/* first device message of a specific device */
		if(SSDP_SendByebye(&device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
						   device->UDN, device->UDN) >= 0)

		{

			/* allocate memory to notifytype to hold UDN, devicetype, :: and a string terminator */
			usn = (char *) rtp_malloc((rtp_strlen(device->UDN) + rtp_strlen(device->deviceType) + 3));
			if (usn)
			{
				/* create a notify type using device UDN and device type */
				rtp_strcpy(usn, device->UDN);
				rtp_strcat(usn, "::");
				rtp_strcat(usn, device->deviceType);

				/* Second device message of a specific device */
				if(SSDP_SendByebye(&device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
					   device->UDN, device->UDN) >= 0)
				{
					result = 0;
				}

				rtp_free(usn);
			}
		}

		_freeLocation(device->rootDevice, descLocation);
	}

    return (result);
}

/*----------------------------------------------------------------------------*
                          _UPnP_DeviceSendServiceByeBye
 *----------------------------------------------------------------------------*/
/** Sends bye-bye notifications a service.

    @return error code
 */

int _UPnP_DeviceSendServiceByeBye (
     UPnPService *service /** address of the service */)
{
	char notifySubType[13];  /* holds ssdp:bye-bye */
	char *usn;
	int result = -1;
	char destAddr[RTP_NET_NI_MAXHOST];
	char *descLocation;
	int ipType = service->device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext.ipType;

	/* proceed if complete device information is available */
	if(!service->serviceId || !service->serviceType || !service->device->rootDevice->descLocation)
	{
		return (-1);
	}

	if (ipType == RTP_NET_TYPE_IPV4)
    {
	    rtp_strcpy(destAddr, (const char *) v4mcastAddr);
	}
	else if (ipType == RTP_NET_TYPE_IPV6)
	{
		rtp_strcpy(destAddr, IPV6_LINK_LOCAL_ADDRESS);
	}

	descLocation = _getLocation((unsigned char *) destAddr, mcastPort, ipType, service->device->rootDevice);


	if (descLocation)
	{
		/* allocate memory to notifytype to hold UDN, devicetype, :: and a string terminator */
		usn = (char *) rtp_malloc((rtp_strlen(service->serviceId) + rtp_strlen(service->serviceType) + 3) * sizeof(char));
		if (usn)
		{
			/* create a notify type using device UDN and device type */
			rtp_strcpy(usn, service->serviceId);
			rtp_strcat(usn, "::");
			rtp_strcat(usn, service->serviceType);

			rtp_strcpy(notifySubType, "ssdp:bye-bye");

			/* service advertisement */
			if (SSDP_SendByebye(&service->device->rootDevice->deviceRuntime->upnpRuntime->ssdpContext,
								service->serviceType, usn) >= 0)
			{
				result = 0;
			}

			rtp_free(usn);
		}

		_freeLocation(service->device->rootDevice, descLocation);
	}

	return (result);
}

/*----------------------------------------------------------------------------*
                                  _getLocation
 *----------------------------------------------------------------------------*/
/*
    @return
 */
char *_getLocation (unsigned char * remoteAddr, int remotePort, int addrType,
                    UPnPRootDevice *rootDevice)
{
	char *toReturn = 0;

	if (rootDevice->autoAddr)
	{
		RTP_NET_ADDR serverAddr = {0};

		rtp_net_getifaceaddr(serverAddr.ipAddr, remoteAddr, remotePort, addrType);

		serverAddr.port = HTTP_ServerGetPort(&rootDevice->deviceRuntime->upnpRuntime->httpServer);
		serverAddr.type = addrType;

	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_claim(rootDevice->mutex);
	 #endif

	 	toReturn = UPnP_DOMSubstituteAddr(rootDevice->descLocation, &serverAddr);

	 #ifdef UPNP_MULTITHREAD
		rtp_sig_mutex_release(rootDevice->mutex);
	 #endif
	}
	else
	{
		toReturn = rootDevice->descLocation;
	}

 	return (toReturn);
}

void _freeLocation (UPnPRootDevice *rootDevice, char *str)
{
	if (rootDevice->autoAddr)
	{
		rtp_free(str);
	}
}
