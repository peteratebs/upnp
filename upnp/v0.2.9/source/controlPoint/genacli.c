//
// GENACLI.C -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:08 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "genacli.h"
#include "urlparse.h"
#include "stdio.h"
#include "rtpscnv.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/
#define GENA_HTTP_BUFFER_LEN          1024
#define GENA_EVENT_STR_LEN            256

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/
GENA_INT32 GENA_CreateHttpSession (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_INT16 ipType
	);

GENA_INT32 GENA_SendRequest (
		GENAClientRequest* request
	);

GENA_INT32 GENA_ReadResponse (
		GENAClientRequest* request
	);

GENA_INT32 _GENA_ReadHeader (
		void*                     userData,
		HTTPSession* 			  session,
		HTTPHeaderType            type,
		const GENA_CHAR*          name,
		const GENA_CHAR*          value
	);

GENA_INT32 _GENA_ProcessHttpHeader (
		void *userData,
		HTTPServerRequestContext *ctx,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value
	);

GENA_INT32 GENA_SendEventResponse (
		GENAClientEvent* genaEvent,
		HTTPServerRequestContext* srv,
		HTTPSession* session,
		HTTPRequest* request
	);


/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
 *  					GENA_ClientInit										  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

GENA_INT32 GENA_ClientInit (
		GENAClientContext* ctx,
		GENAClientCallback callback,
		void* cookie
	)
{
	ctx->userCallback = callback;
	ctx->userData = cookie;
	return (0);
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientDestroy									  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

void GENA_ClientDestroy (
		GENAClientContext* ctx
	)
{
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientProcessEvent								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

GENA_INT32 GENA_ClientProcessEvent (
		GENAClientContext* ctx,
		HTTPServerRequestContext* srv,
		HTTPSession* session,
		HTTPRequest* request,
		RTP_NET_ADDR* notifierAddr
	)
{
	GENAClientEvent genaEvent;
	HTTP_INT32 httpBufferSize = GENA_HTTP_BUFFER_LEN;
	HTTP_UINT8 httpBuffer[GENA_HTTP_BUFFER_LEN];
	HTTP_UINT8 *messageBuffer = 0;

	genaEvent.status = GENA_EVENT_NO_ERROR;

	if(request->methodType == HTTP_METHOD_NOTIFY)
	{
		rtp_memset(&genaEvent, 0, sizeof(GENAClientEvent));

		if(HTTP_ServerReadHeaders (
				srv,
				session,
				(HTTPServerHeaderCallback)_GENA_ProcessHttpHeader,
                                &genaEvent,
				httpBuffer,
				httpBufferSize) >= 0)
		{
			/* if no missing nt, nts or sid header */
			if(genaEvent.ntPresent && genaEvent.ntsPresent &&
				genaEvent.sidPresent)
			{
				if(genaEvent.contentLength > 0)
				{
					messageBuffer = (HTTP_UINT8 *)rtp_malloc
									(genaEvent.contentLength + 1);
					if (messageBuffer)
					{

						if (HTTP_Read(session, messageBuffer,
						 	genaEvent.contentLength) >=
						 	    genaEvent.contentLength)
						{
							int result;
							/* null terminate the message buffer for
						   	rtpxmlParseBuffer */
							messageBuffer[genaEvent.contentLength] = 0;

							result = rtpxmlParseBufferEx((HTTP_INT8*)messageBuffer,
						         	 &genaEvent.propertySetDoc);
							if (result == RTPXML_SUCCESS)
							{
								/* invoke user callback to handle the message */
								if (ctx->userCallback)
								{
									/* if the callback is succesful, the status will reflect */
									/* GENA_EVENT_ERROR and success reply will be send */
									if (ctx->userCallback(ctx, &genaEvent, ctx->userData) < 0)
									{
										/* if the status reflects invalid sid than corresponding */
										/* is send otherwise lag an internal error */
										if(genaEvent.status != GENA_EVENT_ERROR_INVALID_SID)
										{
											/* internal error occured */
											genaEvent.status = GENA_EVENT_ERROR_INTERNAL;
										}
									}
								}
								else
								{
									/* No callback function associated */
									genaEvent.status = GENA_EVENT_ERROR_INTERNAL;
								}
							}
							else
							{
								/* XML parse failed */
								/* send error: internal server error */
								genaEvent.status = GENA_EVENT_ERROR_INTERNAL;
							}

							rtpxmlDocument_free(genaEvent.propertySetDoc);
						}
						else
						{
							/* send error: bad request */
							/* content not equal to content length */
							genaEvent.status = GENA_EVENT_ERROR_BAD_REQUEST;
						}
						rtp_free(messageBuffer);
					}
					else
					{
						/* send error: internal server error */
						/* unable to allocate buffer */
						genaEvent.status = GENA_EVENT_ERROR_INTERNAL;
					}
				}
				else
				{
					/* send error: bad request */
					/* content length is 0 */
					genaEvent.status = GENA_EVENT_ERROR_BAD_REQUEST;
				}
			}
			else
			{
				/* send error: bad request */
				genaEvent.status = GENA_EVENT_ERROR_MISSING_NT_NTS;
			}
		}
		else
		{
			/* send error: internal server error */
			genaEvent.status = GENA_EVENT_ERROR_INTERNAL;
		}

		GENA_SendEventResponse(&genaEvent, srv, session, request);
	}
	return(0);
}

/*----------------------------------------------------------------------------*
 *  					GENA_SendEventResponse								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

GENA_INT32 GENA_SendEventResponse (
		GENAClientEvent* genaEvent,
		HTTPServerRequestContext* srv,
		HTTPSession* session,
		HTTPRequest* request
	)
{
	HTTPResponse response;

	switch(genaEvent->status)
	{
		case GENA_EVENT_NO_ERROR:
			HTTP_ServerInitResponse(srv, session, &response, 200, "OK");
			HTTP_ServerSetDefaultHeaders(srv, &response);
			HTTP_WriteResponse(session, &response);
			HTTP_WriteFlush(session);
			HTTP_FreeResponse(&response);
			break;

		case GENA_EVENT_ERROR_INTERNAL:
			HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
			break;

		case GENA_EVENT_ERROR_INVALID_SID:
		case GENA_EVENT_ERROR_MISSING_SID:
		case GENA_EVENT_ERROR_INVALID_NT:
		case GENA_EVENT_ERROR_INVALID_NTS:
			HTTP_ServerSendError(srv, session, 412, "Precondition Failed");
			break;

		case GENA_EVENT_ERROR_MISSING_NT_NTS:
		case GENA_EVENT_ERROR_BAD_REQUEST:
			HTTP_ServerSendError(srv, session, 400, "Bad Request");
			break;
	}
	return (0);
}

/*----------------------------------------------------------------------------*
 *  					_GENA_ProcessHttpHeader 							  *
 *----------------------------------------------------------------------------*/
GENA_INT32 _GENA_ProcessHttpHeader (void *userData,
                             HTTPServerRequestContext *ctx,
                             HTTPSession *session,
                             HTTPHeaderType type,
                             const HTTP_CHAR *name,
                             const HTTP_CHAR *value)
{
	GENAClientEvent* genaEvent = (GENAClientEvent*) userData;
	switch (type)
	{
		case HTTP_HEADER_NT:
			genaEvent->ntPresent = 1;
			/* if NT value is not upnp:event send an invalid error */
			if(rtp_strcmp(value, "upnp:event"))
			{
				genaEvent->status = GENA_EVENT_ERROR_INVALID_NT;
			}
			break;

		case HTTP_HEADER_NTS:
			genaEvent->ntsPresent = 1;
			/* if NTS value is not upnp:propchange send an invalid error */
			if(rtp_strcmp(value, "upnp:propchange"))
			{
				genaEvent->status = GENA_EVENT_ERROR_INVALID_NTS;
			}
			break;

		case HTTP_HEADER_SID:
			genaEvent->sidPresent = 1;
			rtp_strcpy(genaEvent->sid, value);
			break;

		case HTTP_HEADER_CONTENT_LENGTH:
			genaEvent->contentLength = rtp_atoi(value);
			break;

		case HTTP_UNKNOWN_HEADER_TYPE:
			if (!rtp_strcmp(name, "SEQ"))
			{
				genaEvent->seq = rtp_atoi(value);
			}
			break;
	}
	return (0);
}


/*----------------------------------------------------------------------------*
 *  					GENA_SubscribeRequestInit 							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc::: this will be called each time a GENA request is to be performed. This will
        open a http client, and send an POST, the request structure is filled and
        send to be upnp client manager to add the request to its list.

    @return error code
 */

GENA_INT32 GENA_SubscribeRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_CHAR* callbackUrl,
		GENA_INT32 timeoutSec,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	)
{
	if (GENA_CreateHttpSession(request, httpClient, relUrlStr, baseURL, ipType) >= 0)
	{
	    request->state = GENA_REQUEST_CONNECTING;
	    request->requestType = GENA_REQUEST_SUBSCRIBE;
		request->callbackFn = callbackFn;
		request->callbackData = callbackData;
		request->param.subscribe.callbackUrl = callbackUrl;
		request->param.subscribe.timeoutSec = timeoutSec;

		return(0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					GENA_RenewRequestInit 								  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc::: this will be called each time a GENA request is to be performed. This will
        open a http client, and send an POST, the request structure is filled and
        send to be upnp client manager to add the request to its list.

    @return error code
 */

GENA_INT32 GENA_RenewRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* serverUrl,
		URLDescriptor* baseURL,
		GENA_CHAR* sid,
		GENA_INT32 timeoutSec,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	)
{
    request->state = GENA_REQUEST_CONNECTING;
    request->requestType = GENA_REQUEST_RENEW;
	request->callbackFn = callbackFn;
	request->callbackData = callbackData;
	request->param.renew.sid = sid;
	request->param.renew.timeoutSec = timeoutSec;

	if (GENA_CreateHttpSession(request, httpClient, serverUrl, baseURL, ipType) < 0)
	{
		return(-1);
	}
	return(0);
}

/*----------------------------------------------------------------------------*
 *  					GENA_UnsubscribeRequestInit 						  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc::: this will be called each time a GENA request is to be performed. This will
        open a http client, and send an POST, the request structure is filled and
        send to be upnp client manager to add the request to its list.

    @return error code
 */

GENA_INT32 GENA_UnsubscribeRequestInit (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_INT16 ipType,
		GENA_CHAR* serverUrl,
		URLDescriptor* baseURL,
		GENA_CHAR* sid,
		GENAClientRequestCallback callbackFn,
		void* callbackData
	)
{
    request->state = GENA_REQUEST_CONNECTING;
    request->requestType = GENA_REQUEST_UNSUBSCRIBE;
	request->callbackFn = callbackFn;
	request->callbackData = callbackData;
	request->param.cancel.sid = sid;

	if (GENA_CreateHttpSession(request, httpClient, serverUrl, baseURL, ipType) < 0)
	{
		return(-1);
	}
	return(0);
}


/*----------------------------------------------------------------------------*
 *  					GENA_CreateHttpSession   							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
GENA_INT32 GENA_CreateHttpSession (
		GENAClientRequest* request,
		HTTPManagedClient* httpClient,
		GENA_CHAR* relUrlStr,
		URLDescriptor* baseURL,
		GENA_INT16 ipType
	)
{
	URLDescriptor relEventURL;

	if ((baseURL &&
	     URL_Parse(&relEventURL, relUrlStr, URL_PROTOCOL_HTTP, ipType) >= 0 &&
	     URL_GetAbsolute(&request->eventSubURL, &relEventURL, baseURL,ipType) >= 0) ||
	    (!baseURL &&
	     URL_Parse(&request->eventSubURL, relUrlStr, URL_PROTOCOL_HTTP, ipType) >= 0))
	{
		/* Create a new http session for request */
		if (HTTP_ManagedClientStartTransaction (
				httpClient,
				request->eventSubURL.field.http.host,
				request->eventSubURL.field.http.port,
				ipType,
				HTTP_SESSION_TYPE_TCP,
				1, /* non - blocking */
				&request->httpSession) >= 0)
		{
			if (baseURL)
			{
				URL_Free(&relEventURL);
			}

			return(0);
		}

		if (baseURL)
		{
			URL_Free(&relEventURL);
		}
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientRequestDestroy							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

void GENA_ClientRequestDestroy (
		GENAClientRequest* request
	)
{
	HTTP_ManagedClientCloseSession(request->httpSession);
	URL_Free(&request->eventSubURL);
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientRequestExecute 							  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

GENA_INT32 GENA_ClientRequestExecute (
		GENAClientRequest* request
	)
{
	GENA_BOOL  done = 0;
	RTP_FD_SET readList;
	RTP_FD_SET writeList;
	RTP_FD_SET errList;
	GENA_INT32 timeoutMsec;

	while (!done)
	{
		rtp_fd_zero(&readList);
		rtp_fd_zero(&writeList);
		rtp_fd_zero(&errList);

		timeoutMsec = GENA_ClientRequestAddToSelectList(request, &readList, &writeList, &errList);

		rtp_net_select(&readList, &writeList, &errList, timeoutMsec);

		done = GENA_ClientRequestProcessState(request, &readList, &writeList, &errList);
	}

	if (request->state == GENA_REQUEST_ACCEPTED)
	{
		return (0);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientRequestAddToSelectList					  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
desc:: add to select list, and return timeout milliseconds to select for
    @return error code
 */

GENA_INT32 GENA_ClientRequestAddToSelectList (
		GENAClientRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
	HTTPManagedClientSession* httpSession = request->httpSession;
    /* extract the socket information from the request */
	RTP_SOCKET genaSocket = HTTP_ManagedClientGetSessionSocket (
							 httpSession);

	switch(request->state)
	{
		case GENA_REQUEST_CONNECTING:
			rtp_fd_set (writeList, genaSocket);
			break;
		case GENA_REQUEST_READING:
			rtp_fd_set (readList, genaSocket);
			break;
	}
	return (RTP_TIMEOUT_INFINITE);
}

/*----------------------------------------------------------------------------*
 *  					GENA_ClientRequestProcessState						  *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
    @return error code
 */

GENA_BOOL GENA_ClientRequestProcessState (
		GENAClientRequest* request,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
    GENA_BOOL done = 0;
    /* extract the socket information from the request */
	RTP_SOCKET genaSocket = HTTP_ManagedClientGetSessionSocket (
							 request->httpSession);

	/* if socket is in error state set the state to reflect this error */
	if(rtp_fd_isset (errList, genaSocket) != 0)
	{
		request->state = GENA_REQUEST_ERROR;
		return(done);
	}

	switch(request->state)
	{
		case GENA_REQUEST_CONNECTING:
			if(rtp_fd_isset (writeList, genaSocket) != 0)
			{
				rtp_fd_clr (writeList, genaSocket);
				if (GENA_SendRequest(request) >= 0)
				{
					request->state = GENA_REQUEST_READING;
				}
				else
				{
					request->state = GENA_REQUEST_ERROR;
				}
			}
			break;

		case GENA_REQUEST_READING:
			if(rtp_fd_isset (readList, genaSocket) != 0)
			{
				/* request socket is ready to be read from */
				/* remove the socket from the list */
				/* extract and process request response */
				rtp_fd_clr (readList, genaSocket);
				GENA_ReadResponse (request);
			}
			break;

		case GENA_REQUEST_ACCEPTED:
		case GENA_REQUEST_REJECTED:
			done = 1;
			break;
	}
	return(done);
}

/*----------------------------------------------------------------------------*
 *  					GENA_SendRequest								      *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
GENA_INT32 GENA_SendRequest (
		GENAClientRequest* request
	)
{
	HTTPRequest   httpRequest;
	GENA_CHAR     timeout[40]; /* this will store 'Second-' and timout value */
	GENA_CHAR     timeString[33];

	switch(request->requestType)
	{
		case GENA_REQUEST_SUBSCRIBE:
			rtp_strcpy(timeout, "Second-");
			if (request->param.subscribe.timeoutSec > 0)
			{
				rtp_itoa(request->param.subscribe.timeoutSec, timeString, 10);
				rtp_strcpy(timeout + 7, timeString);
			}
			else
			{
				rtp_strcpy(timeout + 7, "infinite");
			}

			HTTP_ManagedClientRequestEx (
					request->httpSession,
					"SUBSCRIBE",
					request->eventSubURL.field.http.path,
					NULL,
					NULL,
					0,
					&httpRequest
				);

			HTTP_SetRequestHeaderStr (&httpRequest, "Callback", request->param.subscribe.callbackUrl);
			HTTP_SetRequestHeaderStr (&httpRequest, "NT", "upnp:event");
			HTTP_SetRequestHeaderStr (&httpRequest, "Timeout", timeout);
			break;

		case GENA_REQUEST_RENEW:
			rtp_strcpy(timeout, "Second-");
			if (request->param.subscribe.timeoutSec > 0)
			{
				rtp_itoa(request->param.subscribe.timeoutSec, timeString, 10);
				rtp_strcpy(timeout + 7, timeString);
			}
			else
			{
				rtp_strcpy(timeout + 7, "infinite");
			}

			HTTP_ManagedClientRequestEx (
					request->httpSession,
					"SUBSCRIBE",
					request->eventSubURL.field.http.path,
					NULL,
					NULL,
					0,
					&httpRequest
				);

			HTTP_SetRequestHeaderStr (&httpRequest, "SID", request->param.renew.sid);
			HTTP_SetRequestHeaderStr (&httpRequest, "Timeout", timeout);
			break;

		case GENA_REQUEST_UNSUBSCRIBE:
			HTTP_ManagedClientRequestEx (
					request->httpSession,
					"UNSUBSCRIBE",
					request->eventSubURL.field.http.path,
					NULL,
					NULL,
					0,
					&httpRequest
				);

			HTTP_SetRequestHeaderStr (&httpRequest, "SID", request->param.cancel.sid);
			break;
	}

	HTTP_ManagedClientRequestHeaderDone (request->httpSession, &httpRequest);

	/* signal that the request is compete */
	HTTP_ManagedClientWriteDone (request->httpSession);

	return(0);
}


/*----------------------------------------------------------------------------*
 *  					GENA_ReadResponse								      *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
    IMPORTANT - response.sid is allocated memory in read header callback, it needs to be freed
 */
GENA_INT32 GENA_ReadResponse (
		GENAClientRequest* request
	)
{
	HTTPResponseInfo   info;
	GENAClientResponse response;

	response.sid[0] = 0;
	response.timeoutSec = 0;

	if (HTTP_ManagedClientReadResponseInfoEx(
		   request->httpSession,
                   &info,
		   (HTTPHeaderCallback)_GENA_ReadHeader,
		   &response)>=0)
    {
	    response.httpStatus = info.status;

	    /* invoke the callback to process the response */
	    if (request->callbackFn)
	    {
		    if(response.httpStatus != 200)
    		{
    			request->state = GENA_REQUEST_REJECTED;
    		}
    		else
			{
    			request->state = GENA_REQUEST_ACCEPTED;
    		}
		    if(request->callbackFn(request->callbackData, &response) >= 0)
		    {
                return(0);
    	    }
	    }
    }

	request->state = GENA_REQUEST_ERROR;

	return(-1);
}

/*----------------------------------------------------------------------------*
 *  					_GENA_ReadHeader								      *
 *----------------------------------------------------------------------------*/
/** @memo
    @doc
	/*
    @return error code
 */
GENA_INT32 _GENA_ReadHeader (
		void* userData,
		HTTPSession* session,
		HTTPHeaderType type,
		const GENA_CHAR* name,
		const GENA_CHAR* value)
{
	GENAClientResponse* response = (GENAClientResponse*)userData;
	switch(type)
	{
		case HTTP_HEADER_SID:
			rtp_strcpy(response->sid, value);
			break;
		case HTTP_HEADER_TIMEOUT:
            value = rtp_strstr(value, "-");
			response->timeoutSec = rtp_atoi(value+1);
			break;
	}
	return (0);
}
