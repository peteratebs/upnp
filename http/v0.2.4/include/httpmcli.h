//
// HTTPMCLI.H -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/09/28 20:09:02 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __HTTPMCLI_H__
#define __HTTPMCLI_H__

/* define this symbol to support thread-safe operation */
/*#define HTTP_MANAGED_CLIENT_THREADSAFE*/

typedef struct s_HTTPManagedClient        HTTPManagedClient;
typedef struct s_HTTPManagedClientSession HTTPManagedClientSession;
typedef struct s_HTTPResponseInfo         HTTPResponseInfo;

#include "httpp.h"
#include "httpcli.h"
#include "hcookie.h"
#include "httpauth.h"
#include "filetype.h"

#ifdef HTTP_MANAGED_CLIENT_THREADSAFE
#include "rtpsignl.h"
#endif

#define HTTP_CONTENT_LENGTH_UNKNOWN -1
#define HTTP_CONTENT_LENGTH_NONE    -2

enum e_HTTPManagedSessionType
{
	HTTP_SESSION_TYPE_TCP = 0,
	HTTP_SESSION_TYPE_SECURE_TCP,
	HTTP_SESSION_TYPE_UDP,
	HTTP_NUM_SESSION_TYPES
};

enum e_HTTPManagedSessionState
{
	HTTP_SESSION_STATE_ERROR = -1,
	HTTP_SESSION_STATE_NEW = 0,
	HTTP_SESSION_STATE_OPEN,
	HTTP_SESSION_STATE_BUILDING_HEADER,
	HTTP_SESSION_STATE_BUILDING_HEADER_CHUNKED,
	HTTP_SESSION_STATE_BUILDING_HEADER_NO_DATA,
	HTTP_SESSION_STATE_WRITING_DATA,
	HTTP_SESSION_STATE_WRITING_CHUNKS,
	HTTP_SESSION_STATE_REQUEST_SENT,
	HTTP_SESSION_STATE_RESPONSE_READ,
	HTTP_SESSION_STATE_DONE,
	HTTP_NUM_SESSION_STATES
};

typedef enum e_HTTPManagedSessionType     HTTPManagedSessionType;
typedef enum e_HTTPManagedSessionState    HTTPManagedSessionState;
//typedef enum e_HTTPCacheControlType       HTTPCacheControlType;

struct s_HTTPManagedClient
{
	HTTPClientContext            clientContext;
	unsigned                     useKeepAlive;
	HTTPCookieContext*           cookieContext;
	HTTPAuthContext*             authContext;
	const HTTP_CHAR*             userAgent;
	const HTTP_CHAR*             acceptTypes;
	HTTP_INT32                   writeBufferSize;
	HTTP_INT32                   maxTotalConnections;
	HTTP_INT32                   maxHostConnections;
	DLListNode                   cachedSessions;
	HTTP_INT32                   maxSessionIdleMsecs;
#ifdef HTTP_MANAGED_CLIENT_THREADSAFE
	RTP_MUTEX                    lock;
#endif
};

struct s_HTTPResponseInfo
{
	HTTP_INT16                   status;
	FileContentType              contentType;
	HTTP_INT32                   contentLength;
	HTTP_CHAR*                   charset;
	RTP_TIMESTAMP                date;
	RTP_TIMESTAMP                expires;
	RTP_TIMESTAMP                lastModified;
//	HTTPCacheControlType         cacheControl;
	HTTP_CHAR*                   location;
	HTTP_CHAR*                   authRealm;
	HTTPAuthStatus               authStatus;
};

#ifdef __cplusplus
extern "C" {
#endif

int HTTP_ManagedClientInit (
		HTTPManagedClient* client,
		const HTTP_CHAR* userAgent,
		const HTTP_CHAR* acceptTypes,
		unsigned useKeepAlive,
		HTTPCookieContext* cookieContext,
		HTTPAuthContext* authContext,
		RTP_HANDLE sslContext,
		unsigned sslEnabled,
		HTTP_INT32 writeBufferSize,
		HTTP_INT32 maxTotalConnections,
		HTTP_INT32 maxHostConnections
	);

#ifdef HTTP_MANAGED_CLIENT_THREADSAFE
int HTTP_ManagedClientInitMT (
		HTTPManagedClient* client,
		const HTTP_CHAR* userAgent,
		const HTTP_CHAR* acceptTypes,
		unsigned useKeepAlive,
		HTTPCookieContext* cookieContext,
		HTTPAuthContext* authContext,
		RTP_HANDLE sslContext,
		unsigned sslEnabled,
		HTTP_INT32 writeBufferSize,
		HTTP_INT32 maxTotalConnections,
		HTTP_INT32 maxHostConnections,
		RTP_MUTEX lock
	);
#endif

void HTTP_ManagedClientDestroy (
		HTTPManagedClient* client
	);

int HTTP_ManagedClientStartTransaction (
		HTTPManagedClient* client,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
        HTTP_INT16 ipType,
		HTTPManagedSessionType type,
		unsigned blocking,
		HTTPManagedClientSession** session
	);

int HTTP_ManagedClientGet (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince
	);

int HTTP_ManagedClientPut (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* path,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength
	);

int HTTP_ManagedClientPost (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* path,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength
	);

int HTTP_ManagedClientRequestEx (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength,
		HTTPRequest* request
	);

int HTTP_ManagedClientRequestHeaderDone (
		HTTPManagedClientSession* session,
		HTTPRequest* request
	);

HTTP_INT32 HTTP_ManagedClientWrite (
		HTTPManagedClientSession* session,
		HTTP_UINT8* buffer,
		HTTP_INT32 size
	);

int HTTP_ManagedClientWriteDone (
		HTTPManagedClientSession* session
	);

int HTTP_ManagedClientReadResponseInfo (
		HTTPManagedClientSession* session,
		HTTPResponseInfo* responseInfo
	);

int HTTP_ManagedClientReadResponseInfoEx (
		HTTPManagedClientSession* session,
		HTTPResponseInfo* responseInfo,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData
	);

int HTTP_ManagedClientReadSelect (
		HTTPManagedClientSession* session,
		HTTP_INT32 timeoutMsec
	);

int HTTP_ManagedClientWriteSelect (
		HTTPManagedClientSession* session,
		HTTP_INT32 timeoutMsec
	);

int HTTP_ManagedClientSelect (
		HTTPManagedClientSession** writeList,
		HTTP_INT16* writeNum,
		HTTPManagedClientSession** readList,
		HTTP_INT16* readNum,
		HTTPManagedClientSession** errList,
		HTTP_INT16* errNum,
		HTTP_INT32 timeoutMsec
	);

HTTP_INT32 HTTP_ManagedClientRead (
		HTTPManagedClientSession* session,
		HTTP_UINT8* buffer,
		HTTP_INT32 size
	);

HTTP_INT32 HTTP_ManagedClientReadFrom (
		HTTPManagedClientSession* session,
		HTTPResponseInfo* responseInfo,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData,
		HTTP_UINT8* buffer,
		HTTP_INT32 size,
		RTP_NET_ADDR* fromAddr
	);

void HTTP_ManagedClientFinishTransaction (
		HTTPManagedClientSession* session
	);

void HTTP_ManagedClientCloseSession (
		HTTPManagedClientSession* session
	);

void HTTP_ManagedClientCloseAll (
		HTTPManagedClient* client
	);

void HTTP_ManagedClientCloseHost (
		HTTPManagedClient* client,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port
	);

void HTTP_ManagedClientCloseStale (
		HTTPManagedClient* client
	);

RTP_SOCKET HTTP_ManagedClientGetSessionSocket (
		HTTPManagedClientSession* session
	);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPMCLI_H__ */
