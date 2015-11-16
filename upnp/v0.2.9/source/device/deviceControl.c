//
// CONTROL.C - UPnP Control-related functions
//
// EBS - UPnP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "upnp.h"
#include "deviceControl.h"
#include "soapsrv.h"
#include "upnpdom.h"
#include "rtpmem.h"
#include "rtpstr.h"
#include "rtpstdup.h"
#include "rtpprint.h"

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

int UPnP_DeviceControlSOAPCallback (
		SOAPServerContext *ctx,
		SOAPRequest *request,
		void *cookie);

static char * _getControlURL (
		RTPXML_Document *descDoc,
		RTPXML_Element *serviceElement);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
                            UPnP_DeviceControlBindService
 *----------------------------------------------------------------------------*/
/** Initialize a service for control. Binds a service control URL to the
    HTTP server to enable SOAP action processing for that service.

    @return error code
 */
int  UPnP_DeviceControlBindService (
		UPnPDeviceRuntime *deviceRuntime, /** the device runtime that owns the
		                                      service. */
        UPnPService *service              /** the service to bind */)
{
	HTTP_CHAR *path = _getControlURL(service->device->rootDevice->descDoc, service->serviceElement);

	if (path)
	{
		if (SOAP_ServerInit(&service->soapContext, UPnP_DeviceControlSOAPCallback, service) < 0)
		{
			rtp_free(path);
			return (-1);
		}

		if (HTTP_ServerAddPath(&deviceRuntime->upnpRuntime->httpServer,
							   path,
							   1,
							   (HTTPRequestHandler)SOAP_ServerProcessRequest,
							   0,
							   &service->soapContext) < 0)
		{
			rtp_free(path);
			SOAP_ServerShutdown(&service->soapContext);
			return (-1);
		}

		rtp_free(path);
		return (0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                           UPnP_DeviceControlUnBindService
 *----------------------------------------------------------------------------*/
/** Stop control on a service. Removes the HTTP server binding for this
    service's control URL.

    @return error code
 */
int  UPnP_DeviceControlUnBindService (
		UPnPDeviceRuntime *deviceRuntime, /** the device runtime that owns the
		                                      service. */
		UPnPService *service              /** the service to unbind */)
{
HTTP_CHAR *path = _getControlURL(service->device->rootDevice->descDoc, service->serviceElement);

	if (path)
	{
		/* build the event URL from the baseURL and eventSubURL in the DOM */

		if (HTTP_ServerRemovePath(&deviceRuntime->upnpRuntime->httpServer,
								  path) < 0)
		{
			rtp_free(path);
			return (-1);
		}

		SOAP_ServerShutdown(&service->soapContext);

		rtp_free(path);
		return (0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                           UPnP_DeviceControlSOAPCallback
 *----------------------------------------------------------------------------*/
/** SOAP callback for UPnP.  Handles a single incoming SOAP control request by
    parsing the action document and calling the device callback.

    @return error code; tells SOAP whether the action was successful or
            generated a fault
 */
int UPnP_DeviceControlSOAPCallback (
		SOAPServerContext *ctx, /** the SOAP context */
		SOAPRequest *request,   /** the action request */
		void *cookie            /** the UPnPService being controlled */)
{
UPnPService *service = (UPnPService *) cookie;
UPnPRootDevice *rootDevice = service->device->rootDevice;

	/* first to check if the device callback is set */
	if (rootDevice->userCallback)
	{
		/* parse the service type URI and the action name out of the
		   SOAPAction header string */
		UPNP_CHAR *soapAction = (UPNP_CHAR *) rtp_malloc(
				(rtp_strlen(request->in.soapAction) + 1) * sizeof(UPNP_CHAR));

		if (soapAction)
		{
			UPNP_CHAR *actionName;
			UPNP_CHAR *serviceTypeURI;
			char *str;

			/* work with a copy of the soapAction since we'll be
			   chopping it up */
			rtp_strcpy(soapAction, request->in.soapAction);

			serviceTypeURI = soapAction;

			/* SOAPAction is often quote-enclosed */
			if (*serviceTypeURI == '"')
			{
				serviceTypeURI++;
			}

			/* '#' signals the beginning of the action tag name */
			str = rtp_strstr(serviceTypeURI, "#");
			if (str)
			{
				long len;
				RTPXML_NodeList *nodeList;

				*str = 0;
				str++;
				actionName = str;

				len = rtp_strlen(actionName);

				/* remove the trailing quote if there is one */
				if (actionName[len-1] == '"')
				{
					actionName[len-1] = 0;
				}

				/* the action tag must be qualified by the xml namespace
				   of the service type */
	  			nodeList = rtpxmlElement_getElementsByTagNameNS (
	  							request->in.envelope.bodyElem,
	  							serviceTypeURI,
	  							actionName);

	  			if (nodeList)
	  			{
	  				RTPXML_Element *actionElement;

	  				actionElement = (RTPXML_Element *) rtpxmlNodeList_item(nodeList, 0);
	  				rtpxmlNodeList_free(nodeList);

	  				if (actionElement)
	  				{
		  				UPnPActionRequest actionRequest;

		  				rtp_memset(&actionRequest, 0, sizeof(UPnPActionRequest));

		  				actionRequest.in.actionName = actionName;
		  				actionRequest.in.serviceTypeURI = serviceTypeURI;
		  				actionRequest.in.actionElement = actionElement;
		  				actionRequest.in.serviceId = service->serviceId;
		  				actionRequest.in.deviceUDN = service->device->UDN;
		  				actionRequest.in.clientAddr = request->in.clientAddr;

		  				rootDevice->userCallback(rootDevice->userData,
						                         rootDevice->deviceRuntime,
						                         rootDevice,
						                         UPNP_DEVICE_EVENT_ACTION_REQUEST,
						                         &actionRequest);

						if (actionRequest.out.errorCode == 0)
						{
							/* success */
							request->out.body = actionRequest.out.result;
							rtp_free(soapAction);
							return (0);
						}
						else
						{
							/* fault */
							char temp[UPNP_MAX_ERROR_DESC_LEN + 176];

							rtp_sprintf(temp, "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
							                 "<errorCode>%d</errorCode>"
							                 "<errorDescription>%s</errorDescription>"
							                 "</UPnPError>",
							           actionRequest.out.errorCode,
							           actionRequest.out.errorDescription);

							rtp_strcpy(request->out.fault.code, "s:Client");
							rtp_strcpy(request->out.fault.string, "UPnPError");

							/* XML document will be freed by SOAP */
							request->out.fault.detail = rtpxmlParseBuffer(temp);
							rtp_free(soapAction);

							return (-1);
						}
	  				}
	  			}
			}

			rtp_free(soapAction);
		}
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                                _getControlURL
 *----------------------------------------------------------------------------*/
/** Calculates server-relative control URL for a service.  For example, if the
    service's absolute control URL is "http://192.168.1.110:5423/myService.0001",
    this function will allocate and return the string: "/myService.0001".

    @return error code
 */
char * _getControlURL (
		RTPXML_Document *descDoc,      /** the service's device description document */
		RTPXML_Element *serviceElement /** the service element inside descDoc */)
{
DOMString controlUrl;

	controlUrl = UPnP_DOMGetElemTextInElem(serviceElement, "controlURL", 0);

	if (!controlUrl)
	{
		return (0);
	}

	return (UPnP_DOMGetServerPath(descDoc, controlUrl));
}
