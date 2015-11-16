//
// GENASRV.H -
//
// EBS -
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __GENASRV_H__
#define __GENASRV_H__

#include "dllist.h"
#include "rtpnet.h"
#include "httpsrv.h"
#include "genatypes.h"

#define GENA_CALLBACK_URL_LEN      300
#define GENA_SUBSCRIPTION_ID_LEN   300
#define GENA_NT_LEN                32

typedef struct s_GENAServerContext GENAServerContext;
typedef struct s_GENAServerRequest GENAServerRequest;

typedef int (*GENAServerCallback) (
		struct s_GENAServerContext* ctx,
		struct s_GENAServerRequest* request,
		void *cookie);

struct s_GENAServerContext
{
	GENAServerCallback userCallback;
	void*              userCookie;
	GENA_INT16         ipType;
};

typedef struct s_GENAServerNotifier
{
	DLListNode         subscriberList;
	GENA_INT16         numSubscribers;
	GENA_UINT32        eventSequence;
}
GENAServerNotifier;

typedef struct s_GENAServerSubscriber
{
	DLListNode         node;
	GENA_CHAR          notifyURL[GENA_CALLBACK_URL_LEN];
	GENA_CHAR*         notifyHost;
	GENA_CHAR*         notifyPath;
	RTP_NET_ADDR       notifyAddr;
	GENA_CHAR          subscriptionId[GENA_SUBSCRIPTION_ID_LEN];
	GENA_INT32         timeoutSec;
	GENA_UINT32        lastUpdatedTimeSec;
}
GENAServerSubscriber;

struct s_GENAServerRequest
{
	struct
	{
		RTP_NET_ADDR          clientAddr;
	    GENARequestType       type;
		const GENA_CHAR*      target;
		const GENA_CHAR*      notificationType;
		const GENA_CHAR*      callbackURL;
		const GENA_CHAR*      subscriptionId;
		GENA_UINT32           requestedTimeoutSec;
	}
	in;

	struct
	{
		HTTPServerRequestContext* httpServerRequest;
		HTTPSession*              httpSession;
	}
	hidden;

	struct
	{
		GENA_BOOL                 accepted;
	}
	out;
};

typedef struct s_GENAServerEvent
{
	const GENA_CHAR*  contentType;
	const GENA_CHAR*  notifySubType;
	const GENA_CHAR*  notifyType;
	const GENA_CHAR** parts;
	GENA_UINT32*      partLen;
	int               numParts;
}
GENAServerEvent;

#ifdef __cplusplus
extern "C" {
#endif

int  GENA_ServerInit               (GENAServerContext* ctx,
									GENA_INT16 ipType,
                                    GENAServerCallback callback,
                                    void* cookie);

void GENA_ServerShutdown           (GENAServerContext* ctx);

void GENA_ServerSetCookie          (GENAServerContext* ctx,
                                    void* cookie);

int  GENA_ServerProcessRequest     (GENAServerContext* ctx,
                                    HTTPServerRequestContext* srv,
                                    HTTPSession* session,
                                    HTTPRequest* request,
                                    RTP_NET_ADDR* clientAddr);

int  GENA_ServerInitNotifier       (GENAServerContext* ctx,
                                    GENAServerNotifier* who);

int  GENA_ServerAddSubscriber      (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    const GENA_CHAR* callbackURL,
                                    const GENA_CHAR* subscriptionId,
                                    GENA_INT32 timeoutSec,
                                    GENAServerNotifier* firstNotify);

int  GENA_ServerSendAccept         (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    GENAServerRequest* request,
                                    const GENA_CHAR* subscriptionId,
                                    GENA_INT32 timeoutSec,
                                    GENAServerNotifier* firstNotify,
                                    void (*readyToWriteFn)(void*),
                                    void* userData);

int  GENA_ServerRemoveSubscriber   (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    const GENA_CHAR* subscriptionId);

int  GENA_ServerRenewSubscription  (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    const GENA_CHAR* subscriptionId,
                                    GENA_INT32 timeoutSec);

int  GENA_ServerSendNotify         (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    GENAServerEvent* event,
                                    GENA_INT32 msecTimeout);

int  GENA_ServerProcessTimer       (GENAServerContext* ctx,
                                    GENAServerNotifier* who);

void GENA_ServerKillNotifier       (GENAServerContext* ctx,
                                    GENAServerNotifier* who);

void GENA_ServerCloneAllForNotify  (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    GENAServerNotifier* clone);

void GENA_ServerCloneOneForNotify  (GENAServerContext* ctx,
                                    GENAServerNotifier* who,
                                    GENAServerSubscriber* sub,
                                    GENAServerNotifier* clone);

#ifdef __cplusplus
}
#endif

#endif /*__GENASRV_H__*/
