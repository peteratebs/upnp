/*
|  CONTROL_DESCRIBE.C -
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

#include "controlDescribe.h"
#include "upnpdom.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

#define UPNP_CONTROL_HTTP_TIMEOUT_MSEC (30*1000)
#define INCREASE_BUFFER_SIZE(X)        ((X < 0x4000)? (X << 1) : (X + 0x4000))

#define UPNP_CLIENT_ENTER_API(X)   UPNP_RUNTIME_ENTER_CRITICAL((X)->runtime)
#define UPNP_CLIENT_EXIT_API(X)    UPNP_RUNTIME_EXIT_CRITICAL((X)->runtime)

/*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

void _UPnP_ControlDeleteDevice (
		UPnPControlDevice* device
	);

void _UPnP_ControlDeleteService (
		UPnPControlService* service
	);

int _UPnP_ControlGetDocumentInit (
		UPnPControlGetDocument* getDoc,
		HTTPManagedClient* httpClient,
		UPNP_INT16 ipType,
		URLDescriptor* urlDesc,
		RTP_NET_SM_CALLBACK smCallback
	);

void _UPnP_ControlGetDocumentCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

void _UPnP_ControlOpenDeviceCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

void _UPnP_ControlGetServiceInfoCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	);

int _UPnP_ControlDescribeDevice (
		UPnPControlDevice* device,
		RTPXML_Document* xmlDescDoc,
		URLDescriptor* descURL
	);

UPnPControlService* _UPnP_ControlNewService (
		UPnPControlPoint* cp,
		UPnPControlDevice* device,
		RTPXML_Element* serviceElement
	);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*---------------------------------------------------------------------------*/
UPnPControlOpenDevice* UPnP_ControlNewOpenDevice (
		UPnPControlPoint* cp,
		UPnPControlDevice* device,
		UPNP_CHAR* url,
		void* userData,
		UPNP_BOOL generateEvent
	)
{
	UPnPControlOpenDevice* openSm;

	openSm = (UPnPControlOpenDevice*) rtp_malloc(sizeof(UPnPControlOpenDevice));

	if (openSm)
	{
		URLDescriptor urlDesc;

		if (URL_Parse(&urlDesc, url, URL_PROTOCOL_HTTP, cp->runtime->ipType) >= 0)
		{
			if (_UPnP_ControlGetDocumentInit (
					&openSm->base,
					&cp->httpClient,
					cp->runtime->ipType,
					&urlDesc,
					_UPnP_ControlOpenDeviceCallback
				) >= 0)
			{
				openSm->device = device;
				openSm->userData = userData;
				openSm->generateEvent = generateEvent;

				URL_Free(&urlDesc);

				_UPnP_ControlDeviceAddReference(device);

				return (openSm);
			}

			URL_Free(&urlDesc);
		}

		rtp_free(openSm);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int _UPnP_ControlGetDocumentInit (
		UPnPControlGetDocument* getDoc,
		HTTPManagedClient* httpClient,
		UPNP_INT16 ipType,
		URLDescriptor* urlDesc,
		RTP_NET_SM_CALLBACK smCallback
	)
{
	if (urlDesc->protocolType != URL_PROTOCOL_HTTP ||
		!urlDesc->field.http.host ||
		!urlDesc->field.http.path)
	{
		return (-1);
	}

	if (rtp_net_sm_init(&getDoc->base, smCallback) >= 0)
	{
		if (URL_Copy(&getDoc->urlDesc, urlDesc) >= 0)
		{
			if (HTTP_ManagedClientStartTransaction (
					httpClient,
					getDoc->urlDesc.field.http.host,
					getDoc->urlDesc.field.http.port,
					ipType,
					HTTP_SESSION_TYPE_TCP,
					0, /* non-blocking */
					&getDoc->httpSession
				) >= 0)
			{
				getDoc->state = UPNP_CONTROL_HTTP_STATE_CONNECTING;
				getDoc->docBuffer = 0;
				getDoc->docBufferSize = 0;
				getDoc->docBytesRead = 0;

				return (0);
			}

			URL_Free(&getDoc->urlDesc);
		}

		rtp_net_sm_delete(&getDoc->base);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
UPnPControlGetServiceInfo* UPnP_ControlNewGetServiceInfo (
		UPnPControlPoint* cp,
		UPnPControlService* service,
		void* userData,
		UPNP_BOOL generateEvent
	)
{
	UPnPControlGetServiceInfo* openSm;

	openSm = (UPnPControlGetServiceInfo*) rtp_malloc(sizeof(UPnPControlGetServiceInfo));

	if (openSm)
	{
		URLDescriptor scpdURL;

		if (URL_Parse(&scpdURL, service->scpdURL, URL_PROTOCOL_HTTP, cp->runtime->ipType) >= 0)
		{
			URLDescriptor urlDesc;

			if (URL_GetAbsolute(&urlDesc, &scpdURL, &service->device->baseURL, cp->runtime->ipType) >= 0)
			{
				if (_UPnP_ControlGetDocumentInit (
						&openSm->base,
						&cp->httpClient,
						cp->runtime->ipType,
						&urlDesc,
						_UPnP_ControlGetServiceInfoCallback
					) >= 0)
				{
					openSm->service = service;
					openSm->userData = userData;
					openSm->generateEvent = generateEvent;
					openSm->scpdDoc = 0;

					_UPnP_ControlServiceAddReference(service);

					URL_Free(&urlDesc);
					URL_Free(&scpdURL);

					return (openSm);
				}
				URL_Free(&urlDesc);
			}
			URL_Free(&scpdURL);
		}
		rtp_free(openSm);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlOpenDeviceCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlOpenDevice* openSm = (UPnPControlOpenDevice*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			_UPnP_ControlGetDocumentCallback(sm, event, result);
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			UPnPControlHTTPState oldState = openSm->base.state;

			result->done = 0;

			_UPnP_ControlGetDocumentCallback(sm, event, result);

			switch (openSm->base.state)
			{
				case UPNP_CONTROL_HTTP_STATE_READING_DATA:
					if (oldState == UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE)
					{
						/* check the contentType to make sure it is text/xml */
						if (openSm->base.contentType != FILE_TYPE_TEXT_XML)
						{
							openSm->base.state = UPNP_CONTROL_HTTP_STATE_ERROR;
							result->done = 1;
						}
					}
					break;

				case UPNP_CONTROL_HTTP_STATE_DONE:
				{
					RTPXML_Document* xmlDoc = rtpxmlParseBuffer((char*) openSm->base.docBuffer);

					openSm->device->descDoc = 0;

					if (xmlDoc)
					{
						if (_UPnP_ControlDescribeDevice (openSm->device, xmlDoc, &openSm->base.urlDesc) >= 0)
						{
							/* increment the reference count */
							_UPnP_ControlDeviceAddReference(openSm->device);
							break;
						}

						rtpxmlDocument_free(openSm->device->descDoc);
                        openSm->device->descDoc = 0;
					}
					openSm->base.state = UPNP_CONTROL_HTTP_STATE_ERROR;
					break;
				}
			}

			if (result->done)
			{
				if (openSm->generateEvent)
				{
					UPnPControlEvent event;

					if (openSm->base.state == UPNP_CONTROL_HTTP_STATE_ERROR)
					{
						event.type = UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED;
					}
					else
					{
						event.type = UPNP_CONTROL_EVENT_DEVICE_OPEN;
						event.data.service.deviceHandle = openSm->device;
					}

					UPNP_CLIENT_EXIT_API(openSm->device->controlPoint);

					openSm->device->controlPoint->callbackFn (
							openSm->device->controlPoint,
							&event,
							openSm->device->controlPoint->callbackData,
							openSm->userData
						);

					UPNP_CLIENT_ENTER_API(openSm->device->controlPoint);
				}
			}
			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			_UPnP_ControlGetDocumentCallback(sm, event, result);
			_UPnP_ControlDeviceRemoveReference(openSm->device);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(openSm);
			break;
	}
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlGetServiceInfoCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlGetServiceInfo* openSm = (UPnPControlGetServiceInfo*) sm;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
			_UPnP_ControlGetDocumentCallback(sm, event, result);
			break;

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			UPnPControlHTTPState oldState = openSm->base.state;

			result->done = 0;

			_UPnP_ControlGetDocumentCallback(sm, event, result);

			switch (openSm->base.state)
			{
				case UPNP_CONTROL_HTTP_STATE_READING_DATA:
					if (oldState == UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE)
					{
						/* check the contentType to make sure it is text/xml */
						if (openSm->base.contentType != FILE_TYPE_TEXT_XML)
						{
							openSm->base.state = UPNP_CONTROL_HTTP_STATE_ERROR;
							result->done = 1;
						}
					}
					break;

				case UPNP_CONTROL_HTTP_STATE_DONE:
					openSm->scpdDoc = rtpxmlParseBuffer((char*) openSm->base.docBuffer);

					if (!openSm->scpdDoc)
					{
						openSm->base.state = UPNP_CONTROL_HTTP_STATE_ERROR;
					}
					break;
			}

			if (result->done)
			{
				if (openSm->generateEvent)
				{
					UPnPControlEvent event;

					event.data.service.deviceHandle = openSm->service->device;
					event.data.service.id           = openSm->service->serviceId;

					if (openSm->base.state == UPNP_CONTROL_HTTP_STATE_ERROR)
					{
						event.type = UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED;
						event.data.service.scpdDoc  = 0;
					}
					else
					{
						event.type = UPNP_CONTROL_EVENT_SERVICE_INFO_READ;
						event.data.service.scpdDoc  = openSm->scpdDoc;
					}

					UPNP_CLIENT_EXIT_API(openSm->service->device->controlPoint);

					openSm->service->device->controlPoint->callbackFn (
							openSm->service->device->controlPoint,
							&event,
							openSm->service->device->controlPoint->callbackData,
							openSm->userData
						);

					UPNP_CLIENT_ENTER_API(openSm->service->device->controlPoint);
				}
			}
			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			_UPnP_ControlGetDocumentCallback(sm, event, result);
			_UPnP_ControlServiceRemoveReference(openSm->service);
			break;

		case RTP_NET_SM_EVENT_FREE:
			rtp_free(openSm);
			break;
	}
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlGetDocumentCallback (
		RTP_NET_SM* sm,
		RTP_NET_SM_EVENT* event,
		RTP_NET_SM_RESULT* result
	)
{
	UPnPControlGetDocument* getDoc = (UPnPControlGetDocument*) sm;
	RTP_SOCKET sd = (getDoc->httpSession)? HTTP_ManagedClientGetSessionSocket(getDoc->httpSession) : 0;

	switch (event->type)
	{
		case RTP_NET_SM_EVENT_ADD_TO_SELECT_LIST:
		{
			switch (getDoc->state)
			{
				case UPNP_CONTROL_HTTP_STATE_CONNECTING:
					result->maxTimeoutMsec = UPNP_CONTROL_HTTP_TIMEOUT_MSEC;
					rtp_fd_set(event->arg.addToSelectList.writeList, sd);
					rtp_fd_set(event->arg.addToSelectList.errList, sd);
					break;

				case UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE:
				case UPNP_CONTROL_HTTP_STATE_READING_DATA:
					result->maxTimeoutMsec = UPNP_CONTROL_HTTP_TIMEOUT_MSEC;
					rtp_fd_set(event->arg.addToSelectList.readList, sd);
					rtp_fd_set(event->arg.addToSelectList.errList, sd);
					break;

				case UPNP_CONTROL_HTTP_STATE_DONE:
				case UPNP_CONTROL_HTTP_STATE_ERROR:
					result->maxTimeoutMsec = 0;
					break;
			}
			break;
		}

		case RTP_NET_SM_EVENT_PROCESS_STATE:
		{
			result->done = 0;

			/* tbd - should this be here?
			if (rtp_fd_isset(event->arg.processState.errList, sd))
			{
				getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
				result->done = 1;
			}
			else
			{
			*/
				switch (getDoc->state)
				{
					case UPNP_CONTROL_HTTP_STATE_CONNECTING:
						if (rtp_fd_isset(event->arg.processState.writeList, sd))
						{
							if (HTTP_ManagedClientGet (
									getDoc->httpSession,
									getDoc->urlDesc.field.http.path,
									0 /* if-modified-since */
								) >= 0)
							{
								getDoc->state = UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE;
							}
							else
							{
								getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
								result->done = 1;
							}
						}
						break;

					case UPNP_CONTROL_HTTP_STATE_WAITING_RESPONSE:
						if (rtp_fd_isset(event->arg.processState.readList, sd))
						{
							HTTPResponseInfo responseInfo;

							if (HTTP_ManagedClientReadResponseInfo (
									getDoc->httpSession,
									&responseInfo
								) >= 0)
							{
								if (responseInfo.status == HTTP_STATUS_200_OK)
								{
									getDoc->docBufferSize = (responseInfo.contentLength > 0)? responseInfo.contentLength : 0x1000;
									getDoc->contentType = responseInfo.contentType;

									/* malloc a buffer to hold the whole document, include space for the
									   null terminator */
									getDoc->docBuffer = (UPNP_UINT8*) rtp_malloc(getDoc->docBufferSize + 1);
									if (getDoc->docBuffer)
									{
										getDoc->docBytesRead = 0;
										getDoc->state = UPNP_CONTROL_HTTP_STATE_READING_DATA;
									}
									else
									{
										getDoc->docBufferSize = 0;
										getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
										result->done = 1;
										break;
									}
								}
								else
								{
									getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
									result->done = 1;
									break;
								}
							}
							else
							{
								getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
								result->done = 1;
								break;
							}
						}
						else
						{
							break;
						}
						/* intentional fall-through */

					case UPNP_CONTROL_HTTP_STATE_READING_DATA:
						if (rtp_fd_isset(event->arg.processState.readList, sd))
						{
							HTTP_INT32 len = 0;

							do
							{
								/* if the buffer is full, then add some space to read data into */
								if (getDoc->docBytesRead == getDoc->docBufferSize)
								{
									UPNP_INT32 newSize = INCREASE_BUFFER_SIZE(getDoc->docBufferSize);
									UPNP_UINT8* newBuffer = (UPNP_UINT8*) rtp_realloc(getDoc->docBuffer, newSize + 1);
									if (newBuffer)
									{
										getDoc->docBuffer = newBuffer;
										getDoc->docBufferSize = newSize;
									}
									else
									{
										break;
									}
								}

								len = HTTP_ManagedClientRead (
										getDoc->httpSession,
										getDoc->docBuffer + getDoc->docBytesRead,
										getDoc->docBufferSize - getDoc->docBytesRead
									);

								if (len < 0)
								{
									if (len != HTTP_EWOULDBLOCK)
									{
										getDoc->state = UPNP_CONTROL_HTTP_STATE_ERROR;
										result->done = 1;
									}
									break;
								}

								if (len == 0)
								{
									break;
								}

								getDoc->docBytesRead += len;
							}
							/* if we filled the buffer, then try to read more data */
							while (len > 0);

							if (len == 0)
							{
								/* this is OK because we allocated an extra byte above */
								getDoc->docBuffer[getDoc->docBytesRead] = 0;
								getDoc->state = UPNP_CONTROL_HTTP_STATE_DONE;
								result->done = 1;
							}
						}
						break;

					case UPNP_CONTROL_HTTP_STATE_DONE:
					case UPNP_CONTROL_HTTP_STATE_ERROR:
						result->done = 1;
						break;
				}
			//}  tbd - see above
			break;
		}

		case RTP_NET_SM_EVENT_DESTROY:
			URL_Free(&getDoc->urlDesc);
			HTTP_ManagedClientFinishTransaction(getDoc->httpSession);
			getDoc->httpSession = 0;
			if (getDoc->docBuffer)
			{
				rtp_free(getDoc->docBuffer);
			}
			break;

		case RTP_NET_SM_EVENT_FREE:
			break;
	}
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlDeviceAddReference (
		UPnPControlDevice* device
	)
{
	device->refCount++;
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlDeviceRemoveReference (
		UPnPControlDevice* device
	)
{
	UPNP_ASSERT(device->refCount > 0, "No reference to remove");
	device->refCount--;
	if (device->refCount == 0)
	{
		_UPnP_ControlDeleteDevice(device);
	}
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlServiceAddReference (
		UPnPControlService* service
	)
{
	service->refCount++;
	_UPnP_ControlDeviceAddReference(service->device);
}

/*---------------------------------------------------------------------------*/
void _UPnP_ControlServiceRemoveReference (
		UPnPControlService* service
	)
{
	UPnPControlDevice* device = service->device;

	UPNP_ASSERT(service->refCount > 0, "No reference to remove");
	service->refCount--;

	_UPnP_ControlDeviceRemoveReference(device);
}

/*---------------------------------------------------------------------------*/
int _UPnP_ControlDescribeDevice (
		UPnPControlDevice* device,
		RTPXML_Document* xmlDescDoc,
		URLDescriptor* descURL
	)
{
	device->descDoc = xmlDescDoc;

	if (xmlDescDoc)
	{
		UPNP_CHAR* urlBase = UPnP_DOMGetElemTextInDoc(device->descDoc, "URLBase", 0);
        if ((urlBase && URL_Parse(&device->baseURL, urlBase, URL_PROTOCOL_HTTP, device->controlPoint->runtime->ipType) >= 0) ||
		    (!urlBase && URL_Copy(&device->baseURL, descURL) >= 0))
		{
			device->deviceElement = UPnP_DOMGetElemInDoc(device->descDoc, "device", 0);
			if (device->deviceElement)
			{
				UPnPControlService* service;
				int result = 0;
				int n = 0;
				RTPXML_Element* serviceElement = UPnP_DOMGetElemInDoc (
						device->descDoc,
						"service",
						n
					);

				while (serviceElement)
				{
					service = _UPnP_ControlNewService(device->controlPoint, device, serviceElement);

					if (!service)
					{
						result = -1;
						break;
					}

					n++;

					serviceElement = UPnP_DOMGetElemInDoc (
							device->descDoc,
							"service",
							n
						);
				}

				if (result >= 0)
				{
					return (result);
				}
			}

			URL_Free(&device->baseURL);
		}
	}

	return (-1);
}
