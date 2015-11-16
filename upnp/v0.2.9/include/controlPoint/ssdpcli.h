//
// SSDPCLI.H -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:07 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __SSDPCLI_H__
#define __SSDPCLI_H__

#include "ssdptypes.h"
#include "httpmcli.h"

typedef struct s_SSDPSearchResponse SSDPSearchResponse;

typedef int (*SSDPSearchCallback) (
		void* callbackData,
		SSDPSearchResponse* response
	);

enum e_SSDPSearchState
{
	SSDP_SEARCH_SENDING_REQUEST = 0,
	SSDP_SEARCH_READING_REPLIES,
	SSDP_SEARCH_COMPLETE,
	SSDP_SEARCH_ERROR,
	SSDP_NUM_SEARCH_STATES
};

typedef enum e_SSDPSearchState SSDPSearchState;

struct s_SSDPClientSearch
{
	SSDPSearchState           state;
	HTTPManagedClientSession* httpSession;
	SSDPSearchCallback        callbackFn;
	void*                     callbackData;
	SSDP_UINT32               searchSentTimeMsec;
	SSDP_UINT32               maxResponseTimeMsec;
};

typedef struct s_SSDPClientSearch SSDPClientSearch;

struct s_SSDPSearchResponse
{
	const SSDP_CHAR*          location;
	const SSDP_CHAR*          usn;
	const SSDP_CHAR*          searchType;
	SSDP_INT32                cacheTimeoutSec;
	RTP_NET_ADDR              fromAddr;
};

#ifdef __cplusplus
extern "C" {
#endif

int SSDP_SearchInit (
		SSDPClientSearch* search,
		HTTPManagedClient* httpClient,
        SSDP_INT16 ipType,
		SSDP_CHAR* searchType,
		SSDP_INT32 maxResponseTimeoutSec,
		SSDPSearchCallback callbackFn,
		void* callbackData
	);

void SSDP_SearchDestroy (
		SSDPClientSearch* search
	);

/* synchronous; do the whole search and return the results */
int SSDP_SearchExecute (
		SSDPClientSearch* search
	);

SSDP_INT32 SSDP_SearchAddToSelectList (
		SSDPClientSearch* search,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

SSDP_BOOL SSDP_SearchProcessState (
		SSDPClientSearch* search,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	);

#ifdef __cplusplus
}
#endif

#endif /* __SSDPCLI_H__ */
