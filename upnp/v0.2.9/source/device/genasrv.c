//
// GENA.C -
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

#include "genasrv.h"
#include "rtptime.h"
#include "rtpstr.h"
#include "rtpscnv.h"
#include "rtpmem.h"
#include "rtpprint.h"
#include "assert.h"
#include "rtplog.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

#define GENA_HTTP_BUFFER_SIZE      1024
#define GENA_NOTIFY_BATCH_SIZE     16
#define GENA_ASSERT(X)             assert(X)


/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

int  _GENA_ServerProcessTimer      (GENAServerContext *ctx,
                                    GENAServerNotifier *who);

int _GENA_ServerProcessHttpHeader(
		void *userData,
		HTTPServerRequestContext *ctx,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value);

void _GENA_ServerGenerateSubscriptionId(
		GENA_CHAR *sid,
		const GENA_CHAR *callbackURL,
		const GENA_CHAR *eventURL);

int  _GENA_ServerSendNotify (GENAServerContext *ctx,
                             GENAServerNotifier *who,
							 GENAServerEvent *event,
							 GENA_INT32 msecTimeout);

HTTP_INT32 _GENA_ServerWriteTimeout (
		void* privateData,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	);

GENAServerSubscriber * _GENA_ServerAllocSubscriber (void);
void                   _GENA_ServerFreeSubscriber  (GENAServerSubscriber *who);
GENAServerSubscriber * _GENA_ServerCloneSubscriber (GENAServerSubscriber *who);

/*****************************************************************************/
// Functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  GENA_ServerInit        (GENAServerContext *ctx,
							 GENA_INT16 ipType,
                             GENAServerCallback callback,
                             void *cookie)
{
	ctx->userCallback = callback;
	ctx->userCookie = cookie;
	ctx->ipType = ipType;

	return (0);
}

/*---------------------------------------------------------------------------*/
void GENA_ServerShutdown          (GENAServerContext *ctx)
{
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerProcessRequest    (GENAServerContext *ctx,
                                   HTTPServerRequestContext *srv,
                                   HTTPSession *session,
                                   HTTPRequest *request,
                                   RTP_NET_ADDR *clientAddr)
{
	switch (request->methodType)
	{
		case HTTP_METHOD_UNSUBSCRIBE:
		case HTTP_METHOD_SUBSCRIBE:
		{
			int result;
			GENAServerRequest genaRequest;
			GENA_CHAR nt[GENA_NT_LEN] = "";
			GENA_CHAR cbUrl[GENA_CALLBACK_URL_LEN] = "";
			GENA_CHAR sid[GENA_SUBSCRIPTION_ID_LEN] = "";
			HTTP_UINT8 buffer[GENA_HTTP_BUFFER_SIZE];
			HTTP_INT32 bufferSize = GENA_HTTP_BUFFER_SIZE;

			rtp_memset(&genaRequest, 0, sizeof(GENAServerRequest));

			sid[0] = 0;

			genaRequest.in.clientAddr = *clientAddr;

			if (request->methodType == HTTP_METHOD_SUBSCRIBE)
			{
				genaRequest.in.type = GENA_REQUEST_SUBSCRIBE;
			}
			else
			{
				genaRequest.in.type = GENA_REQUEST_UNSUBSCRIBE;
			}

			genaRequest.hidden.httpServerRequest = srv;
			genaRequest.hidden.httpSession = session;

			genaRequest.out.accepted = 0;

			genaRequest.in.target = request->target;
			genaRequest.in.notificationType = nt;
			genaRequest.in.callbackURL = cbUrl;
			genaRequest.in.subscriptionId = sid;

			HTTP_ServerReadHeaders (srv, session, _GENA_ServerProcessHttpHeader,
			                        &genaRequest, buffer, bufferSize);

			if (ctx->userCallback)
			{
				result = ctx->userCallback(ctx, &genaRequest, ctx->userCookie);

				if (!genaRequest.out.accepted)
				{
					HTTP_ServerSendError (srv, session, 500, "Request Denied");
				}
			}
			break;
		}

		default:
			break;
	}

	return (HTTP_REQUEST_STATUS_CONTINUE);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerInitNotifier      (GENAServerContext *ctx,
                                   GENAServerNotifier *who)
{
	DLLIST_INIT(&who->subscriberList);
	who->numSubscribers = 0;
	who->eventSequence = 0;

	return (0);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerAddSubscriber     (GENAServerContext* ctx,
                                   GENAServerNotifier* who,
                                   const GENA_CHAR* callbackURL,
                                   const GENA_CHAR* subscriptionId,
                                   GENA_INT32 timeoutSec,
                                   GENAServerNotifier* firstNotify)
{
GENAServerSubscriber *subscriber;
GENA_CHAR *pos;

	subscriber = (GENAServerSubscriber *) who->subscriberList.next;
	while (subscriber != (GENAServerSubscriber *) &who->subscriberList)
	{
		if (!rtp_strcmp(subscriber->subscriptionId, subscriptionId))
		{
			break;
		}
		subscriber = (GENAServerSubscriber *) subscriber->node.next;
	}

	if (subscriber == (GENAServerSubscriber *) &who->subscriberList)
	{
		subscriber = _GENA_ServerAllocSubscriber();
		if (!subscriber)
		{
			return (-1);
		}

		rtp_memset(subscriber, 0, sizeof(GENAServerSubscriber));

		/* link the node into the list (at the end of the list) */
		DLLIST_INSERT_BEFORE(&who->subscriberList, &subscriber->node);
		who->numSubscribers++;
	}

	if (callbackURL[0] == '<')
	{
		long i = rtp_strlen(callbackURL) - 1;
		if (i < 0)
		{
			i = 0;
		}
		else
		{
			if (callbackURL[i] == '>')
			{
				i--;
			}
		}
		if (i > GENA_CALLBACK_URL_LEN-2)
		{
			i = GENA_CALLBACK_URL_LEN-2;
		}
		rtp_strncpy(subscriber->notifyURL, callbackURL+1, i);
	}
	else
	{
		rtp_strncpy(subscriber->notifyURL, callbackURL, GENA_CALLBACK_URL_LEN-2);
	}
	subscriber->notifyURL[GENA_CALLBACK_URL_LEN-1] = 0;

	subscriber->notifyPath = 0;
	subscriber->notifyHost = 0;

	/* tbd - parse out the URL */
	pos = rtp_stristr(subscriber->notifyURL, "http://");
	if (pos)
	{
		subscriber->notifyHost = pos + 7;
		pos = subscriber->notifyHost + 1;
		while (*pos)
		{
			if (*pos == ':')
			{
				subscriber->notifyAddr.port = rtp_atoi(pos + 1);
				subscriber->notifyPath = rtp_strstr(pos, "/");
				*pos = 0;
				break;
			}
			else if (*pos == '/')
			{
				subscriber->notifyAddr.port = 80;
				subscriber->notifyPath = pos + 1;
				rtp_memmove(subscriber->notifyPath, pos, rtp_strlen(pos) * sizeof(GENA_CHAR));
				*pos = 0;
				break;
			}
			pos++;
		}

		if (subscriber->notifyHost[0] >= '0' && subscriber->notifyHost[0] <= '9')
		{
			if (rtp_net_str_to_ip(subscriber->notifyAddr.ipAddr,
			                      subscriber->notifyHost,
			                      &subscriber->notifyAddr.type) < 0)
			{
				rtp_memset(subscriber->notifyAddr.ipAddr, 0, RTP_NET_IP_ALEN);
			}
		}
	}

	rtp_strncpy(subscriber->subscriptionId, subscriptionId, GENA_SUBSCRIPTION_ID_LEN-1);
	subscriber->subscriptionId[GENA_SUBSCRIPTION_ID_LEN-1] = 0;

	subscriber->timeoutSec = timeoutSec;
	subscriber->lastUpdatedTimeSec = rtp_get_system_sec();

	if (firstNotify)
	{
		GENA_ServerCloneOneForNotify (
				ctx,
				who,
				subscriber,
				firstNotify
			);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerRemoveSubscriber  (GENAServerContext *ctx,
                                   GENAServerNotifier *who,
                                   const GENA_CHAR *subscriptionId)
{
GENAServerSubscriber *subscriber;

	subscriber = (GENAServerSubscriber *) who->subscriberList.next;
	while (subscriber != (GENAServerSubscriber *) &who->subscriberList)
	{
		if (!rtp_strcmp(subscriber->subscriptionId, subscriptionId))
		{
			DLLIST_REMOVE(&subscriber->node);
			_GENA_ServerFreeSubscriber(subscriber);
			who->numSubscribers--;
			return (0);
		}
		subscriber = (GENAServerSubscriber *) subscriber->node.next;
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerRenewSubscription (GENAServerContext *ctx,
                                   GENAServerNotifier *who,
                                   const GENA_CHAR *subscriptionId,
                                   GENA_INT32 timeoutSec)
{
GENAServerSubscriber *subscriber;

	subscriber = (GENAServerSubscriber *) who->subscriberList.next;
	while (subscriber != (GENAServerSubscriber *) &who->subscriberList)
	{
		if (!rtp_strcmp(subscriber->subscriptionId, subscriptionId))
		{
			subscriber->timeoutSec = timeoutSec;
			subscriber->lastUpdatedTimeSec = rtp_get_system_sec();
			return (0);
		}
		subscriber = (GENAServerSubscriber *) subscriber->node.next;
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerSendNotify        (GENAServerContext *ctx,
                                   GENAServerNotifier *who,
						           GENAServerEvent *event,
                                   GENA_INT32 timeoutMsec)
{
RTP_SOCKET sock[GENA_NOTIFY_BATCH_SIZE];
int sockState[GENA_NOTIFY_BATCH_SIZE];
GENAServerSubscriber *sub[GENA_NOTIFY_BATCH_SIZE];
RTP_FD_SET wSet1, wSet2, rSet1, rSet2, eSet1, eSet2;
RTP_FD_SET *writeSet, *nextWriteSet, *readSet, *nextReadSet, *tempSet;
RTP_FD_SET *errSet, *nextErrSet;
int numLeft;
int n;
int batchSize = GENA_NOTIFY_BATCH_SIZE;
GENA_UINT8 *httpMessage;
GENA_INT32  httpMessageLen, httpMessageBufferSize, bodyLen;
GENAServerSubscriber *nextSubscriber;
GENA_INT32 startTimeMsec, msecRemaining;

#define FDSET_SWAP(A,B) {tempSet = (A); (A) = (B); (B) = tempSet;}

	GENA_ASSERT(GENA_NOTIFY_BATCH_SIZE < RTP_FD_SET_MAX);

	_GENA_ServerProcessTimer(ctx, who);

	if (who->numSubscribers == 0)
	{
		return (0);
	}

	numLeft = who->numSubscribers;
	nextSubscriber = (GENAServerSubscriber *) who->subscriberList.next;

	bodyLen = 0;
	for (n=0; n<event->numParts; n++)
	{
		bodyLen += event->partLen[n];
	}

	/* grab a buffer big enough to hold the entire HTTP message (size is an estimate)*/
	httpMessageBufferSize = 384 + rtp_strlen(event->notifyType) + rtp_strlen(event->notifySubType) +
	                        GENA_CALLBACK_URL_LEN + GENA_SUBSCRIPTION_ID_LEN + bodyLen;

	httpMessage = (GENA_UINT8 *) rtp_malloc(httpMessageBufferSize);
	if (!httpMessage)
	{
		return (-1);
	}

	if (who->numSubscribers < batchSize)
	{
		batchSize = who->numSubscribers;
	}

	who->eventSequence++;

	writeSet = &wSet1;
	readSet = &rSet1;
	errSet = &eSet1;
	nextWriteSet = &wSet2;
	nextReadSet = &rSet2;
	nextErrSet = &eSet2;

	rtp_memset(sockState, 0, sizeof(int) * GENA_NOTIFY_BATCH_SIZE);
	rtp_memset(sub, 0, sizeof(GENAServerSubscriber *) * GENA_NOTIFY_BATCH_SIZE);

	rtp_fd_zero(writeSet);
	rtp_fd_zero(readSet);
	rtp_fd_zero(errSet);

	startTimeMsec = rtp_get_system_msec();

	while (numLeft > 0)
	{
		rtp_fd_zero(nextWriteSet);
		rtp_fd_zero(nextReadSet);
		rtp_fd_zero(nextErrSet);

		for (n = 0; n < batchSize && numLeft > 0; n++)
		{
			if (rtp_fd_isset(errSet, sock[n]))
			{
				rtp_net_closesocket(sock[n]);
				sockState[n] = 0;
				numLeft--;
			}

			switch (sockState[n])
			{
				case 2: /* waiting for response */
					if (rtp_fd_isset(readSet, sock[n]))
					{
						/* server responded; this socket is all done */
						rtp_net_closesocket(sock[n]);
						sockState[n] = 0;
						numLeft--;
						/* fall through to case 0 below */
					}
					else
					{
						/* no response yet; put socket back on read list */
						rtp_fd_set(nextReadSet, sock[n]);
						rtp_fd_set(nextErrSet, sock[n]);
						break;
					}
					/* intentional fall through (see above) */

				case 0: /* socket not in use */
					if (nextSubscriber != (GENAServerSubscriber *) &who->subscriberList)
					{
						/* a free slot; use it for the next subscriber */
						if (rtp_net_socket_stream_dual(&sock[n], ctx->ipType) < 0)
						{
							break;
						}

						/* it is crucial that non-blocking socket I/O be supported! */
						rtp_net_setblocking(sock[n], 0);
						rtp_net_setlinger(sock[n], 1, 0);

						rtp_net_connect_dual(sock[n], RTP_NET_STREAM,
						                nextSubscriber->notifyAddr.ipAddr,
						                nextSubscriber->notifyAddr.port,
						                nextSubscriber->notifyAddr.type);

						sub[n] = nextSubscriber;
						nextSubscriber = (GENAServerSubscriber *) nextSubscriber->node.next;

						/* select this socket for connect complete or error;
						   state is waiting for connect */
						rtp_fd_set(nextWriteSet, sock[n]);
						rtp_fd_set(nextErrSet, sock[n]);
						sockState[n] = 1;
					}
					break;

				case 1: /* waiting for connect */
					if (rtp_fd_isset(writeSet, sock[n]))
					{
						int i;
						GENA_INT32 partLen;

						/* connected; write the notification message */
						rtp_sprintf((char *) httpMessage,
						           "NOTIFY %s HTTP/1.1\r\n"
						           "SID: %s\r\n"
						           "Host: %s:%d\r\n"
						           "SEQ: %d\r\n"
						           "Content-Type: %s\r\n"
						           "Connection: close\r\n"
						           "NTS: %s\r\n"
						           "NT: %s\r\n"
						           "Content-Length: %d\r\n"
						           "\r\n",
						           sub[n]->notifyPath,
						           sub[n]->subscriptionId,
						           sub[n]->notifyHost,
						           sub[n]->notifyAddr.port,
						           who->eventSequence,
								   event->contentType,
						           event->notifySubType,
						           event->notifyType,
						           bodyLen);

						httpMessageLen = rtp_strlen((const char *) httpMessage);

						for (i=0; i<event->numParts; i++)
						{
							partLen = event->partLen[i];
							rtp_memcpy(httpMessage + httpMessageLen, event->parts[i], partLen);
							httpMessageLen += partLen;
						}

						RTP_LOG_WRITE("GENA_ServerSendNotify SEND", httpMessage, httpMessageLen)

						rtp_net_send(sock[n], (unsigned char *) httpMessage, httpMessageLen);

						/* advance to waiting for response state */
						rtp_fd_set(nextReadSet, sock[n]);
						rtp_fd_set(nextErrSet, sock[n]);
						sockState[n] = 2;
					}
					else
					{
						/* connect still in progress; put socket back on write list */
						rtp_fd_set(nextWriteSet, sock[n]);
						rtp_fd_set(nextErrSet, sock[n]);
					}
					break;
			}
		}

		if (numLeft <= 0)
		{
			break;
		}

		FDSET_SWAP(readSet, nextReadSet);
		FDSET_SWAP(writeSet, nextWriteSet);
		FDSET_SWAP(errSet, nextErrSet);

		msecRemaining = timeoutMsec - (GENA_INT32) rtp_get_system_msec() + startTimeMsec;
		if (msecRemaining > 0)
		{
			rtp_net_select(readSet, writeSet, errSet, msecRemaining);
		}
		else
		{
			break;
		}
	}

	for (n = 0; n < batchSize && numLeft > 0; n++)
	{
		if (sockState[n] != 0)
		{
			rtp_net_closesocket(sock[n]);
		}
	}

	rtp_free(httpMessage);

#undef FDSET_SWAP

	return (0);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerProcessTimer      (GENAServerContext *ctx,
                                   GENAServerNotifier *who)
{
	return (_GENA_ServerProcessTimer(ctx, who));
}

/*---------------------------------------------------------------------------*/
int  _GENA_ServerProcessTimer     (GENAServerContext *ctx,
                                   GENAServerNotifier *who)
{
GENAServerSubscriber *subscriber, *next;
unsigned long systemSec = rtp_get_system_sec();

	/* this function is called periodically to clean out subscriptions that
	   have expired */
	subscriber = (GENAServerSubscriber *) who->subscriberList.next;
	while (subscriber != (GENAServerSubscriber *) &who->subscriberList)
	{
		next = (GENAServerSubscriber *) subscriber->node.next;
		if (subscriber->timeoutSec != GENA_TIMEOUT_INFINITE && subscriber->timeoutSec > 0)
		{
			if (systemSec - subscriber->lastUpdatedTimeSec > (GENA_UINT32) subscriber->timeoutSec)
			{
				DLLIST_REMOVE(&subscriber->node);
				_GENA_ServerFreeSubscriber(subscriber);
				who->numSubscribers--;
			}
		}
		subscriber = next;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void GENA_ServerKillNotifier      (GENAServerContext *ctx,
                                   GENAServerNotifier *who)
{
GENAServerSubscriber *subscriber;

	while (who->subscriberList.next != &who->subscriberList)
	{
		subscriber = (GENAServerSubscriber *) who->subscriberList.next;
		DLLIST_REMOVE(&subscriber->node);
		_GENA_ServerFreeSubscriber(subscriber);
		who->numSubscribers--;
	}
}

/*---------------------------------------------------------------------------*/
int _GENA_ServerProcessHttpHeader (void *userData,
                                   HTTPServerRequestContext *ctx,
                                   HTTPSession *session,
                                   HTTPHeaderType type,
                                   const HTTP_CHAR *name,
                                   const HTTP_CHAR *value)
{
GENAServerRequest *genaRequest = (GENAServerRequest *) userData;

	switch (type)
	{
		case HTTP_HEADER_CALLBACK:
			rtp_strncpy((char *) genaRequest->in.callbackURL, value, GENA_CALLBACK_URL_LEN-1);
			((char *)genaRequest->in.callbackURL)[GENA_CALLBACK_URL_LEN-1] = 0;
			break;

		case HTTP_HEADER_NT:
			rtp_strncpy((char *) genaRequest->in.notificationType, value, GENA_NT_LEN-1);
			((char *)genaRequest->in.notificationType)[GENA_NT_LEN-1] = 0;
			break;

		case HTTP_HEADER_SID:
			rtp_strncpy((char *) genaRequest->in.subscriptionId, value, GENA_SUBSCRIPTION_ID_LEN-1);
			((char *)genaRequest->in.subscriptionId)[GENA_SUBSCRIPTION_ID_LEN-1] = 0;
			break;

		case HTTP_HEADER_TIMEOUT:
		{
			char *str = rtp_stristr(value, "Second-");
			if (str)
			{
				str += 7;
				genaRequest->in.requestedTimeoutSec = rtp_atol(str);
			}
			else
			{
				if (rtp_stristr(value, "infinite"))
				{
					genaRequest->in.requestedTimeoutSec = GENA_TIMEOUT_INFINITE;
				}
			}
			break;
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _GENA_ServerGenerateSubscriptionId (
		GENA_CHAR *sid,
		const GENA_CHAR *callbackURL,
		const GENA_CHAR *eventURL
	)
{
unsigned long h1 = 24539;
unsigned long h2 = 9302127;
unsigned long h3 = rtp_get_system_msec();
const char *s;

	for (s = callbackURL; *s; s++)
	{
		h1 = h1 ^ ( (h1 << 5) + (h1 >> 2) + *s);
	}

	for (s = eventURL; *s; s++)
	{
		h2 = h2 ^ ( (h2 << 5) + (h2 >> 2) + *s);
	}


	for (s = callbackURL; *s; s++)
	{
		h3 = h3 ^ ( (h3 << 5) + (h3 >> 2) + *s);
	}

	for (s = eventURL; *s; s++)
	{
		h3 = h3 ^ ( (h3 << 5) + (h3 >> 2) + *s);
	}

	rtp_sprintf(sid, "uuid:%08x-%08x-%08x", h1, h2, h3);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _GENA_ServerWriteTimeout (
		void* privateData,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	)
{
	GENA_INT32 *timeout = (GENA_INT32 *) privateData;

	if (*timeout == GENA_TIMEOUT_INFINITE)
	{
		writeFn(requestStream, "infinite", 8);
		return (8);
	}
	else
	{
		HTTP_CHAR buffer[20];
		HTTP_INT32 len;

		rtp_itoa(*timeout, buffer, 10);
		len = rtp_strlen(buffer);

		writeFn(requestStream, "Second-", 7);
		writeFn(requestStream, buffer, len);

		return (7 + len);
	}
}

/*---------------------------------------------------------------------------*/
GENAServerSubscriber * _GENA_ServerAllocSubscriber (void)
{
	return ((GENAServerSubscriber *) rtp_malloc(sizeof(GENAServerSubscriber)));
}

/*---------------------------------------------------------------------------*/
void _GENA_ServerFreeSubscriber (GENAServerSubscriber *who)
{
	rtp_free(who);
}

/*---------------------------------------------------------------------------*/
int  GENA_ServerSendAccept   (GENAServerContext *ctx,
                              GENAServerNotifier *who,
                              GENAServerRequest *request,
                              const GENA_CHAR *subscriptionId,
                              GENA_INT32 timeoutSec,
                              GENAServerNotifier* firstNotify,
							  void (*readyToWriteFn)(void*),
							  void *userData)

{
	HTTPResponse response;

	if (firstNotify)
	{
		GENA_ServerInitNotifier(ctx, firstNotify);
	}

	if (request->in.type == GENA_REQUEST_UNSUBSCRIBE)
	{
		if (GENA_ServerRemoveSubscriber(ctx, who, request->in.subscriptionId) < 0)
		{
			/* return (-1); */
			/* someone tried to unsubscribe a non-existant subscription */
		}
	}
	else
	{
		if ((!request->in.callbackURL || !request->in.callbackURL[0]) &&
		    (request->in.subscriptionId && request->in.subscriptionId[0]))
		{
			subscriptionId = request->in.subscriptionId;

			if (timeoutSec == 0)
			{
				timeoutSec = request->in.requestedTimeoutSec;
			}

			if (GENA_ServerRenewSubscription(ctx, who, subscriptionId, timeoutSec) < 0)
			{
				return (-1);
			}
		}
		else
		{
			if (!subscriptionId || !subscriptionId[0])
			{
				if (!request->in.subscriptionId[0])
				{
					_GENA_ServerGenerateSubscriptionId (
							(GENA_CHAR*) request->in.subscriptionId,
							request->in.callbackURL,
							request->in.target
						);
				}

				subscriptionId = request->in.subscriptionId;
			}

			if (timeoutSec == 0)
			{
				timeoutSec = request->in.requestedTimeoutSec;
			}

			if (GENA_ServerAddSubscriber (
					ctx,
					who,
					request->in.callbackURL,
					subscriptionId,
					timeoutSec,
					firstNotify
				) < 0)
			{
				return (-1);
			}
		}
	}

	request->out.accepted = 1;

	if (readyToWriteFn)
	{
		readyToWriteFn(userData);
	}

	HTTP_ServerInitResponse(request->hidden.httpServerRequest, request->hidden.httpSession,
	                        &response, 200, "OK");

	HTTP_ServerSetDefaultHeaders(request->hidden.httpServerRequest, &response);

	if (request->in.type == GENA_REQUEST_SUBSCRIBE)
	{
		HTTP_SetResponseHeaderStr (
				&response,
				"SID",
				subscriptionId
			);

		HTTP_SetResponseHeaderFn (
				&response,
				"Timeout",
				_GENA_ServerWriteTimeout,
				&timeoutSec
			);
	}

	HTTP_WriteResponse(request->hidden.httpSession, &response);
	HTTP_WriteFlush(request->hidden.httpSession);
	HTTP_FreeResponse(&response);
	HTTP_ServerConnectionClose(request->hidden.httpServerRequest, request->hidden.httpSession);

	return (0);
}

/*---------------------------------------------------------------------------*/
void GENA_ServerCloneAllForNotify (
		GENAServerContext *ctx,
		GENAServerNotifier *who,
		GENAServerNotifier *clone
	)
{
GENAServerSubscriber *sub;
GENAServerSubscriber *subClone;

	_GENA_ServerProcessTimer(ctx, who);

	*clone = *who;
	DLLIST_INIT(&clone->subscriberList);

	who->eventSequence++;
	clone->numSubscribers = 0;

	sub = (GENAServerSubscriber *) who->subscriberList.next;
	while (sub != (GENAServerSubscriber *) &who->subscriberList)
	{
		subClone = _GENA_ServerCloneSubscriber(sub);
		if (subClone)
		{
			DLLIST_INSERT_BEFORE(&clone->subscriberList, &subClone->node);
			clone->numSubscribers++;
		}

		sub = (GENAServerSubscriber *) sub->node.next;
	}
}

/*---------------------------------------------------------------------------*/
void GENA_ServerCloneOneForNotify (
		GENAServerContext *ctx,
		GENAServerNotifier *who,
		GENAServerSubscriber *sub,
		GENAServerNotifier *clone
	)
{
GENAServerSubscriber *subClone;

	_GENA_ServerProcessTimer(ctx, who);

	*clone = *who;
	DLLIST_INIT(&clone->subscriberList);

	who->eventSequence++;
	clone->numSubscribers = 0;

	subClone = _GENA_ServerCloneSubscriber(sub);
	if (subClone)
	{
		DLLIST_INSERT_BEFORE(&clone->subscriberList, &subClone->node);
		clone->numSubscribers++;
	}
}

/*---------------------------------------------------------------------------*/
GENAServerSubscriber * _GENA_ServerCloneSubscriber (GENAServerSubscriber *who)
{
	GENAServerSubscriber *clone = _GENA_ServerAllocSubscriber();

	if (clone)
	{
		*clone = *who;

		DLLIST_INIT(&clone->node);

		clone->notifyHost = clone->notifyURL + (who->notifyHost - who->notifyURL);
		clone->notifyPath = clone->notifyURL + (who->notifyPath - who->notifyURL);
	}

	return (clone);
}
