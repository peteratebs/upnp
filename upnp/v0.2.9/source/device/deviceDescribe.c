/*============================ D E S C R I B E . C ==========================*
*
*   Copyright EBS Inc , 1993-2004
*   All rights reserved.
*   This code may not be redistributed in source or linkable object form
*   without the consent of its author.
*
*   Purpose:    UPnP functions descrobing root device, embedded
*               devices and services
*
*   Changes:    01-21-2004 VM - Created.
******************************************************************************/

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "deviceDescribe.h"
#include "rtpmem.h"
#include "rtpstr.h"
#include "upnpdom.h"
#include "upnp.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

void _UPnP_DeviceDescribeDevice  (UPnPDevice *device, RTPXML_Element *deviceElement,
                                  UPnPRootDevice *rootDevice, int maxDepth);

void _UPnP_DeviceDescribeService (UPnPService *serviceNode,
                                  RTPXML_Element *serviceElement,
                                  UPnPDevice *device);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
                            UPnP_DescribeRootDevice
 *----------------------------------------------------------------------------*/
/** Get the root device elements needed by SSDP, GENA and SOAP.

    @return Address of UPnPRootDevice buffer containing root device information
 */
UPnPRootDevice * UPnP_DeviceDescribeRootDevice(
		          RTPXML_Document *doc, /** pointer to device document xml page */
		          int maxDepth        /** indicates the depth level this funtion
	                                      will search the dom tree for devices
	                                      embedded within this device.<br>If maxDepth
	                                      = x, where x > 0, then UPnPDevice
	                                      buffer will search for embedded
	                                      devices and related services that are x
	                                      level deep in the XML DOM tree.<br>If
                                          maxDepth = 0 then only the device and its
                                          related service information are described */)
{
UPnPRootDevice *rootDevice = 0;
RTPXML_Element   *deviceElement;

    deviceElement = UPnP_DOMGetElemInDoc(doc, "device", 0);

	if (deviceElement)
	{
		rootDevice = (UPnPRootDevice *) rtp_malloc(sizeof(UPnPRootDevice));
		if (rootDevice)
		{
			rtp_memset(rootDevice, 0, sizeof(UPnPRootDevice));

			/* Store address of descriptor document */
			rootDevice->descDoc = doc;

			/* Initialize service and device list */
			DLLIST_INIT(&rootDevice->serviceList);
			DLLIST_INIT(&rootDevice->deviceList);

			/* extract device inforamtion */
			_UPnP_DeviceDescribeDevice (
					&rootDevice->device,
					deviceElement,
					rootDevice,
					maxDepth
				);
		}
	}

	return (rootDevice);
}

/*----------------------------------------------------------------------------*
                              UPnP_DeviceDescribeDevice
 *----------------------------------------------------------------------------*/
/** Extracts the device information needed by SSDP, GENA and SOAP.

    @return Address of UPnPDevice buffer having device information
 */

UPnPDevice * UPnP_DeviceDescribeDevice (
              RTPXML_Element *deviceElement, /** location of device information inside
                                               xml dom tree */
              UPnPRootDevice *rootDevice,  /** pointer to the device's root device */
              int maxDepth                 /** indicates the depth level this funtion
		                                       will search the dom tree for devices
		                                       embedded within this device.<br>If maxDepth
		                                       = x, where x > 0, then UPnPDevice
		                                       buffer will search for embedded
		                                       devices and related services that are x
		                                       level deep in the XML DOM tree.<br>If
                                               maxDepth = 0 then only the device and its
                                               related service information are described */)

{
    UPnPDevice *device;

    /* get a buffer to store device info */
    device = (UPnPDevice *)rtp_malloc(sizeof(UPnPDevice));
	if (device)
	{
		rtp_memset(device, 0, sizeof(UPnPDevice));
		_UPnP_DeviceDescribeDevice(device, deviceElement, rootDevice, maxDepth);
	}

    return (device);
}


/*----------------------------------------------------------------------------*
                            _UPnP_DeviceDescribeDevice
 *----------------------------------------------------------------------------*/
/** Extracts device information.  Gets the required information from a XML
    document searching maxDepth deep for any embedded devices and related
    services under this device

    @return None
 */

void _UPnP_DeviceDescribeDevice(
      UPnPDevice *device,          /** pointer to UPnPDevice buffer to store
                                       extracted information in */
      RTPXML_Element *deviceElement, /** location of device information inside
                                       xml dom tree */
      UPnPRootDevice *rootDevice,  /** pointer to the device's root device */
      int maxDepth                 /** indicates the depth level this funtion
		                               will search the dom tree for devices
		                               embedded within this device */)
{
    UPnPService   *service;
	RTPXML_Node     *xmlNode;
    RTPXML_NodeList *nodeList;
	RTPXML_Element  *serviceListElement;
	int n;

    /* store the rootdevice info */
    device->rootDevice = rootDevice;

    /* Store device Type */
    device->deviceType = UPnP_DOMGetElemTextInElem(deviceElement, "deviceType", 0);

    /* Store UDN */
    device->UDN = UPnP_DOMGetElemTextInElem(deviceElement, "UDN", 0);

    /* insert this device at the end of device list */
    DLLIST_INIT(&device->node);

    //ServiceList
	serviceListElement = UPnP_DOMGetElemInElem(deviceElement, "serviceList", 0);
    if (serviceListElement)
    {
        nodeList = rtpxmlElement_getElementsByTagName(serviceListElement, "service");
		if (nodeList)
		{
			for (n = 0;; n++)
			{
				xmlNode = rtpxmlNodeList_item(nodeList, n);
				if(xmlNode == 0)
				{
					break;
				}

				service = UPnP_DeviceDescribeService((RTPXML_Element *) xmlNode, device);
				if (!service)
				{
					break;
				}
			}

			rtpxmlNodeList_free(nodeList);
		}
    }

    // check DeviceList

    if(maxDepth > 0)
    {
		RTPXML_Element *deviceListElement;

		maxDepth--;

		deviceListElement = UPnP_DOMGetElemInElem(deviceElement, "deviceList", 0);


		if (deviceListElement)
		{

			nodeList = rtpxmlElement_getElementsByTagName(deviceListElement, "device");

			if (nodeList)
			{
				UPnPDevice *childDevice;

				xmlNode = rtpxmlNodeList_item(nodeList, 0);

				while (xmlNode)
				{
					/* NOTE: casting the node to an element here might cause problems in
					   in the iXML DOM functions! */
					childDevice = UPnP_DeviceDescribeDevice((RTPXML_Element *) xmlNode, rootDevice, maxDepth);
					if (!childDevice)
					{
						break;
					}

				    DLLIST_INSERT_BEFORE(&rootDevice->deviceList, &childDevice->node);

					xmlNode = rtpxmlNode_getNextSibling(xmlNode);
				}

				rtpxmlNodeList_free(nodeList);
			}
		}
    }
}


/*----------------------------------------------------------------------------*
                              UPnP_DeviceDescribeService
 *----------------------------------------------------------------------------*/
/** Extracts the service information.

    @return Address of UPnPService buffer containing service information
 */

UPnPService *UPnP_DeviceDescribeService(
             RTPXML_Element *serviceElement, /** location of service information inside
                                               xml dom tree */
             UPnPDevice *device            /** address of device this service belongs
                                               to */)
{
    UPnPService   *serviceNode;

    serviceNode = (UPnPService *)rtp_malloc(sizeof(UPnPService));
	if (serviceNode)
	{
		rtp_memset(serviceNode, 0, sizeof(UPnPService));
		_UPnP_DeviceDescribeService(serviceNode, serviceElement, device);
		return (serviceNode);
	}

	return (0);
}

/*----------------------------------------------------------------------------*
                          _UPnP_DeviceDescribeService
 *----------------------------------------------------------------------------*/
/** Extracts service information. Gets the service related information from a XML
    document

    @return None
 */

void _UPnP_DeviceDescribeService(
      UPnPService *serviceNode,     /** pointer to UPnPService buffer to store
                                        extracted information in */
      RTPXML_Element *serviceElement, /** location of service information inside
                                        xml dom tree */
      UPnPDevice *device            /** address of device this service belongs
                                        to */)
{
	/* set device */
    serviceNode->device = device;

	serviceNode->serviceElement = serviceElement;

    /* extract Service type */
    serviceNode->serviceType = UPnP_DOMGetElemTextInElem(serviceElement, "serviceType", 0);

    /* extract Service Id */
    serviceNode->serviceId = UPnP_DOMGetElemTextInElem(serviceElement, "serviceId", 0);

    /* insert this service at the end of service list */
    DLLIST_INSERT_BEFORE(&(device->rootDevice->serviceList), &serviceNode->node);
}


/*----------------------------------------------------------------------------*
                        UPnP_FreeRootDevice
 *----------------------------------------------------------------------------*/
/** Frees the resources used by a UPnPRootDevice type root device

    @return None
 */

void UPnP_DeviceFreeRootDevice(
      UPnPRootDevice *rootDevice /** pointer to root device whose resources
                                     are to be freed */)
{
    DLListNode *nodePtr;
    DLListNode *nextNode;

    nodePtr = rootDevice->deviceList.next;
    while(nodePtr != &rootDevice->deviceList)
    {
        nextNode = nodePtr->next;
        UPnP_DeviceFreeDevice((UPnPDevice *)nodePtr);
        nodePtr = nextNode;
    }
    nodePtr = rootDevice->serviceList.next;
    while(nodePtr != &rootDevice->serviceList)
    {
        nextNode = nodePtr->next;
        UPnP_DeviceFreeService((UPnPService *)nodePtr);
        nodePtr = nextNode;
    }
    rtp_free(rootDevice);
}

/*----------------------------------------------------------------------------*
                        UPnP_FreeDevice
 *----------------------------------------------------------------------------*/
/** Frees the resources used by the device

    @return None
 */
void UPnP_DeviceFreeDevice(
      UPnPDevice *device /** pointer to device whose resources are to be freed */)
{
    DLLIST_REMOVE(&device->node);
    rtp_free(device);
}

/*----------------------------------------------------------------------------*
                        UPnP_FreeService
 *----------------------------------------------------------------------------*/
/** Frees the resources used by the device

    @return None
 */
void UPnP_DeviceFreeService(
      UPnPService *service /** pointer to service whose resources are to be freed */)
{
    DLLIST_REMOVE(&service->node);
    rtp_free(service);
}
