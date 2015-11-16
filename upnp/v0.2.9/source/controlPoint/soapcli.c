//
// SOAP.C -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "soapcli.h"
#include "rtpmem.h"
#include "rtpprint.h"
#include "rtpstr.h"
#include "httpmcli.h"
#include "rtpxml.h"
#include "upnpdom.h"
#include "urlparse.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define SOAP_HTTP_BUFFER_LEN           1024
#define SOAP_RESPONSE_BUFFER_LENGTH    100
#define SOAP_ACTION_ENVELOPE_LENGTH    256
#define SOAP_SELECT_LIST_TIMEOUTMSEC   100
#define SOAP_ACTION_REQUEST_TEMPLATE   \
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"\
    "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\""\
			" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"\
	"<s:Body>\r\n"\
	"%s"\
	"</s:Body>\r\n"\
	"</s:Envelope>\r\n"

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/
SOAPActionState _SOAP_ParseActionRespose (
		HTTPResponseInfo*         info,
		HTTPManagedClientSession* httpSession,
		SOAPActionResponse*       response
	);

SOAP_INT32 SOAP_SendActionRequest (
		SOAPAction* action
	);

SOAP_INT32 SOAP_ReadActionResponse (
		SOAPAction* action
	);

SOAP_INT32 SOAP_CreateHttpSession (
		SOAPAction* action
	);

/*----------------------------------------------------------------------------*
 *  					SOAP_ActionInit										  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc::: this will be called each time a soap action is to be performed. This will
        open a http client, and send an POST, the request structure is filled and
        send to be upnp client manager to add the request to its list.

    @return error code
 */

SOAP_INT32 SOAP_ActionInit (
		SOAPAction* action,
		HTTPManagedClient* httpClient,
		SOAP_INT16 ipType,
		const SOAP_CHAR* destUri,
		const SOAP_CHAR* soapAction,
		SOAP_CHAR* headerStr,
		SOAP_INT32 headerLen,
		SOAP_CHAR* bodyStr,
		SOAP_INT32 bodyLen,
		SOAPActionCallback callbackFn,
		void* callbackData
	)
{
    action->state = SOAP_ACTION_CONNECTING_POST;
	action->httpClient = httpClient;
	action->headerStr = headerStr;
	action->headerLen = headerLen;
	action->bodyStr	= bodyStr;
	action->bodyLen = bodyLen;
	action->callbackFn = callbackFn;
	action->callbackData = callbackData;

	rtp_strncpy(action->soapAction, soapAction, SOAP_ACTION_MAX_LEN-1);
	action->soapAction[SOAP_ACTION_MAX_LEN-1] = 0;

	if (URL_Parse(&action->postTargetURL, destUri, URL_PROTOCOL_HTTP, ipType) >= 0)
	{
		if (SOAP_CreateHttpSession(action) >= 0)
		{
			return (0);
		}

		URL_Free(&action->postTargetURL);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					SOAP_ActionInit										  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc::: this will be called each time a soap action is to be performed. This will
        open a http client, and send an POST, the request structure is filled and
        send to be upnp client manager to add the request to its list.

    @return error code
 */

SOAP_INT32 SOAP_ActionInitEx (
		SOAPAction* action,
		HTTPManagedClient* httpClient,
		SOAP_INT16 ipType,
		URLDescriptor* postURL,
		URLDescriptor* baseURL,
		const SOAP_CHAR* soapAction,
		SOAP_CHAR* headerStr,
		SOAP_INT32 headerLen,
		SOAP_CHAR* bodyStr,
		SOAP_INT32 bodyLen,
		SOAPActionCallback callbackFn,
		void* callbackData
	)
{
    action->state = SOAP_ACTION_CONNECTING_POST;
	action->httpClient = httpClient;
	action->ipType = ipType;
	action->headerStr = headerStr;
	action->headerLen = headerLen;
	action->bodyStr	= bodyStr;
	action->bodyLen = bodyLen;
	action->callbackFn = callbackFn;
	action->callbackData = callbackData;

	rtp_strncpy(action->soapAction, soapAction, SOAP_ACTION_MAX_LEN-1);
	action->soapAction[SOAP_ACTION_MAX_LEN-1] = 0;

	if (URL_GetAbsolute(&action->postTargetURL, postURL, baseURL, action->ipType) >= 0)
	{
		if (SOAP_CreateHttpSession(action) >= 0)
		{
			return (0);
		}

		URL_Free(&action->postTargetURL);
	}

	return (-1);
}
/*----------------------------------------------------------------------------*
 *  					SOAP_CreateHttpSession   							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
SOAP_INT32 SOAP_CreateHttpSession (
		SOAPAction* action
	)
{
    /* Create a new http session for action request */
	if (HTTP_ManagedClientStartTransaction (
			action->httpClient,
			action->postTargetURL.field.http.host,
			action->postTargetURL.field.http.port,
            action->ipType,
			HTTP_SESSION_TYPE_TCP,
			1, /* non - blocking */
			&action->httpSession) >= 0)
	{
		return (0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					SOAP_ActionDestroy									  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
close the session ::
    @return error code
 */
void SOAP_ActionDestroy (
		SOAPAction* action
	)
{
	if (action->postTargetURL.buffer)
    {
        rtp_free(action->postTargetURL.buffer);
    }

    if (action->httpSession)
	{
		HTTP_ManagedClientCloseSession(action->httpSession);
		action->httpSession = 0;
	}
}

/*----------------------------------------------------------------------------*
 *  					SOAP_ActionExecute										  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
SOAP_INT32 SOAP_ActionExecute (
		SOAPAction* action
	)
{
	SOAP_BOOL  done = 0;
	RTP_FD_SET readList;
	RTP_FD_SET writeList;
	RTP_FD_SET errList;
	SOAP_INT32 timeoutMsec;

	while (!done)
	{
		rtp_fd_zero(&readList);
		rtp_fd_zero(&writeList);
		rtp_fd_zero(&errList);

		timeoutMsec = SOAP_ActionAddToSelectList(action, &readList, &writeList, &errList);

		rtp_net_select(&readList, &writeList, &errList, timeoutMsec);

		done = SOAP_ActionProcessState(action, &readList, &writeList, &errList);
	}

	if (action->state == SOAP_ACTION_RESPONSE_READ)
	{
		return (0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					SOAP_ActionAddToSelectList							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc:: add to select list, and return timeout milliseconds to select for
    @return error code
 */
SOAP_INT32 SOAP_ActionAddToSelectList (
		SOAPAction* action,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
	SOAP_INT32 timeoutMsec = SOAP_SELECT_LIST_TIMEOUTMSEC;
    /* extract the socket information from the action request */
	RTP_SOCKET actionSocket = HTTP_ManagedClientGetSessionSocket (
		action->httpSession);

	switch(action->state)
	{
		case SOAP_ACTION_CONNECTING_POST:
			rtp_fd_set (writeList, actionSocket);
			break;

		case SOAP_ACTION_READING_POST_RESPONSE:
			rtp_fd_set (readList, actionSocket);
			break;

		case SOAP_ACTION_CONNECTING_M_POST:
			rtp_fd_set (writeList, actionSocket);
			break;

		case SOAP_ACTION_READING_M_POST_RESPONSE:
			rtp_fd_set (readList, actionSocket);
			break;
	}
	return(timeoutMsec);
}


/*----------------------------------------------------------------------------*
 *  					SOAP_ActionProcessState								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */
SOAP_BOOL SOAP_ActionProcessState (
		SOAPAction* action,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
    SOAP_BOOL done = 0;
    /* extract the socket information from the action request */
	RTP_SOCKET actionSocket = HTTP_ManagedClientGetSessionSocket (
		action->httpSession);

	/* if socket is in error state set the state to reflect this error */
	if(rtp_fd_isset (errList, actionSocket) != 0)
	{
		action->state = SOAP_ACTION_ERROR;
		return(done);
	}
	switch(action->state)
	{
		case SOAP_ACTION_CONNECTING_POST:
			if (rtp_fd_isset (writeList, actionSocket) != 0)
			{
				rtp_fd_clr (writeList, actionSocket);
				if (SOAP_SendActionRequest (action) >= 0)
				{
					action->state = SOAP_ACTION_READING_POST_RESPONSE;
				}
			}
			break;

		case SOAP_ACTION_READING_POST_RESPONSE:
			if(rtp_fd_isset (readList, actionSocket) != 0)
			{
				/* action socket is ready to be read from */
				/* remove the socket from the list */
				/* extract and process action response */
				rtp_fd_clr (readList, actionSocket);
				SOAP_ReadActionResponse (action);
			}
			break;

		case SOAP_ACTION_CONNECTING_M_POST:
			if(rtp_fd_isset (writeList, actionSocket) != 0)
			{
				rtp_fd_clr (writeList, actionSocket);
				if(SOAP_SendActionRequest (action) >= 0)
				{
					action->state = SOAP_ACTION_READING_M_POST_RESPONSE;
				}
			}
			break;

		case SOAP_ACTION_READING_M_POST_RESPONSE:
			if(rtp_fd_isset (readList, actionSocket) != 0)
			{
				/* action socket is ready to be read from */
				/* remove the socket from the list */
				/* extract and process action response */
				rtp_fd_clr (readList, actionSocket);
				SOAP_ReadActionResponse (action);
			}
			break;

		case SOAP_ACTION_RESPONSE_READ:
			done = 1;
			break;
	}


	if (action->state == SOAP_ACTION_ERROR)
	{
		done = 1;
	}
	return(done);
}


/*----------------------------------------------------------------------------*
 *  					SOAP_SendActionRequest								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
#define USE_EXPERIMENTAL_REQUEST 1 /* Needed to define in htptpmcli.c */
#if (USE_EXPERIMENTAL_REQUEST)
 int HTTP_ManagedClientRequestExExperimental (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince,
		HTTP_CHAR* contentType,
		HTTP_INT32 *contentLength,
		HTTPRequest* request,
        HTTP_CHAR* SOAPAction,
        HTTP_CHAR* soapAction);
 #endif
SOAP_INT32 SOAP_SendActionRequest (
		SOAPAction* action
	)
{
	SOAP_CHAR*    requestBuffer;
	SOAP_INT32    contentLen;
	HTTPRequest   request;

	/* Create soap envelope for the request */
	requestBuffer = (char *)rtp_malloc(action->bodyLen +
						SOAP_ACTION_ENVELOPE_LENGTH);
	if (requestBuffer)
	{
		rtp_sprintf(requestBuffer, SOAP_ACTION_REQUEST_TEMPLATE,
					   action->bodyStr);

		/* get the length of the envelope which is the content length of SOAP Request */
		contentLen = rtp_strlen(requestBuffer);

		/* send a Post or a M-Post request */
		switch(action->state)
		{
			case SOAP_ACTION_CONNECTING_POST:
				/* Create soap envelope for the request */
#if (USE_EXPERIMENTAL_REQUEST)
				HTTP_ManagedClientRequestExExperimental(action->httpSession, "POST", action->postTargetURL.field.http.path, NULL,
					"text/xml; charset=\"utf-8\"", &contentLen, &request, "SOAPAction", action->soapAction);
#else
				HTTP_ManagedClientRequestEx (action->httpSession, "POST", action->postTargetURL.field.http.path, NULL,
					"text/xml; charset=\"utf-8\"", contentLen, &request);
#endif
				break;

			case SOAP_ACTION_CONNECTING_M_POST:
				HTTP_ManagedClientRequestEx (action->httpSession, "M-POST", action->postTargetURL.field.http.path, NULL,
					"text/xml; charset=\"utf-8\"", contentLen, &request);
				HTTP_SetRequestHeaderStr (&request, "Man", "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=s");
				HTTP_SetRequestHeaderStr (&request, "s-SOAPAction", action->soapAction);
				break;
		}
		HTTP_ManagedClientRequestHeaderDone (action->httpSession, &request);

		/* write the POST data */
		HTTP_ManagedClientWrite (action->httpSession, (SOAP_UINT8*)requestBuffer, contentLen);

		/* signal that the request is compete */
		HTTP_ManagedClientWriteDone (action->httpSession);

		rtp_free(requestBuffer);

		return(0);
	}

	return (-1);
}


/*----------------------------------------------------------------------------*
 *  					SOAP_ReadActionResponse								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
SOAP_INT32 SOAP_ReadActionResponse (
		SOAPAction* action
	)
{
	HTTPResponseInfo   info;
	SOAPActionResponse response;

	rtp_memset(&response, 0, sizeof(response));

	HTTP_ManagedClientReadResponseInfo(action->httpSession, &info);
	/* If the response to a post request is 'Method Not Allowed' then, resend
       as a M-Post request */
    /* Close http session for Post and open new http session for MPost */
	if(info.status == 405 && action->state == SOAP_ACTION_READING_POST_RESPONSE)
	{
		action->state = SOAP_ACTION_CONNECTING_M_POST;
		HTTP_ManagedClientCloseSession(action->httpSession);
		/* open new http session for M-Post */
		if(SOAP_CreateHttpSession(action) < 0)
		{
			action->state = SOAP_ACTION_ERROR;
			return(-1);
		}
		return(0); /* failure is when mpost fails */
	}

	action->state = _SOAP_ParseActionRespose(
		&info, action->httpSession, &response);

    /* invoke the callback to process the response */
	if (action->callbackFn && action->state == SOAP_ACTION_RESPONSE_READ)
	{
		if(action->callbackFn(action->callbackData,	&response) >= 0)
		{
			if (response.envelope.doc)
            	rtpxmlDocument_free (response.envelope.doc);
            return(0);
		}
	}
	action->state = SOAP_ACTION_ERROR;
	return(-1);
}

/*----------------------------------------------------------------------------*
 *  					_SOAP_ParseActionRespose							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
SOAPActionState _SOAP_ParseActionRespose (
		HTTPResponseInfo* info,
		HTTPManagedClientSession* session,
		SOAPActionResponse* response
	)
{
	SOAP_INT32      result;
	SOAP_INT32      responseBytesRead = 0;
	SOAP_INT32      responseBufferSize = SOAP_RESPONSE_BUFFER_LENGTH;
	RTPXML_NodeList*  elements;
	RTPXML_Element*   envelopeElem;
    SOAP_CHAR* responseBuffer;
	SOAPActionState	state = SOAP_ACTION_ERROR;

	if(info->status == 200) //OK
	{
		response->status = SOAP_RESPONSE_NORMAL;
	}
	else
	{
		response->status = SOAP_RESPONSE_FAULT;
	}

	/* collect the response and store it in responseBuffer */
	/* Create soap envelope for the request */

	responseBuffer = (char *)rtp_malloc(SOAP_RESPONSE_BUFFER_LENGTH);
	if (!responseBuffer)
	{
		return (-1);
	}
	rtp_memset((SOAP_UINT8*)responseBuffer, 0, SOAP_RESPONSE_BUFFER_LENGTH);

	do
	{

		/* if the buffer is full, then add some space to read data into */
		if (responseBytesRead == responseBufferSize)
		{
			SOAP_CHAR* newBuffer = rtp_realloc(
				responseBuffer,
				responseBufferSize + SOAP_RESPONSE_BUFFER_LENGTH
			);
			if (newBuffer)
			{
				responseBuffer = newBuffer;
				responseBufferSize = responseBufferSize + SOAP_RESPONSE_BUFFER_LENGTH;
			}
			else
			{
				break;
			}
		}

		result = HTTP_ManagedClientRead (
				session,
				(SOAP_UINT8*)(responseBuffer + responseBytesRead),
				responseBufferSize - responseBytesRead
			);

		if (result == 0)
		{
			break;
		}

		responseBytesRead += result;
	}
	/* if we filled the buffer, then try to read more data */
	while (result > 0);

	/* If there is nothing after 500 "Description" then point the fault string at the description */
	if (responseBytesRead == 0)
	{
		response->status = SOAP_REQUEST_ERROR;
        response->fault.string = info->httpReply;
		response->fault.code = response->fault.string;
		return(SOAP_ACTION_RESPONSE_READ);
	}


	if (result == 0)
	{
		/* this is OK because we allocated an extra byte above */
		responseBuffer[responseBytesRead] = 0;
		state = SOAP_ACTION_RESPONSE_READ;

        if(rtpxmlParseBufferEx(responseBuffer, &response->envelope.doc) != RTPXML_SUCCESS)
		{
processing_error:
			response->status = SOAP_REQUEST_ERROR;
			response->fault.string = "Soap Action failed but XML error response is badly formed";
			response->fault.code = response->fault.string;
			rtp_free(responseBuffer);
			return(state);
		}

		elements = rtpxmlDocument_getElementsByTagNameNS (
			response->envelope.doc,
			"http://schemas.xmlsoap.org/soap/envelope/",
			"Envelope");
		if (!elements)
		{
			rtpxmlNodeList_free(elements);
			goto processing_error;
		}

		envelopeElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
		rtpxmlNodeList_free(elements);

		if (!envelopeElem)
		{
			goto processing_error;
		}

		elements = rtpxmlElement_getElementsByTagNameNS (
			envelopeElem,
			"http://schemas.xmlsoap.org/soap/envelope/",
			"Body");

        if (elements)
		{
			response->envelope.bodyElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
			rtpxmlNodeList_free(elements);
		}
		else
		{
			response->envelope.bodyElem = 0;
		}

		elements = rtpxmlElement_getElementsByTagNameNS (
			envelopeElem,
			"http://schemas.xmlsoap.org/soap/envelope/",
			"Header");

		if (elements)
		{
				response->envelope.headerElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
				rtpxmlNodeList_free(elements);
		}
		else
		{
			response->envelope.headerElem = 0;
		}

        /* If action response is error, store error description */
        if(response->status == SOAP_RESPONSE_FAULT)
        {
            if (response->envelope.bodyElem)
		    {
                response->fault.code = UPnP_DOMGetElemTextInElem(response->envelope.bodyElem, "faultcode", 0);
				response->fault.string = UPnP_DOMGetElemTextInElem(response->envelope.bodyElem, "faultstring", 0);
				elements = rtpxmlElement_getElementsByTagName (
					response->envelope.bodyElem,
					"detail");
				if (elements)
				{
					response->fault.detailElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
					rtpxmlNodeList_free(elements);
				}
				else
				{
					response->fault.detailElem = 0;
				}
		    }
        }
	}
    rtp_free(responseBuffer);
	return(state);
}
