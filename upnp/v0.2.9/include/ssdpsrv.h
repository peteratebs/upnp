/*=================================== S S D P . H ==================================*
*
*   Copyright EBS Inc , 1993-2004
*   All rights reserved.
*   This code may not be redistributed in source or linkable object form
*   without the consent of its author.
*
*   Purpose:    Header file for SSDP library
*
*   Changes:    12-19-2003 - Created.
*************************************************************************************/
#ifndef __SSDPSRV_H__
#define __SSDPSRV_H__

typedef struct s_SSDPServerContext   SSDPServerContext;
typedef struct s_SSDPServerRequest   SSDPServerRequest;
typedef struct s_SSDPSearch          SSDPSearch;
typedef struct s_SSDPNotify          SSDPNotify;

#include "dllist.h"
#include "rtpnet.h"
#include "ssdptypes.h"

typedef RTP_HANDLE SSDP_SOCKET;

typedef int (*SSDPCallback) (
		SSDPServerContext* ctx,
		SSDPServerRequest* request,
		void* cookie);

enum e_SSDPServerRequestType
{
	SSDP_SERVER_REQUEST_UNKNOWN = -1,
	SSDP_SERVER_REQUEST_M_SEARCH = 0,
	SSDP_SERVER_REQUEST_NOTIFY,
	SSDP_NUM_SERVER_REQUEST_TYPES
};
typedef enum e_SSDPServerRequestType SSDPServerRequestType;

struct s_SSDPServerContext
{
	SSDP_SOCKET      announceSocket;
	SSDPCallback     userCallback;
	SSDP_CHAR*       serverName;
	SSDP_INT16       ipType;     /* version 6 or version 4 */
	void*            userCookie;
	DLListNode       pendingResponses;
};

struct s_SSDPSearch
{
	RTP_NET_ADDR     clientAddr;
	SSDP_UINT32      maxReplyTimeoutSec;
	SSDP_CHAR*       target;
};

struct s_SSDPNotify
{
	SSDP_CHAR*       type;
	SSDP_CHAR*       subType;
	SSDP_CHAR*       usn;
	SSDP_CHAR*       location;
	SSDP_UINT32      timeout;
};

struct s_SSDPServerRequest
{
	SSDPServerRequestType type;

	union
	{
		SSDPSearch search;
		SSDPNotify notify;
	}
	data;
};

typedef struct s_SSDPPendingResponse
{
	DLListNode       node;
	RTP_NET_ADDR     clientAddr;
	SSDP_UINT32      scheduledTimeMsec;
	SSDP_CHAR       *searchTarget;
	SSDP_CHAR       *targetLocation;
	SSDP_CHAR       *targetUSN;
	SSDP_UINT32      targetTimeoutSec;
}
SSDPPendingResponse;

#ifdef __cplusplus
extern "C" {
#endif

SSDP_INT32 SSDP_ServerInit (
		SSDPServerContext *ctx,
		SSDP_UINT8 *ipv4Addr,
		SSDP_INT16 ipType,         /** ip version 4 or ipversion 6 */
		const SSDP_CHAR *serverId,
		SSDPCallback cb,
		void *cookie
	);

void SSDP_ServerDestroy (
		SSDPServerContext* ctx
	);

SSDP_INT32 SSDP_SendAlive (
		SSDPServerContext* ctx,
		const SSDP_CHAR* notifyType,
		const SSDP_CHAR* usn,
		const SSDP_CHAR* location,
		SSDP_UINT32* timeout
	);
SSDP_INT32 SSDP_SendByebye (
		SSDPServerContext* ctx,
		const SSDP_CHAR* notifyType,
		const SSDP_CHAR* usn
	);

SSDP_INT32 SSDP_ServerAddToSelectList (
		SSDPServerContext* ctx,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

SSDP_BOOL SSDP_ServerProcessState (
		SSDPServerContext* ctx,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

/* called from inside the callback when a target match is found */
SSDP_INT32 SSDP_QueueSearchResponse (
		SSDPServerContext* ctx,
		SSDPSearch* search,
		const SSDP_CHAR* targetLocation,
		const SSDP_CHAR* targetURN,
		SSDP_UINT32 targetTimeoutSec
	);

#ifdef __cplusplus
}
#endif

#endif		// __SSDPSRV_H__
// END SSDP.H
