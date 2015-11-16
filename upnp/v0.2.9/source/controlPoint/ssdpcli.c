//
// SSDP.C - UPnP Client side SSDP-related functions
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
#include "ssdpcli.h"
#include "rtp.h"
#include "rtpnet.h"
#include "rtpstr.h"
#include "rtpscnv.h"
#include "rtptime.h"
#include "rtpprint.h"
#include "httpp.h"
#include "httpmcli.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define SSDP_ProcessError(X)
#define	SSDP_BUFFER_SIZE     2048

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_SSDPSearchHeaderInfo
{
	HTTP_CHAR           buffer[SSDP_BUFFER_SIZE];
	HTTP_INT32          bufferUsed;
	SSDPSearchResponse* searchResponse;
}
SSDPSearchHeaderInfo;

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

int _SSDP_SearchProcessResponseHeader (
		void* userData,
		HTTPSession* session,
		HTTPHeaderType type,
		const HTTP_CHAR* name,
		const HTTP_CHAR* value
	);

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*----------------------------------------------------------------------------*
                          SSDP_SearchInit
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
int SSDP_SearchInit (
		SSDPClientSearch* search,
		HTTPManagedClient* httpClient,
        SSDP_INT16 ipType,
		SSDP_CHAR* searchType,
		SSDP_INT32 maxResponseTimeoutSec,
		SSDPSearchCallback callbackFn,
		void* callbackData
	)
{
	SSDP_CHAR ipAddr[32];

    rtp_memset(search, 0, sizeof(SSDPClientSearch));

	if(ipType == RTP_NET_TYPE_IPV6)
    {
        rtp_sprintf (ipAddr, "[FF02::C]");
    }
    else
    {
        rtp_sprintf (ipAddr, "239.255.255.250");
    }
    if (HTTP_ManagedClientStartTransaction (
			httpClient,
			ipAddr,
			1900,
            ipType, /* IP v4 or v6 */
			HTTP_SESSION_TYPE_UDP,
			0, /* non-blocking */
			&search->httpSession
		) >= 0)
	{
		if (HTTP_ManagedClientWriteSelect(search->httpSession, 0) >= 0)
		{
			HTTPRequest request;

			if (HTTP_ManagedClientRequestEx (
					search->httpSession,
					"M-SEARCH",
					"*",
					0,                        /* ifModifiedSince date: omit */
					0,                        /* contentType: omit */
					HTTP_CONTENT_LENGTH_NONE, /* contentLength: omit */
					&request
				) >= 0)
			{
				HTTP_SetRequestHeaderInt(&request, "MX", maxResponseTimeoutSec);
				HTTP_SetRequestHeaderStr(&request, "Man", "\"ssdp:discover\"");
				HTTP_SetRequestHeaderStr(&request, "ST", searchType);

				if (HTTP_ManagedClientRequestHeaderDone (
						search->httpSession,
						&request
					) >= 0)
				{
					search->state = SSDP_SEARCH_READING_REPLIES;
					search->callbackFn = callbackFn;
					search->callbackData = callbackData;
					search->searchSentTimeMsec = rtp_get_system_msec();
					search->maxResponseTimeMsec = maxResponseTimeoutSec * 1000;

					return (0);
				}
			}
		}

		HTTP_ManagedClientFinishTransaction(search->httpSession);
	}

	return (-1);
}

/*----------------------------------------------------------------------------*
                          SSDP_SearchDestroy
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
void SSDP_SearchDestroy (
		SSDPClientSearch* search
	)
{
	HTTP_ManagedClientFinishTransaction(search->httpSession);
}

/*----------------------------------------------------------------------------*
                          SSDP_SearchExecute
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
/* synchronous; do the whole search and return the results */
int SSDP_SearchExecute (
		SSDPClientSearch* search
	)
{
	return (-1);
}

/*----------------------------------------------------------------------------*
                          SSDP_SearchAddToSelectList
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
SSDP_INT32 SSDP_SearchAddToSelectList (
		SSDPClientSearch* search,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
	SSDP_INT32 msecsRemaining;
	RTP_SOCKET sd = HTTP_ManagedClientGetSessionSocket(search->httpSession);

	switch (search->state)
	{
        case SSDP_SEARCH_READING_REPLIES:
			msecsRemaining = (SSDP_INT32) search->maxResponseTimeMsec - (rtp_get_system_msec() - search->searchSentTimeMsec);
			if (msecsRemaining <= 0)
			{
				search->state = SSDP_SEARCH_COMPLETE;
				msecsRemaining = 0;
			}
			else
			{
				rtp_fd_set(readList, sd);
				rtp_fd_set(errList, sd);
			}
			break;

		default:
			msecsRemaining = 0;
			break;
	}

	return (msecsRemaining);
}

/*----------------------------------------------------------------------------*
                          SSDP_SearchProcessState
 *----------------------------------------------------------------------------*/
/** @memo
    @doc

    @return error code
 */
SSDP_BOOL SSDP_SearchProcessState (
		SSDPClientSearch* search,
		RTP_FD_SET* readList,
		RTP_FD_SET* writeList,
		RTP_FD_SET* errList
	)
{
	SSDP_BOOL done = 0;
	RTP_SOCKET sd = HTTP_ManagedClientGetSessionSocket(search->httpSession);

	if (rtp_fd_isset(errList, sd))
	{
		search->state = SSDP_SEARCH_ERROR;
		done = 1;
	}
	else
	{
		switch (search->state)
		{
			case SSDP_SEARCH_READING_REPLIES:
				if (rtp_fd_isset(readList, sd))
				{
					HTTP_INT32 bytesRead;
					SSDPSearchResponse searchResponse;
					HTTPResponseInfo responseInfo;
					SSDPSearchHeaderInfo headerInfo;
					HTTP_UINT8 buffer[SSDP_BUFFER_SIZE];
					HTTP_INT32 bufferSize = SSDP_BUFFER_SIZE;

					rtp_fd_clr(readList, sd);

					headerInfo.bufferUsed = 0;
					headerInfo.buffer[SSDP_BUFFER_SIZE - 1] = 0;
					headerInfo.searchResponse = &searchResponse;

					searchResponse.location = 0;
					searchResponse.usn = 0;
					searchResponse.searchType = 0;
					searchResponse.cacheTimeoutSec = -1;

					/* ready to recv; read the response */
					bytesRead = HTTP_ManagedClientReadFrom (
							search->httpSession,
							&responseInfo,
							_SSDP_SearchProcessResponseHeader,
							&headerInfo,
							buffer,
							bufferSize,
							&searchResponse.fromAddr
						);

					if (bytesRead >= 0)
					{
						if (search->callbackFn(search->callbackData, &searchResponse) == 1)
						{
							done = 1;
						}
					}
				}
				break;

			default:
				done = 1;
				break;
		}
	}

	return (done);
}

/*----------------------------------------------------------------------------*
                      _SSDP_SearchProcessResponseHeader
 *----------------------------------------------------------------------------*/
int _SSDP_SearchProcessResponseHeader (
		void* userData,
		HTTPSession* session,
		HTTPHeaderType type,
		const HTTP_CHAR* name,
		const HTTP_CHAR* value
	)
{
	SSDPSearchHeaderInfo* headerInfo = (SSDPSearchHeaderInfo*) userData;

	switch (type)
	{
		case HTTP_HEADER_LOCATION:
			if (headerInfo->bufferUsed < SSDP_BUFFER_SIZE)
			{
				rtp_strncpy (
						headerInfo->buffer + headerInfo->bufferUsed,
						value,
						SSDP_BUFFER_SIZE - (headerInfo->bufferUsed + 2)
					);

				headerInfo->searchResponse->location = headerInfo->buffer + headerInfo->bufferUsed;
				headerInfo->bufferUsed += rtp_strlen(value) + 1;
			}
			break;

		case HTTP_HEADER_USN:
			if (headerInfo->bufferUsed < SSDP_BUFFER_SIZE)
			{
				rtp_strncpy (
						headerInfo->buffer + headerInfo->bufferUsed,
						value,
						SSDP_BUFFER_SIZE - (headerInfo->bufferUsed + 2)
					);

				headerInfo->searchResponse->usn = headerInfo->buffer + headerInfo->bufferUsed;
				headerInfo->bufferUsed += rtp_strlen(value) + 1;
			}
			break;

		case HTTP_HEADER_CACHE_CONTROL:
		{
			HTTP_CHAR* maxAgePos = rtp_strstr(value, "max-age=");

			if (maxAgePos)
			{
				maxAgePos += 8;
				headerInfo->searchResponse->cacheTimeoutSec = rtp_atol(maxAgePos);
			}
			break;
		}

		case HTTP_HEADER_ST:
			if (headerInfo->bufferUsed < SSDP_BUFFER_SIZE)
			{
				rtp_strncpy (
						headerInfo->buffer + headerInfo->bufferUsed,
						value,
						SSDP_BUFFER_SIZE - (headerInfo->bufferUsed + 2)
					);

				headerInfo->searchResponse->searchType = headerInfo->buffer + headerInfo->bufferUsed;
				headerInfo->bufferUsed += rtp_strlen(value) + 1;
			}
			break;
	}

	return (0);
}
