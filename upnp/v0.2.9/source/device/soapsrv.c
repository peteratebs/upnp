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

#include "soapsrv.h"
#include "rtpmem.h"
#include "rtpscnv.h"
#include "rtpstr.h"
#include "rtpprint.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

#define SOAP_HTTP_BUFFER_LEN           1024
#define SOAP_ACTION_STR_LEN            256
#define SOAP_FAULT_RESPONSE_TEMPLATE   \
	"<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\""\
            " xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"\
		"<s:Body>"\
			"<s:Fault>"\
				"<faultcode>%s</faultcode>"\
				"<faultstring>%s</faultstring>"\
				"<detail>"\
					"%s"\
				"</detail>"\
			"</s:Fault>"\
		"</s:Body>"\
	"</s:Envelope>"

#define SOAP_NORMAL_RESPONSE_TEMPLATE   \
	"<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\""\
            " xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"\
		"<s:Body>"\
			"%s"\
		"</s:Body>"\
	"</s:Envelope>"

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/
int _SOAP_ProcessHttpHeader (void *userData,
                             HTTPServerRequestContext *ctx,
                             HTTPSession *session,
                             HTTPHeaderType type,
                             const HTTP_CHAR *name,
                             const HTTP_CHAR *value);


/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  SOAP_ServerInit           (SOAPServerContext *ctx,
                                SOAPServerCallback callback,
                                void *cookie)
{
	rtp_memset(ctx, 0, sizeof(SOAPServerContext));

	ctx->userCallback = callback;
	ctx->userCookie = cookie;

	return (0);
}

/*---------------------------------------------------------------------------*/
void SOAP_ServerShutdown       (SOAPServerContext *ctx)
{
	rtp_memset(ctx, 0, sizeof(SOAPServerContext));
}

/*---------------------------------------------------------------------------*/
int  SOAP_ServerProcessRequest (SOAPServerContext *ctx,
                                HTTPServerRequestContext *srv,
                                HTTPSession *session,
                                HTTPRequest *request,
                                RTP_NET_ADDR *clientAddr)
{
SOAPRequest soapRequest;
SOAP_CHAR sa[SOAP_ACTION_STR_LEN];
HTTP_INT32 httpBufferSize = SOAP_HTTP_BUFFER_LEN;
HTTP_UINT8 httpBuffer[SOAP_HTTP_BUFFER_LEN];
HTTP_UINT8 *messageBuffer = 0;

	sa[0] = 0;

	switch (request->methodType)
	{
		case HTTP_METHOD_POST:     /* intentional fall-through */
		case HTTP_METHOD_M_POST:

			rtp_memset(&soapRequest, 0, sizeof(SOAPRequest));

			soapRequest.in.messageLen = -1;
			soapRequest.in.target = request->target;
			soapRequest.in.soapAction = sa;
			soapRequest.in.clientAddr = *clientAddr;
			soapRequest.in.envelope.headerElem = 0;
			soapRequest.in.envelope.bodyElem = 0;

			HTTP_ServerReadHeaders (srv, session, _SOAP_ProcessHttpHeader,
			                        &soapRequest, httpBuffer, httpBufferSize);

			if (soapRequest.in.messageLen > 0)
			{
				messageBuffer = (HTTP_UINT8 *) rtp_malloc(soapRequest.in.messageLen + 1);
				if (messageBuffer)
				{

					if (HTTP_Read(session, messageBuffer, soapRequest.in.messageLen) < soapRequest.in.messageLen)
					{
						/* send error: bad request */
						HTTP_ServerSendError(srv, session, 400, "Bad Request");
					}
					else
					{
						int result;

						/* null terminate the message buffer for rtpxmlParseBuffer */
						messageBuffer[soapRequest.in.messageLen] = 0;

						result = rtpxmlParseBufferEx((char *) messageBuffer, &soapRequest.in.envelope.doc);

						if (result == RTPXML_SUCCESS)
						{
							RTPXML_NodeList *elements = rtpxmlDocument_getElementsByTagNameNS (
									soapRequest.in.envelope.doc,
									"http://schemas.xmlsoap.org/soap/envelope/",
									"Envelope");

							if (elements)
							{
								RTPXML_Element *envelopeElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
								rtpxmlNodeList_free(elements);

								if (envelopeElem)
								{
									elements = rtpxmlElement_getElementsByTagNameNS (
											envelopeElem,
											"http://schemas.xmlsoap.org/soap/envelope/",
											"Body");

									if (elements)
									{
										soapRequest.in.envelope.bodyElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
										rtpxmlNodeList_free(elements);

										if (soapRequest.in.envelope.bodyElem)
										{
											elements = rtpxmlElement_getElementsByTagNameNS (
													envelopeElem,
													"http://schemas.xmlsoap.org/soap/envelope/",
													"Header");

											if (elements)
											{
												soapRequest.in.envelope.headerElem = (RTPXML_Element *) rtpxmlNodeList_item(elements, 0);
												rtpxmlNodeList_free(elements);
											}
											else
											{
												soapRequest.in.envelope.headerElem = 0;
											}

											/* now call the user callback */
											if (ctx->userCallback)
											{
												HTTPResponse response;
												HTTP_INT32 responseLen;
												SOAP_INT32 responseBufferSize = 0;
												SOAP_CHAR *responseBuffer = 0;

												if (ctx->userCallback(ctx, &soapRequest, ctx->userCookie) < 0)
												{
													if (soapRequest.out.fault.detail)
													{
														DOMString faultDetailStr = rtpxmlNodetoString((RTPXML_Node *) soapRequest.out.fault.detail);

														if (!faultDetailStr)
														{
															/* couldn't print fault detail to string */
															HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
														}
														else
														{
															responseBufferSize =
																	rtp_strlen(soapRequest.out.fault.code) +
																	rtp_strlen(soapRequest.out.fault.string) +
																	rtp_strlen(faultDetailStr) + 384;

															responseBuffer = (SOAP_CHAR *) rtp_malloc(sizeof(SOAP_CHAR) * responseBufferSize);
															if (!responseBuffer)
															{
																/* malloc failure */
																HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
															}
															else
															{
																rtp_sprintf(responseBuffer,
																           SOAP_FAULT_RESPONSE_TEMPLATE,
																           soapRequest.out.fault.code,
																           soapRequest.out.fault.string,
																           faultDetailStr);

																responseLen = rtp_strlen(responseBuffer);

																HTTP_ServerInitResponse(srv, session, &response, 500, "Internal Server Error");
																HTTP_ServerSetDefaultHeaders(srv, &response);
																HTTP_SetResponseHeaderStr(&response, "Ext", "");
																HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/xml; charset=\"utf-8\"");
																HTTP_SetResponseHeaderInt(&response, "Content-Length", responseLen);

																HTTP_WriteResponse(session, &response);
																HTTP_Write(session, (const HTTP_UINT8 *) responseBuffer, responseLen);
																HTTP_WriteFlush(session);

																HTTP_FreeResponse(&response);
																rtp_free(responseBuffer);
															}

															rtpxmlFreeDOMString(faultDetailStr);
														}

														rtpxmlDocument_free(soapRequest.out.fault.detail);
													}
												}
												else
												{
													/* operation succeeded; format normal response */

													if (soapRequest.out.body)
													{
														DOMString bodyStr = rtpxmlNodetoString((RTPXML_Node *) soapRequest.out.body);

														if (!bodyStr)
														{
															HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
														}
														else
														{
															responseBufferSize = rtp_strlen(bodyStr) + 256;

															responseBuffer = (SOAP_CHAR *) rtp_malloc(sizeof(SOAP_CHAR) * responseBufferSize);
															if (!responseBuffer)
															{
																/* malloc failure */
																HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
															}
															else
															{
																rtp_sprintf(responseBuffer,
																           SOAP_NORMAL_RESPONSE_TEMPLATE,
																           bodyStr);

																responseLen = rtp_strlen(responseBuffer);

																HTTP_ServerInitResponse(srv, session, &response, 200, "OK");
																HTTP_ServerSetDefaultHeaders(srv, &response);
																HTTP_SetResponseHeaderStr(&response, "Ext", "");
																HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/xml; charset=\"utf-8\"");
																HTTP_SetResponseHeaderInt(&response, "Content-Length", responseLen);

																HTTP_WriteResponse(session, &response);
																HTTP_Write(session, (const HTTP_UINT8 *) responseBuffer, responseLen);
																HTTP_WriteFlush(session);

																HTTP_FreeResponse(&response);
																rtp_free(responseBuffer);
															}

															rtpxmlFreeDOMString(bodyStr);
														}

														rtpxmlDocument_free(soapRequest.out.body);
													}
												}
											}
											else
											{
												HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
											}
										}
										else
										{
											/* DOM function error */
											HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
										}
									}
									else
									{
										/* no Body element */
										HTTP_ServerSendError(srv, session, 400, "Bad Request");
									}
								}
								else
								{
									/* DOM function error */
									HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
								}

							}
							else
							{
								/* no Envelope element */
								HTTP_ServerSendError(srv, session, 400, "Bad Request");
							}
						}
						else
						{
							/* XML parse failed */
							/* send error: internal server error */
							HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
						}

						rtpxmlDocument_free(soapRequest.in.envelope.doc);
					}

					rtp_free(messageBuffer);
				}
				else
				{
					/* send error: internal server error */
					HTTP_ServerSendError(srv, session, 500, "Internal Server Error");
				}
			}
			else
			{
				/* send error: bad request */
				HTTP_ServerSendError(srv, session, 400, "Bad Request");
			}

			HTTP_ServerConnectSetKeepAlive(srv, session, 0);

			return (HTTP_REQUEST_STATUS_DONE);
	}

	return (HTTP_REQUEST_STATUS_CONTINUE);
}

/*---------------------------------------------------------------------------*/
int _SOAP_ProcessHttpHeader (void *userData,
                             HTTPServerRequestContext *ctx,
                             HTTPSession *session,
                             HTTPHeaderType type,
                             const HTTP_CHAR *name,
                             const HTTP_CHAR *value)
{
SOAPRequest *soapRequest = (SOAPRequest *) userData;

	switch (type)
	{
		case HTTP_HEADER_CONTENT_LENGTH:
			soapRequest->in.messageLen = rtp_atoi(value);
			break;

		case HTTP_HEADER_SOAP_ACTION:
			rtp_strncpy((char *) soapRequest->in.soapAction, value, SOAP_ACTION_STR_LEN-1);
			((char *) soapRequest->in.soapAction)[SOAP_ACTION_STR_LEN-1] = 0;
			break;

		case HTTP_UNKNOWN_HEADER_TYPE:
			if (rtp_stristr(name, "SOAPAction"))
			{
				rtp_strncpy((char *) soapRequest->in.soapAction, value, SOAP_ACTION_STR_LEN-1);
				((char *) soapRequest->in.soapAction)[SOAP_ACTION_STR_LEN-1] = 0;
			}
			break;
	}

	return (0);
}
