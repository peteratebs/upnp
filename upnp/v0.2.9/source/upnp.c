//
// UPNP.C -
//
// EBS -
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "upnp.h"
#include "upnpdom.h"
#include "dllist.h"
#include "deviceDiscover.h"
#include "deviceDescribe.h"
#include "deviceControl.h"
#include "deviceEvent.h"
#include "rtpstdup.h"
#include "rtpstr.h"
#include "rtpthrd.h"
#include "rtpmem.h"
#include "rtptime.h"
#include "rtpprint.h"
#include "httpsrv.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/
typedef struct s_UPnPXmlFile
{
 #ifdef UPNP_MULTITHREAD
	RTP_MUTEX mutex;
 #endif
	RTPXML_Document *document;
}
UPnPXmlFile;

/*****************************************************************************/
// Data
/*****************************************************************************/

const HTTP_CHAR UPNP_SERVER_NAME[] =
		"RT-Platform/1.0 UPnP/1.0 EBS MicroWeb-Pro";

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

void _UPnP_Daemon (void *data);

static int  _setArg (RTPXML_Document *doc,
                     const UPNP_CHAR *name,
                     const UPNP_CHAR *value);

static UPnPXmlFile * _UPnP_AllocXmlFile (void);
static void          _UPnP_FreeXmlFile  (UPnPXmlFile *p);
static void releaseMutex(void *p);

static int UPnP_SSDPCallback (SSDPServerContext* ctx,
                              SSDPServerRequest* request,
                              void* cookie);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
  UPnP_RuntimeInit
 *----------------------------------------------------------------------------*/
/**
	@memo Initialize a UPnPRuntime

	@doc
		Initializes the given \Ref{UPnPRuntime} struct, and sets up an HTTP
		server instance to receive control/event messages.  This function
		must be called before any other function in the UPnP SDK.

    @return error code
 */
int UPnP_RuntimeInit (
		UPnPRuntime* rt,             /** pointer to uninitialized \Ref{UPnPRuntime} struct */
		UPNP_UINT8* serverAddr,      /** ip address to bind HTTP server to (NULL for IP_ADDR_ANY) */
		UPNP_UINT16 serverPort,      /** port to bind HTTP server to (0 for ANY_PORT) */
		UPNP_INT16 ipType,           /** type of ip version used (ipv4 or ipv6), (RTP_NET_TYPE_IPV4 for v4 and RTP_NET_TYPE_IPV6 for v6) */
		UPNP_CHAR* wwwRootDir,       /** HTTP root dir on local file system */
		UPNP_INT16 maxConnections,   /** the maximum limit on simultaneous HTTP server connections */
		UPNP_INT16 maxHelperThreads  /** if UPNP_MULTITHREAD is defined, the max number of helper threads to spawn */
	)
{
	HTTPServerConnection* connectCtxArray;
	rt->ipType = ipType;

	connectCtxArray = (HTTPServerConnection*) rtp_malloc(sizeof(HTTPServerConnection) * maxConnections);
	if (connectCtxArray)
	{
		if (HTTP_ServerInit (
				&rt->httpServer,
				UPNP_SERVER_NAME, // name
				wwwRootDir,       // rootDir
				"index.html",     // defaultFile
				1,                // httpMajorVersion
				1,                // httpMinorVersion
				serverAddr,       // ipAddr
				serverPort,       // port,
				ipType,           // ipversion type(4 or 6)
				0,                // allowKeepAlive
				connectCtxArray,  // connectCtxArray
				maxConnections,   // maxConnections
				maxHelperThreads  // maxHelperThreads
			) >= 0)
		{
			if (SSDP_ServerInit (
					&rt->ssdpContext,
                    serverAddr,
					ipType,
                    "RTPlatform/1.0, UPnP/1.0, EBS UPnPSDK/2.9",
                    UPnP_SSDPCallback,
                    rt
				) >= 0)
			{
			 #ifdef UPNP_MULTITHREAD
                if (rtp_sig_mutex_alloc(&rt->mutex, 0) >= 0)
				{
					rt->daemonState = UPNP_DAEMON_STOPPED;
             #endif /* UPNP_MULTITHREAD */

                    rt->deviceRuntime = 0;
					rt->controlPoint = 0;
					return (0);

             #ifdef UPNP_MULTITHREAD
				}
             #endif /* UPNP_MULTITHREAD */

				SSDP_ServerDestroy(&rt->ssdpContext);
			}

			HTTP_ServerDestroy(&rt->httpServer, &connectCtxArray);
		}

		rtp_free(connectCtxArray);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
  UPnP_RuntimeDestroy
 *----------------------------------------------------------------------------*/
/**
	@memo Destroy a UPnPRuntime

	@doc
		Must be called after all other UPnP SDK calls to clean up runtime
		data for UPnP.

    @return error code
 */
void UPnP_RuntimeDestroy (
		UPnPRuntime* rt   /** pointer to \Ref{UPnPRuntime} struct */
	)
{
	HTTPServerConnection* connectCtxArray;

	SSDP_ServerDestroy(&rt->ssdpContext);
	HTTP_ServerDestroy(&rt->httpServer, &connectCtxArray);
	rtp_free(connectCtxArray);
}


/*----------------------------------------------------------------------------*
  UPnP_AddVirtualFile
 *----------------------------------------------------------------------------*/
/**
	@memo Create a virtual file on the HTTP server.

	@doc
		Makes the data buffer passed in available at the given path on the
		HTTP server.

	@see UPnP_RemoveVirtualFile

    @return error code
 */
int  UPnP_AddVirtualFile (
		UPnPRuntime* rt,
        const UPNP_CHAR* serverPath,
        const UPNP_UINT8* data,
        UPNP_INT32 size,
        const UPNP_CHAR* contentType
	)
{
	return (HTTP_ServerAddVirtualFile (
			&rt->httpServer,
			serverPath,
			data,
			size,
			contentType
		));
}

/*----------------------------------------------------------------------------*
  UPnP_RemoveVirtualFile
 *----------------------------------------------------------------------------*/
/**
	@memo Remove a virtual file from the server

	@doc
		Must be called before UPnP_RuntimeDestroy to remove any virtual files
		added using \Ref{UPnP_AddVirtualFile}.

    @return error code
 */
int  UPnP_RemoveVirtualFile (
		UPnPRuntime* rt,
		const UPNP_CHAR *serverPath
	)
{
	return (HTTP_ServerRemovePath(&rt->httpServer, serverPath));
}


RTP_INT32 _UPnP_UpdateTimeout (RTP_INT32 timeout, RTP_INT32 newValue)
{
	if (timeout == RTP_TIMEOUT_INFINITE ||
	    (newValue != RTP_TIMEOUT_INFINITE &&
	     newValue < timeout))
	{
		return (newValue);
	}
	return (timeout);
}

/*----------------------------------------------------------------------------*
                               UPnP_ProcessState
 *----------------------------------------------------------------------------*/
int  _UPnP_ProcessState (
		UPnPRuntime* rt,       /** pointer to \Ref{UPnPRuntime} struct */
		UPNP_INT32 msecTimeout /** time in miliseconds for which the function blocks */
	)
{
	RTP_FD_SET readList;
	RTP_FD_SET writeList;
	RTP_FD_SET errList;
	RTP_INT32 timeout;
	RTP_UINT32 currentTimeSec;

	/* reset the select lists */
	rtp_fd_zero(&readList);
	rtp_fd_zero(&writeList);
	rtp_fd_zero(&errList);

	currentTimeSec = rtp_get_system_sec();

	/* --- DEVICE Announcements ------------------------------ */
	if (rt->deviceRuntime)
	{
		timeout = rt->deviceRuntime->nextAnnounceTimeSec - currentTimeSec;
		if (timeout < 0)
		{
			timeout = 0;
		}
	}
	else
	{
		timeout = msecTimeout;
	}

	/* --- SSDP ---------------------------------------------- */
	timeout = _UPnP_UpdateTimeout(
			SSDP_ServerAddToSelectList (
					&rt->ssdpContext,
					&readList,
					&writeList,
					&errList
				),
			timeout
		);

	/* --- HTTP Server --------------------------------------- */
	rtp_fd_set(&readList, HTTP_ServerGetSocket(&rt->httpServer));

	/* --- CONTROL POINT-------------------------------------- */
	if (rt->controlPoint)
	{
	  	UPNP_RUNTIME_EXIT_CRITICAL(rt);

		timeout = _UPnP_UpdateTimeout(
				rtp_net_sm_add_to_select_list (
						&rt->controlPoint->requestManager,
						&readList,
						&writeList,
						&errList
					),
				timeout
			);
	}
	else
	{
  		UPNP_RUNTIME_EXIT_CRITICAL(rt);
  	}

	rtp_net_select(&readList, &writeList, &errList, timeout);

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);

	/* --- SSDP ---------------------------------------------- */
	SSDP_ServerProcessState (
			&rt->ssdpContext,
			&readList,
			&writeList,
			&errList
		);

	/* --- HTTP Server --------------------------------------- */
	if (rtp_fd_isset(&readList, HTTP_ServerGetSocket(&rt->httpServer)))
	{
	  	UPNP_RUNTIME_EXIT_CRITICAL(rt);
		HTTP_ServerProcessOneRequest(&rt->httpServer, 0);
	  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
	}

	/* --- CONTROL POINT-------------------------------------- */
	if (rt->controlPoint)
	{
	  	UPNP_RUNTIME_EXIT_CRITICAL(rt);

		rtp_net_sm_process_state (
				&rt->controlPoint->requestManager,
				&readList,
				&writeList,
				&errList
			);

	  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
	}

	/* --- DEVICE Announcements ------------------------------ */
	if (rt->deviceRuntime)
	{
		currentTimeSec = rtp_get_system_sec();

		if ((long)(currentTimeSec - rt->deviceRuntime->nextAnnounceTimeSec) >= 0)
		{
			rt->deviceRuntime->nextAnnounceTimeSec = currentTimeSec + rt->deviceRuntime->announceFrequencySec;
			rt->deviceRuntime->announceAll(rt->deviceRuntime);
		}
	}

	return (0);
}

/*----------------------------------------------------------------------------*
                               UPnP_ProcessState
 *----------------------------------------------------------------------------*/
/**
	@memo Process asynchronous operations in non-threaded mode.

	@doc
		This function blocks for at most msecTimeout milliseconds, processing
		any asynchronous operations that may be in progress on either the
		control point or device runtime attached to the given \Ref{UPnPRuntime}.

		This function must be called in order to receive events if an
		application is running with the UPnP SDK in single-threaded mode.

    @return error code
 */

int  UPnP_ProcessState (
		UPnPRuntime* rt,       /** pointer to \Ref{UPnPRuntime} struct */
		UPNP_INT32 msecTimeout /** time in miliseconds for which the function blocks */
	)
{
	int result;

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);

	result = _UPnP_ProcessState(rt, msecTimeout);

  	UPNP_RUNTIME_EXIT_CRITICAL(rt);

  	return (result);
}

/*----------------------------------------------------------------------------*
                            UPnP_StartDaemon
 *----------------------------------------------------------------------------*/
/**
	@memo Start the UPnP Daemon thread.

	@doc
		This function must be called in multithreaded mode to start the
		UPnP daemon, which listens for requests/announcements on the network,
		and sends any events to the attached control point/device runtime.

	@see UPnP_StopDaemon

    @return error code
 */
int  UPnP_StartDaemon (
		UPnPRuntime *rt /** pointer to \Ref{UPnPRuntime} struct */
	)
{
 #ifdef UPNP_MULTITHREAD
	RTP_THREAD threadHandle;

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);

	if (rt->daemonState == UPNP_DAEMON_STOPPED)
	{
		if (rtp_thread_spawn (
				&threadHandle,
				_UPnP_Daemon,
				0, /* thread name */
				0, /* stack size (0 == normal) */
				0, /* priority (0 == normal) */
				rt
			) >= 0)
		{
			rt->daemonState = UPNP_DAEMON_RUNNING;
		  	UPNP_RUNTIME_EXIT_CRITICAL(rt);
			return (0);
		}
	}

  	UPNP_RUNTIME_EXIT_CRITICAL(rt);
 #endif

	return (-1);
}

/*----------------------------------------------------------------------------*
                            UPnP_DeviceStopDaemon
 *----------------------------------------------------------------------------*/
/**
	@memo Kill the UPnP Daemon thread.

	@doc
		This function stops the UPnP daemon from executing.  It will wait
		for at most secTimeout seconds for all helper threads to terminate.
		If this function returns negative error code, it means the timeout
		expired without the successful termination of one or more helper
		threads.  In this case, calling \Ref{UPnP_RuntimeDestroy} may cause a fault
		since there are still helper threads running that may try to access
		the data structures pointed to by the \Ref{UPnPRuntime}.

    @return error code
 */
int  UPnP_StopDaemon (
		UPnPRuntime* rt          /** the device runtime to stop */,
		UPNP_INT32 secTimeout    /** time to wait for daemon to stop */
	)
{
 #ifdef UPNP_MULTITHREAD

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
	rt->daemonState = UPNP_DAEMON_STOPPING;
  	UPNP_RUNTIME_EXIT_CRITICAL(rt);

	if (secTimeout > 0)
	{
		int secsElapsed = 0;

	  	UPNP_RUNTIME_ENTER_CRITICAL(rt);

		/* wait for all the helper threads to exit */
		while (rt->daemonState != UPNP_DAEMON_STOPPED &&
		       secsElapsed < secTimeout)
		{
		  	UPNP_RUNTIME_EXIT_CRITICAL(rt);

			rtp_thread_sleep(1000);
			secsElapsed++;

		  	UPNP_RUNTIME_ENTER_CRITICAL(rt);
		}

	  	UPNP_RUNTIME_EXIT_CRITICAL(rt);
	}

 #endif

	return (0);
}

/*----------------------------------------------------------------------------*
  UPnP_GetPropertyValueByName
 *----------------------------------------------------------------------------*/
/**
	@memo Get the value of a named property in a GENA notify message.

	@doc
		The string returned must not be modified in any way.  It is valid until
		the RTPXML_Document is deleted.

    @return the value or NULL if the property was not found
 */
const UPNP_CHAR* UPnP_GetPropertyValueByName (
		RTPXML_Document* propertySet,	/* address of xml property */
		const UPNP_CHAR* name
	)
{
	return (UPnP_DOMGetElemTextInDoc (propertySet, (char *) name, 0));
}

/*----------------------------------------------------------------------------*
  UPnP_GetPropertyNameByIndex
 *----------------------------------------------------------------------------*/
/**
	@memo Get the name of the nth property.

	@doc
		The string returned must not be modified in any way.  It is valid until
		the RTPXML_Document is deleted.

    @return the value or NULL if the property was not found
 */
const UPNP_CHAR* UPnP_GetPropertyNameByIndex (
		RTPXML_Document* propertySet,	/** address of xml property set */
		int index                   /** index in property for value */
	)
{
	const UPNP_CHAR* str = 0;
	RTPXML_Element* propertyElem;

	propertyElem = UPnP_DOMGetElemInDocNS (propertySet, "property", "urn:schemas-upnp-org:event-1-0", 0);
	if (propertyElem)
	{
		RTPXML_NodeList* children = rtpxmlNode_getChildNodes((RTPXML_Node*) propertyElem);

		if (children)
		{
			RTPXML_Node* node = rtpxmlNodeList_item(children, index);
			if (node && rtpxmlNode_getNodeType(node) == rtpxeELEMENT_NODE)
			{
				str = rtpxmlElement_getTagName((RTPXML_Element*) node);
			}
			rtpxmlNodeList_free(children);
		}
	}

	return (str);
}

/*----------------------------------------------------------------------------*
  UPnP_GetPropertyValueByIndex
 *----------------------------------------------------------------------------*/
/**
	@memo Get the value of the nth property.

	@doc
		The string returned must not be modified in any way.  It is valid until
		the RTPXML_Document is deleted.

    @return the value or NULL if the property was not found
 */
const UPNP_CHAR* UPnP_GetPropertyValueByIndex (
		RTPXML_Document* propertySet, /** address of xml property set */
		int index                   /** index in property for value */
	)
{
	const UPNP_CHAR* propName = UPnP_GetPropertyNameByIndex(propertySet, index);
	if (propName)
	{
		return UPnP_GetPropertyValueByName(propertySet, propName);
	}
	return 0;
}


#ifdef UPNP_MULTITHREAD
/*----------------------------------------------------------------------------*
                            _UPnP_Daemon
 *----------------------------------------------------------------------------*/
void _UPnP_Daemon (void *data)
{
	UPnPRuntime *rt = (UPnPRuntime *) data;

  	UPNP_RUNTIME_ENTER_CRITICAL(rt);

	while (rt->daemonState == UPNP_DAEMON_RUNNING)
	{
		_UPnP_ProcessState(rt, 1000);
	}

	rt->daemonState = UPNP_DAEMON_STOPPED;

 	UPNP_RUNTIME_EXIT_CRITICAL(rt);
}
#endif /* UPNP_MULTITHREAD */


/*----------------------------------------------------------------------------*
                            UPnP_AddToPropertySet
 *----------------------------------------------------------------------------*/
/**
	@memo Add name and value pair to GENA notify message property set.

	@doc
		Add a new name value pair entry to the property set

    @return error code
 */
int  UPnP_AddToPropertySet (
		RTPXML_Document **doc,   /** address of property set */
        const UPNP_CHAR *name, /** pointer to name for new entry */
        const UPNP_CHAR *value /** address of value of for the new entry */
	)
{
RTPXML_NodeList *nodeList;

	if (!doc)
	{
		return (-1);
	}

	if (!*doc)
	{
		/* if the document does not already exist, create it */
		*doc = rtpxmlParseBuffer("<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">"
							   "</e:propertyset>");

		if (!*doc)
		{
			return (-1);
		}
	}

	nodeList = rtpxmlDocument_getElementsByTagNameNS (*doc,
					"urn:schemas-upnp-org:event-1-0",
					"propertyset");

	if (nodeList)
	{
		RTPXML_Element *propertySetElement;

		propertySetElement = (RTPXML_Element *) rtpxmlNodeList_item(nodeList, 0);
		rtpxmlNodeList_free(nodeList);

		if (propertySetElement)
		{
		    RTPXML_Element *propertyElement = rtpxmlDocument_createElement(*doc, "e:property");
		    RTPXML_Element *nameElement = rtpxmlDocument_createElement(*doc, (char *) name);

			if (nameElement && propertyElement)
			{
			    if (value)
			    {
			        RTPXML_Node *valueTextNode = rtpxmlDocument_createTextNode(*doc, (char *) value);
			        if (valueTextNode)
			        {
			        	rtpxmlNode_appendChild((RTPXML_Node *) nameElement, valueTextNode);
			        }
			    }

		        rtpxmlNode_appendChild((RTPXML_Node *) propertyElement,
		                             (RTPXML_Node *) nameElement);

		        rtpxmlNode_appendChild((RTPXML_Node *) propertySetElement,
		                             (RTPXML_Node *) propertyElement);

				return (0);
			}
		}
	}

	return (-1);
}


/*----------------------------------------------------------------------------*
                         UPnP_CreateActionResponse
 *----------------------------------------------------------------------------*/
/**
	@memo Creates a SOAP action response message.

	@doc
		Creates a response message skeleton for the supplied SOAP action request

    @return error code
 */
int UPnP_CreateActionResponse (
		UPnPActionRequest *request /* pointer to action request */
	)
{
char temp[256];
int result = -1;

	if (request->in.actionName && request->in.serviceTypeURI &&
	    (rtp_strlen(request->in.actionName) * 2 + rtp_strlen(request->in.serviceTypeURI) < 200))
	{
		rtp_sprintf(temp, "<u:%sResponse xmlns:u=\"%s\"></u:%sResponse>",
		           request->in.actionName,
		           request->in.serviceTypeURI,
		           request->in.actionName);

		request->out.result = rtpxmlParseBuffer(temp);
		if(request->out.result)
		{
			result = 0;
		}
	}

	return (result);
}

/*----------------------------------------------------------------------------*
                            UPnP_CreateAction
 *----------------------------------------------------------------------------*/
/**
	@memo Create a SOAP action request.

	@doc
		Creates an XML document which will hold the SOAP action request
		message. This function returns the address of newly formed XML document.
		After finishing the process of sending action request the application
		must release this xml document.

    @return pointer to newly created RTPXML_Document, which can be passed into
    	\Ref{UPnP_SetActionArg} to set the action arguments; NULL on error
 */
RTPXML_Document * UPnP_CreateAction (
		const UPNP_CHAR *serviceTypeURI, /** string containing service type of the target service */
		const UPNP_CHAR *actionName      /** name on action on the target service */
	)
{
char temp[256];

	if (actionName && serviceTypeURI &&
	    (rtp_strlen(actionName) * 2 + rtp_strlen(serviceTypeURI) < 216))
	{
		rtp_sprintf(temp, "<u:%s xmlns:u=\"%s\"></u:%s>",
		           actionName,
		           serviceTypeURI,
		           actionName);

		return (rtpxmlParseBuffer(temp));
	}

	return (0);
}

/*----------------------------------------------------------------------------*
                              UPnP_SetActionArg
 *----------------------------------------------------------------------------*/
/**
	@memo Set an argument for a SOAP action response/request

	@doc
		This function can be used on an RTPXML_Document created by either
		\Ref{UPnP_CreateActionResponse} or \Ref{UPnP_CreateAction} to set
		either the input or output arguments for a SOAP action.

    @return error code
 */
int UPnP_SetActionArg (
		RTPXML_Document *actionDoc, /** pointer to action respose message */
		const UPNP_CHAR *name,    /** argument name **/
		const UPNP_CHAR *value    /** argument value **/
	)
{
	return (_setArg(actionDoc, name, value));
}

/*----------------------------------------------------------------------------*
                                    _setArg
 *----------------------------------------------------------------------------*/
/*

    @return error code
 */
int  _setArg  (RTPXML_Document *doc,
               const UPNP_CHAR *name,
               const UPNP_CHAR *value)
{
	RTPXML_Node *responseNode = rtpxmlNode_getFirstChild((RTPXML_Node *) doc);

	if (responseNode)
	{
	    RTPXML_Element *nameElement = rtpxmlDocument_createElement(doc, (char *) name);

		if (nameElement)
		{
		    if (value)
		    {
		        RTPXML_Node *valueTextNode = rtpxmlDocument_createTextNode(doc, (char *) value);
		        if (valueTextNode)
		        {
		        	rtpxmlNode_appendChild((RTPXML_Node *) nameElement, valueTextNode);
		        }
		    }

	        rtpxmlNode_appendChild(responseNode, (RTPXML_Node *) nameElement);

			return (0);
		}
	}

	return (-1);
}


/*---------------------------------------------------------------------------*/
void releaseMutex(void *p)
{
	rtp_sig_mutex_release(*((RTP_MUTEX *) p));
}

/*---------------------------------------------------------------------------*/
int UPnP_SSDPCallback (SSDPServerContext* ctx,
                       SSDPServerRequest* request,
                       void* cookie)
{
	UPnPRuntime* rt = (UPnPRuntime*) cookie;

	switch (request->type)
	{
		case SSDP_SERVER_REQUEST_M_SEARCH:
			/* let the device runtime handle it */
			if (rt->deviceRuntime)
			{
				return (rt->deviceRuntime->deviceSSDPCallback(ctx, request, rt->deviceRuntime));
			}
			break;

		case SSDP_SERVER_REQUEST_NOTIFY:
			/* let the control point handle it */
			if (rt->controlPoint)
			{
				int result;

				UPNP_RUNTIME_EXIT_CRITICAL(rt);
				result =  rt->controlPoint->controlSSDPCallback(ctx, request, rt->controlPoint);
				UPNP_RUNTIME_ENTER_CRITICAL(rt);

				return (result);
			}
			break;
	}

	return (0);
}
