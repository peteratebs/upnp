//
// HTTPSRV.H - Portable HTTP server implementation
//
// EBS - HTTP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __HTTPSRV_H__
#define __HTTPSRV_H__


#define HTTP_SERVER_MULTITHREAD
//#define HTTP_SERVER_KEEP_ALIVE

#include "httpp.h"
#include "rtpnet.h"

#ifdef HTTP_SERVER_MULTITHREAD
#include "rtpsignl.h"
#include "rtphelper.h"
#endif /* HTTP_SERVER_MULTITHREAD */

typedef RTP_HANDLE HTTP_SOCKET;

#define HTTP_SERVER_NAME_LEN         32
#define HTTP_SERVER_PATH_LEN         256
#define HTTP_SERVER_DEFAULT_FILE_LEN 32
#define HTTP_SERVER_MAX_SESSIONS     32

typedef enum e_HTTPAuthScheme
{
	HTTP_UNKNOWN_AUTH_SCHEME = -1,
	HTTP_AUTH_SCHEME_BASIC = 0,
	HTTP_AUTH_SCHEME_DIGEST,
	HTTP_NUM_AUTH_SCHEMES
}
HTTPAuthScheme;

typedef struct s_HTTPClientInfo
{
	HTTP_UINT8         ipAddr[RTP_NET_IP_ALEN];
	HTTP_UINT8         ipMask[RTP_NET_IP_ALEN];
	HTTP_UINT8         ipType;
}
HTTPClientInfo;

typedef struct s_HTTPAuthRealmPath
{
	const HTTP_CHAR   *path;
	HTTP_BOOL          exactMatch;
}
HTTPAuthRealmPath;

typedef struct s_HTTPAuthRealmUser
{
	const HTTP_CHAR   *userName;
	const HTTP_CHAR   *password;
}
HTTPAuthRealmUser;

typedef enum e_HTTPRequestHandlerStatus
{
	HTTP_REQUEST_STATUS_DONE = 0,
	HTTP_REQUEST_STATUS_CONTINUE = 1
}
HTTPRequestHandlerStatus;

typedef enum e_HTTPServerStatus
{
	HTTP_SERVER_NOT_READY = 0,
	HTTP_SERVER_INITIALIZED
}
HTTPServerStatus;

typedef struct s_HTTPServerConnection
{
#ifdef HTTP_SERVER_MULTITHREAD
	RTP_HELPER_THREAD_JOB job;
	struct s_HTTPServerContext* server;
#endif /* HTTP_SERVER_MULTITHREAD */
	RTP_SOCKET            socket;
	RTP_NET_ADDR          clientAddr;
}
HTTPServerConnection;

typedef struct s_HTTPServerContext
{
	/*-----------------------------------------------*
     * the socket is allocated when the server is    *
     * initialized and never touched again until it  *
     * is closed when the server is freed            */
	HTTP_SOCKET           serverSocket;
    /*                                               *
	 *-----------------------------------------------*/

	/*-----------------------------------------------*
	 * this data is read/write when server daemon is *
	 * not running, read-only when it is             */
	HTTP_CHAR             serverName[HTTP_SERVER_NAME_LEN];
	RTP_NET_ADDR          serverAddress;
	HTTP_INT8             httpMajorVersion;
	HTTP_INT8             httpMinorVersion;
	HTTP_BOOL             allowKeepAlive;
	HTTP_CHAR             rootDir[HTTP_SERVER_PATH_LEN];
	HTTP_CHAR             defaultFile[HTTP_SERVER_DEFAULT_FILE_LEN];
	DLListNode            pathList;
	DLListNode            realmList;
	HTTPClientInfo*       allowedClientList;
	HTTP_INT16            numAllowedClients;
	HTTPClientInfo*       blockedClientList;
	HTTP_INT16            numBlockedClients;
    /*                                               *
	 *-----------------------------------------------*/

	/*-----------------------------------------------*
	 * this data is accessed/modified by multiple    *
	 * threads when the server daemon is running, so *
	 * is protected by 'lock'                        */
	HTTPServerStatus      status;
	HTTP_INT16            maxConnections;

#ifdef HTTP_SERVER_MULTITHREAD

	RTP_MUTEX             lock;
	HTTP_INT16            maxHelperThreads;
	RTP_HELPER_THREAD_CTX helperContext;
	HTTPServerConnection* jobFreeList;
	HTTPServerConnection* connectionContextArray;

#endif /* HTTP_SERVER_MULTITHREAD */


#ifdef HTTP_SERVER_KEEP_ALIVE

	DLListNode            openConnections;

#endif /* HTTP_SERVER_KEEP_ALIVE */

    /*                                               *
	 *-----------------------------------------------*/
}
HTTPServerContext;

typedef struct s_HTTPServerRequestContext HTTPServerRequestContext;

typedef int (*HTTPServerHeaderCallback)(
		void *userData,
		HTTPServerRequestContext *ctx,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value
	);

struct s_HTTPServerRequestContext
{
	HTTPServerContext*        server;
	RTP_SOCKET                socketHandle;
	int                       keepAlive;
	int                       connectionClosed;
	int                       ifModifiedSince;
	int                       ifUnmodifiedSince;
	RTP_TIMESTAMP             modifiedTime;
	RTP_TIMESTAMP             unmodifedTime;
	HTTPServerHeaderCallback  userCallback;
	void*                     userData;
}
;

/* Request Handler return values: tbd */
typedef int (*HTTPRequestHandler)(
		void *userData,
		HTTPServerRequestContext *ctx,
		HTTPSession *session,
		HTTPRequest *request,
		RTP_NET_ADDR *clientAddr
	);

typedef void (*HTTPServerPathDestructor)(
		void *userData
	);

typedef struct s_HTTPServerPath
{
	DLListNode                node;
	HTTPRequestHandler        fn;
	HTTPServerPathDestructor  dfn;
	void                     *userData;
	HTTP_BOOL                 exactMatch;
	HTTP_CHAR                 path[HTTP_SERVER_PATH_LEN];
}
HTTPServerPath;

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_ServerRemoveVirtualFile(X,Y)   HTTP_ServerRemovePath(X,Y)
#define HTTP_ServerRemovePostHandler(X,Y)   HTTP_ServerRemovePath(X,Y)
#define HTTP_ServerGetSocket(S)             ((S)->serverSocket)

/* just returns 0 (keep going) */
int HTTP_ServerProcessHeaderStub (
		void *userData,
		HTTPServerRequestContext *server,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value
	);

/* ------------------------------------------------------------------ */
/* general API functions */

int  HTTP_ServerInit                (HTTPServerContext *server,
                                     const HTTP_CHAR *name,
                                     const HTTP_CHAR *rootDir,
                                     const HTTP_CHAR *defaultFile,
                                     HTTP_INT8 httpMajorVersion,
                                     HTTP_INT8 httpMinorVersion,
                                     HTTP_UINT8 *ipAddr,
                                     HTTP_INT16 port,
									 HTTP_INT16 ipType,
                                     HTTP_BOOL allowKeepAlive,
                                     HTTPServerConnection* connectCtxArray,
                                     HTTP_INT16 maxConnections,
                                     HTTP_INT16 maxHelperThreads);

void HTTP_ServerDestroy             (HTTPServerContext *server,
                                     HTTPServerConnection** connectCtxArray);

int  HTTP_ServerAddAuthRealm        (HTTPServerContext *server,
                                     const HTTP_CHAR *realmName,
                                     HTTPAuthRealmPath *pathList,
                                     HTTP_INT16 numPaths,
                                     HTTPAuthRealmUser *userList,
                                     HTTP_INT16 numUsers,
                                     HTTPAuthScheme scheme);

int  HTTP_ServerRemoveAuthRealm     (HTTPServerContext *server,
                                     const HTTP_CHAR *realmName);

int  HTTP_ServerSetAllowedClients   (HTTPServerContext *server,
                                     HTTPClientInfo *clientList,
                                     HTTP_INT16 numClients);

int  HTTP_ServerSetBlockedClients   (HTTPServerContext *server,
                                     HTTPClientInfo *clientList,
                                     HTTP_INT16 numClients);

int  HTTP_ServerAddPath             (HTTPServerContext *server,
                                     const HTTP_CHAR *path,
                                     HTTP_BOOL exactMatch,
                                     HTTPRequestHandler fn,
                                     HTTPServerPathDestructor dfn,
                                     void *userData);

int  HTTP_ServerRemovePath          (HTTPServerContext *server,
                                     const HTTP_CHAR *path);

int  HTTP_ServerAddVirtualFile      (HTTPServerContext *server,
                                     const HTTP_CHAR *path,
                                     const HTTP_UINT8 *data,
                                     HTTP_INT32 size,
                                     const HTTP_CHAR *contentType);

int  HTTP_ServerAddPostHandler      (HTTPServerContext *server,
                                     const HTTP_CHAR *path,
                                     HTTPRequestHandler fn,
                                     void *userData);

int  HTTP_ServerProcessOneRequest   (HTTPServerContext *server,
                                     HTTP_INT32 timeoutMsec);

int  HTTP_ServerStopHelperThreads   (HTTPServerContext *server,
                                     HTTP_INT32 secTimeout);

HTTP_UINT16 HTTP_ServerGetPort      (HTTPServerContext *server);

/* ------------------------------------------------------------------ */
/* request handler API functions */

int  HTTP_ServerInitResponse        (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPResponse *response,
                                     HTTP_INT16 httpStatus,
                                     const HTTP_CHAR *httpMessage);

int  HTTP_ServerSetDefaultHeaders   (HTTPServerRequestContext *ctx,
                                     HTTPResponse *response);

int  HTTP_ServerSendError           (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTP_INT16 httpStatus,
                                     const HTTP_CHAR *httpMessage);

int  HTTP_ServerValidateRequest     (HTTPServerRequestContext *ctx,
                                     HTTPRequest *request,
                                     HTTP_CHAR *authorization);

int  HTTP_ServerSendAuthChallenge   (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPRequest *request,
                                     const HTTP_CHAR *realmName);

int  HTTP_ServerConnectSetKeepAlive (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTP_BOOL doKeepAlive);

int  HTTP_ServerConnectionClose     (HTTPServerRequestContext *ctx,
                                     HTTPSession *session);

int  HTTP_ServerReadHeaders         (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPServerHeaderCallback processHeader,
                                     void *userData,
                                     HTTP_UINT8 *buffer,
                                     HTTP_INT32 size);

int  HTTP_ServerRequestGetLocalAddr (HTTPServerRequestContext *ctx,
                                     RTP_NET_ADDR *local);

/* ------------------------------------------------------------------ */
/* internal helper functions NOT to be used outside the http server */

int             _HTTP_ServerValidateClient      (HTTPServerContext *server,
                                                 RTP_NET_ADDR *clientAddr);


int             _HTTP_ServerSendError           (HTTPServerRequestContext *ctx,
                                                 HTTPSession *session,
                                                 HTTP_INT16 httpStatus,
                                                 const HTTP_CHAR *httpMessage);

int             _HTTP_ServerInitRequestContext  (HTTPServerContext *server,
                                                 HTTPServerRequestContext *ctx);


int             _HTTP_ServerHandleRequest       (HTTPServerRequestContext *ctx,
							                     HTTPSession *session,
                                                 HTTPRequest *request,
                                                 RTP_NET_ADDR *clientAddr);

HTTPServerPath* _HTTP_ServerFindMatchingPath    (HTTPServerContext *server,
                                                 const HTTP_CHAR *path);

int             _HTTP_ServerQueueRequest        (HTTPServerContext *server,
                                                 RTP_SOCKET sock,
                                                 RTP_NET_ADDR *clientAddr);

int             _HTTP_ServerWaitRequest         (HTTPServerContext *server,
                                                 RTP_SOCKET *sock,
                                                 RTP_NET_ADDR *clientAddr,
                                                 HTTP_INT32 msecTimeout);


//typedef HTTP_INT32 (*WriteFn) (void* requestStream, const HTTP_CHAR* data, HTTP_INT32 len);
//typedef HTTP_INT32 (*WriteString) (void* privateData, WriteFn writeFn, void* requestStream);

typedef void (*HTTPServerReadStreamPeekCallback)(
		RTP_SOCKET                sockHandle,
		HTTP_CHAR *pData,
		HTTP_INT32 nBytes);

typedef void (*HTTPServerWriteStreamPeekCallback)(
		RTP_SOCKET                sockHandle,
		HTTP_CHAR *pData,
		HTTP_INT32 nBytes);


#ifdef __cplusplus
}
#endif

#endif /* __HTTPSRV_H__ */
