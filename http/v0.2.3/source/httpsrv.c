//
// HTTPSRV.C - HTTP Server
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

#include "httpsrv.h"
#include "stdlib.h"
#include "rtpdate.h"
#include "rtpfile.h"
#include "rtpthrd.h"
#include "rtpdobj.h"
#include "rtpstr.h"
#include "rtplog.h"
#include "rtpsignl.h"
#include "filetype.h"
#include "fileext.h"
#include "assert.h"

#include "rtpprint.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTP_SERVER_QUEUE_SIZE             10
#define HTTP_SERVER_ENTER_CRITICAL(S)      rtp_sig_mutex_claim((S)->lock)
#define HTTP_SERVER_EXIT_CRITICAL(S)       rtp_sig_mutex_release((S)->lock)
#define HTTP_ASSERT(X)                     assert(X)

#define HTTP_MASTER_PRIORITY               0 /* normal */
#define HTTP_HELPER_PRIORITY               0 /* normal */
#define HTTP_SINGLE_PRIORITY               0 /* normal */
#define HTTP_MASTER_STACK_SIZE            -1 /* small */
#define HTTP_HELPER_STACK_SIZE             1 /* large */
#define HTTP_SINGLE_STACK_SIZE             1 /* large */

#define UPPERCASE(X) (((X) >= 'a' || (X) <= 'z')? ((X) + 'A' - 'a') : (X))

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_HTTPServerAuthRealm
{
	DLListNode                node;
	DLListNode                activeNonceList;
	HTTPAuthRealmPath        *pathList;
	HTTPAuthRealmUser        *userList;
	HTTPAuthScheme            scheme;
	HTTP_INT16                numPaths;
	HTTP_INT16                numUsers;
	HTTP_CHAR                 realmName[HTTP_SERVER_NAME_LEN];
}
HTTPServerAuthRealm;

typedef struct s_HTTPVirtualFileInfo
{
	const HTTP_UINT8         *data;
	HTTP_INT32                size;
	const HTTP_CHAR          *contentType;
	const HTTP_CHAR          *contentEncoding;
	RTP_TIMESTAMP             creationTime;
}
HTTPVirtualFileInfo;

typedef struct s_RtpNetSockStruct
{
	RTP_SOCKET                sockHandle;
	int                       serverKeepAlive; /* 1 = yes, 0 = no */
	int                       keepAlive;       /* -1 = no, 1 = yes, 0 = no preference */
}
RtpNetSockStruct;


/*****************************************************************************/
// Data
/*****************************************************************************/

const HTTP_CHAR *gpHTTPAuthSchemeName[HTTP_NUM_AUTH_SCHEMES] =
{
	"Basic",
	"Digest"
};

const char HTTP_SERVER_NAME_STRING[]  = "EBS MicroWeb-Pro Server v0.0";
const char HTTP_SERVER_DEFAULT_FILE[] = "index.html";


/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

#ifdef HTTP_SERVER_MULTITHREAD
void            _HTTP_ServerExecuteJob          (RTP_HELPER_THREAD_JOB* job);
#endif //HTTP_SERVER_MULTITHREAD

int             HTTP_ServerProcessOneConnection (HTTPServerContext *server,
                                                 RTP_SOCKET sockHandle,
                                                 RTP_NET_ADDR *clientAddr);

int             _HTTP_VirtualFileRequestHandler (void *userData,
                                                 HTTPServerRequestContext *ctx,
							                     HTTPSession *session,
                                                 HTTPRequest *request,
                                                 RTP_NET_ADDR *clientAddr);

int             _HTTP_VirtualFileProcessHeader  (void *userData,
                                                 HTTPServerRequestContext *ctx,
                                                 HTTPSession *session,
                                                 HTTPHeaderType type,
                                                 const HTTP_CHAR *name,
                                                 const HTTP_CHAR *value);

int             _HTTP_ServerAddPath             (HTTPServerContext *server,
                                                 const HTTP_CHAR *path,
                                                 HTTP_BOOL exactMatch,
                                                 HTTPRequestHandler fn,
                                                 HTTPServerPathDestructor dfn,
                                                 void *userData);

int             _HTTP_ServerRemovePath          (HTTPServerContext *server,
                                                 const HTTP_CHAR *path);

int             _HTTP_ServerReadHeaders         (HTTPServerRequestContext *ctx,
                                                 HTTPSession *session,
                                                 HTTPServerHeaderCallback processHeader,
                                                 void *userData,
                                                 HTTP_UINT8 *buffer,
                                                 HTTP_INT32 size);

int             _HTTP_ServerInitResponse        (HTTPServerRequestContext *ctx,
                                                 HTTPSession *session,
                                                 HTTPResponse *response,
                                                 HTTP_INT16 httpStatus,
                                                 const HTTP_CHAR *httpMessage);

int             _HTTP_ServerSetDefaultHeaders   (HTTPServerRequestContext *ctx,
                                                 HTTPResponse *response);

void            _HTTP_ConstructLocalPath        (HTTPServerContext *server,
                                                 HTTP_CHAR *fsPath,
												 HTTP_CHAR *targetURL,
												 HTTP_INT32 maxChars);

HTTP_BOOL       _HTTP_ServerMatchPath           (const HTTP_CHAR *pattern,
                                                 const HTTP_CHAR *path,
                                                 HTTP_BOOL exactMatch);

static void     _HTTP_ServerCollapsePath        (HTTP_CHAR *path);

int             _HTTP_ServerHeaderCallback      (void *userData,
                                                 HTTPSession *session,
                                                 HTTPHeaderType type,
                                                 const HTTP_CHAR *name,
                                                 const HTTP_CHAR *value);

static HTTPServerAuthRealm *_HTTP_AllocServerAuthRealm (void);
static void                 _HTTP_FreeServerAuthRealm  (HTTPServerAuthRealm *realm);
static HTTPServerPath      *_HTTP_AllocServerPath      (void);
static void                 _HTTP_FreeServerPath       (HTTPServerPath *path);
static HTTPVirtualFileInfo *_HTTP_AllocVirtualFileInfo (void);
static void                 _HTTP_FreeVirtualFileInfo  (HTTPVirtualFileInfo *vfile);

#ifdef HTTP_SERVER_KEEP_ALIVE
static HTTPConnectionInfo  *_HTTP_AllocConnectionInfo  (void);
static void                 _HTTP_FreeConnectionInfo   (HTTPConnectionInfo *con);
#endif


/*****************************************************************************/
//
/*****************************************************************************/

HTTP_INT32 RtpNetSockReadFn (
		void *streamPtr,
		HTTP_UINT8 *buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	);

HTTP_INT32 RtpNetSockWriteFn (
		void *streamPtr,
		const HTTP_UINT8 *buffer,
		HTTP_INT32 size
	);


/*****************************************************************************/
// Server functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
  HTTP_ServerInit
  ---------------------------------------------------------------------------*/
/** @memo Initialize an HTTP Managed Server instance.

    @doc  This function must be called once before any other managed server calls
          to create a context in which the server will operate.

    @see  HTTP_ServerDestroy

    @return 0 on success, negative on failure
 */

int  HTTP_ServerInit                (HTTPServerContext *server,			    /**     pointer to uninitialized \Ref{HTTPServerContext} struct */
                                     const HTTP_CHAR *name,				    /**     Server name. Sent in HTTP header. */
                                     const HTTP_CHAR *rootDir,			    /**     Path to the root directory for content. */
                                     const HTTP_CHAR *defaultFile,		    /**     Default Filename if "\" or "path\" is passed with no file name. */
                                     HTTP_INT8 httpMajorVersion,		    /**     Normally 1 for HTTP 1.1 */
                                     HTTP_INT8 httpMinorVersion,		    /**     Normally 1 for HTTP 1.1 */
                                     HTTP_UINT8 *ipAddr,				    /**     V4/V6 examples: ipaddr[4] = {192, 168, 0, 6}; ipAddr = "fe80::20b:dbff:fe2f:c162";                                     */
                                     HTTP_INT16 port,					    /**     port number (80) usually */
									 HTTP_INT16 ipType,					    /**     ipversion 4 or 6       */
                                     HTTP_BOOL allowKeepAlive,			    /**     If HTTP version < 1.1 enables keep-alive support. For 1,1 and greater keep-alive is the default.*/
                                     HTTPServerConnection* connectCtxArray,	/**     pointer to uninitialized bytes, must be (sizeof(HTTPServerConnection) * maxConnections)) bytes.        */
                                     HTTP_INT16 maxConnections,			    /**     The maximum limit on simultaneous HTTP server connections         */
                                     HTTP_INT16 maxHelperThreads		    /**     If HTTP_SERVER_MULTITHREAD is defined, the max number of helper threads to spawn       */
                                     )
{
	rtp_memset((unsigned char *) server, 0, sizeof(HTTPServerContext));



	if (rtp_net_socket_stream_dual(&server->serverSocket, ipType) >= 0)
	{
		if (rtp_net_bind_dual(server->serverSocket, RTP_NET_STREAM, ipAddr, port, ipType) >= 0 &&
		    rtp_net_listen(server->serverSocket, maxConnections) >= 0)
		{
			if (!ipAddr)
			{
				if (rtp_net_getsockname(server->serverSocket, server->serverAddress.ipAddr,
										&server->serverAddress.port, &server->serverAddress.type) < 0)
				{
					rtp_memset(server->serverAddress.ipAddr, 0, RTP_NET_IP_ALEN);
				}
			}
			else
			{
				rtp_memcpy(server->serverAddress.ipAddr, ipAddr, RTP_NET_IP_ALEN);
				server->serverAddress.port = port;
			}

			if (name)
			{
				rtp_strncpy(server->serverName, name, HTTP_SERVER_NAME_LEN-1);
			}
			else
			{
				rtp_strncpy(server->serverName, HTTP_SERVER_NAME_STRING, HTTP_SERVER_NAME_LEN-1);
			}

			server->httpMajorVersion = httpMajorVersion;
			server->httpMinorVersion = httpMinorVersion;

			if (rootDir)
			{
				rtp_strncpy(server->rootDir, rootDir, HTTP_SERVER_PATH_LEN-1);
			}
			else
			{
				/* default to the current working directory */
				server->rootDir[0] = '.';
				server->rootDir[1] = 0;
			}

			if (defaultFile)
			{
				rtp_strncpy(server->defaultFile, defaultFile, HTTP_SERVER_DEFAULT_FILE_LEN-1);
			}
			else
			{
				rtp_strncpy(server->defaultFile, HTTP_SERVER_DEFAULT_FILE, HTTP_SERVER_DEFAULT_FILE_LEN-1);
			}

			if (!allowKeepAlive ||
			    (server->httpMajorVersion == 1 && server->httpMinorVersion > 0) ||
				(server->httpMajorVersion > 1))
			{
				server->allowKeepAlive = allowKeepAlive;
				server->maxConnections = maxConnections;

				DLLIST_INIT(&server->pathList);
				DLLIST_INIT(&server->realmList);

				server->allowedClientList = 0;
				server->numAllowedClients = 0;

				server->blockedClientList = 0;
				server->numBlockedClients = 0;

			  #ifdef HTTP_SERVER_KEEP_ALIVE
				DLLIST_INIT(&server->openConnections);
			  #endif /* HTTP_SERVER_KEEP_ALIVE */

			  #ifdef HTTP_SERVER_MULTITHREAD
				server->connectionContextArray = connectCtxArray;
				server->maxHelperThreads = maxHelperThreads;

				/* allocate the server lock */
				if (rtp_sig_mutex_alloc(&server->lock, 0) >= 0)
				{
					RTP_MUTEX helperThreadMutex;
					RTP_SEMAPHORE helperThreadSemaphore;

					/* initialize the session queue */
					if (rtp_sig_mutex_alloc(&helperThreadMutex, 0) >= 0)
					{
						if (rtp_sig_semaphore_alloc(&helperThreadSemaphore, 0) >= 0)
						{
							if (rtp_helper_thread_ctx_init (
									&server->helperContext,
									helperThreadMutex,
									helperThreadSemaphore
								) >= 0)
							{
								int n;
								// initialize the free job list

								server->jobFreeList = connectCtxArray;

								for (n = 0; n < maxConnections-1; n++)
								{
									connectCtxArray[n].job.next = &(connectCtxArray[n+1].job);
									connectCtxArray[n].job.execute = _HTTP_ServerExecuteJob;
									connectCtxArray[n].server = server;
								}
                                connectCtxArray[n].job.next = 0;
                                connectCtxArray[n].job.execute = _HTTP_ServerExecuteJob;
                                connectCtxArray[n].server = server;

								server->status = HTTP_SERVER_INITIALIZED;
								return (0);
							}

							rtp_sig_semaphore_free(helperThreadSemaphore);
						}

						rtp_sig_mutex_free(helperThreadMutex);
					}

					rtp_sig_mutex_free(server->lock);
				}

			  #else  /* HTTP_SERVER_MULTITHREAD */

				//server->queueFirst = 0;
				//server->queueLast = 0;
				server->status = HTTP_SERVER_INITIALIZED;
				return (0);

			  #endif /* HTTP_SERVER_MULTITHREAD */
			}
		}

		rtp_net_closesocket(server->serverSocket);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ServerDestroy
  ---------------------------------------------------------------------------*/
/** @memo Destroy an HTTP Managed Server instance.

    @doc  This function must be called to release sockets, semaphores and treads
          owned by this instance of the server. This function does not de-allocate the
		  HTTPServerContext or the server's connectCtxArray, that must be done explicitly.

    @see  HTTP_ServerInit

    @return nothing
 */
void HTTP_ServerDestroy (
		HTTPServerContext* server, /**     Pointer to structure that was initialized by HTTP_ServerInit */
		HTTPServerConnection** connectCtxArray /** The server's contextArray is returned in the variable. Call rtp_free() to release it.*/
	)
{
    rtp_net_closesocket(server->serverSocket);

  #ifdef HTTP_SERVER_MULTITHREAD
    {
        RTP_MUTEX helperThreadMutex;
	    RTP_SEMAPHORE helperThreadSemaphore;
        if (connectCtxArray)
        {
	        *connectCtxArray = server->connectionContextArray;
        }
        rtp_sig_mutex_free(server->lock);

        rtp_helper_thread_ctx_destroy (
		        &server->helperContext,
		        &helperThreadMutex,
		        &helperThreadSemaphore
	        );

        rtp_sig_mutex_free(helperThreadMutex);
        rtp_sig_semaphore_free(helperThreadSemaphore);
    }
  #endif /* HTTP_SERVER_MULTITHREAD */

	rtp_memset((unsigned char *) server, 0, sizeof(HTTPServerContext));
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerAddAuthRealm      (HTTPServerContext *server,
                                   const HTTP_CHAR *realmName,
                                   HTTPAuthRealmPath *pathList,
                                   HTTP_INT16 numPaths,
                                   HTTPAuthRealmUser *userList,
                                   HTTP_INT16 numUsers,
                                   HTTPAuthScheme scheme)
{
HTTPServerAuthRealm *realm;

	/* first try to find an existing realm with the same name */
	realm = (HTTPServerAuthRealm *) server->realmList.next;
	while (realm != (HTTPServerAuthRealm *) &server->realmList)
	{
		if (!rtp_strcmp(realmName, realm->realmName))
		{
			/* already exists; first need to remove it */
			return (-1);
		}
		realm = (HTTPServerAuthRealm *) realm->node.next;
	}

	realm = _HTTP_AllocServerAuthRealm();
	if (realm)
	{
		DLLIST_INSERT_BEFORE(&server->realmList, &realm->node);
		DLLIST_INIT(&realm->activeNonceList);
		realm->pathList = pathList;
		realm->userList = userList;
		realm->scheme   = scheme;
		realm->numPaths = numPaths;
		realm->numUsers = numUsers;
		rtp_strncpy(realm->realmName, realmName, HTTP_SERVER_NAME_LEN-1);
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerRemoveAuthRealm   (HTTPServerContext *server,
                                   const HTTP_CHAR *realmName)
{
HTTPServerAuthRealm *realm;

	realm = (HTTPServerAuthRealm *) server->realmList.next;
	while (realm != (HTTPServerAuthRealm *) &server->realmList)
	{
		if (!rtp_strcmp(realmName, realm->realmName))
		{
			/* found it */
			DLLIST_REMOVE(&realm->node);
			_HTTP_FreeServerAuthRealm(realm);
			return (0);
		}
		realm = (HTTPServerAuthRealm *) realm->node.next;
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerSetAllowedClients (HTTPServerContext *server,
                                   HTTPClientInfo *clientList,
                                   HTTP_INT16 numClients)
{
	server->allowedClientList = clientList;
	server->numAllowedClients = numClients;
	return (0);
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerSetBlockedClients (HTTPServerContext *server,
                                   HTTPClientInfo *clientList,
                                   HTTP_INT16 numClients)
{
	server->blockedClientList = clientList;
	server->numBlockedClients = numClients;
	return (0);
}

/*---------------------------------------------------------------------------
  HTTP_ServerAddPath
  ---------------------------------------------------------------------------*/
/** @memo Add a URL and service callback functions to when the URL is accessed from a client.

    @doc  This function assigns a callback handler for the URL. When a client accesses the URL the callbaack
    function is called. It processes the incoming request and formats and sends a reply to the client.
    This method may be used to reply with live data for HTTP GET requests. It can also be used for processing
    HTTP POST requests. Study the HTTP server sample code for example uses of this function and the callbacks.

    @see  HTTP_ServerRemovePath

    @return 0 on success, negative on failure
 */
int  HTTP_ServerAddPath           (HTTPServerContext *server, /**     Pointer to structure that was initialized by HTTP_ServerInit */
                                   const HTTP_CHAR *path,	  /**     The URL */
                                   HTTP_BOOL exactMatch,	  /**     If true the path must match the URL exactly , if false, match when all characters in the path match the URL, even if the URL has more characters */
                                   HTTPRequestHandler fn,	  /**     Function to call when the URL is accessed */
								   HTTPServerPathDestructor dfn, /** Optional function to call when the URL is removed or the server context is released */
                                   void *userData  /** Optional user data. This is passed to the request handler when the URL is accessed */
)
{
	return (_HTTP_ServerAddPath(server, path, exactMatch, fn, dfn, userData));
}

/*---------------------------------------------------------------------------*/
int  _HTTP_ServerAddPath          (HTTPServerContext *server,
                                   const HTTP_CHAR *path,
                                   HTTP_BOOL exactMatch,
                                   HTTPRequestHandler fn,
								   HTTPServerPathDestructor dfn,
                                   void *userData)
{
HTTPServerPath *serverPath;

	/* first try to find an existing path */
	serverPath = (HTTPServerPath *) server->pathList.next;
	while (serverPath != (HTTPServerPath *) &server->pathList)
	{
		/* path name space is case-preserving, case-insensitive */
		if (!rtp_stricmp(path, serverPath->path))
		{
			/* already exists; first need to remove it */
			return (-1);
		}
		serverPath = (HTTPServerPath *) serverPath->node.next;
	}

	serverPath = _HTTP_AllocServerPath();
	if (serverPath)
	{
		DLLIST_INSERT_BEFORE(&server->pathList, &serverPath->node);
		serverPath->fn = fn;
		serverPath->dfn = dfn;
		serverPath->userData = userData;
		serverPath->exactMatch = exactMatch;
		rtp_strncpy(serverPath->path, path, HTTP_SERVER_PATH_LEN-1);
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ServerRemovePath
  ---------------------------------------------------------------------------*/
/** @memo Release a URL service callback function that was assigned by HTTP_ServerAddPath

    @doc  This function releases internal resources required to maintain a link between the URL and the request handler.
	It also calls the destructor function prvided to HTTP_ServerAddPath to allow the user to free any resources that are sopecific to
	this URL.

    @see  HTTP_ServerAddPath

    @return 0 on success, negative on failure
 */
int  HTTP_ServerRemovePath        (HTTPServerContext *server,
                                   const HTTP_CHAR *path)
{
	return (_HTTP_ServerRemovePath(server, path));
}

/*---------------------------------------------------------------------------*/
int  _HTTP_ServerRemovePath       (HTTPServerContext *server,
                                   const HTTP_CHAR *path)
{
HTTPServerPath *serverPath;

	serverPath = (HTTPServerPath *) server->pathList.next;
	while (serverPath != (HTTPServerPath *) &server->pathList)
	{
		if (!rtp_strcmp(path, serverPath->path))
		{
			/* found it */
			if (serverPath->dfn)
			{
				serverPath->dfn(serverPath->userData);
			}
			DLLIST_REMOVE(&serverPath->node);
			_HTTP_FreeServerPath(serverPath);
			return (0);
		}
		serverPath = (HTTPServerPath *) serverPath->node.next;
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ServerAddVirtualFile
  ---------------------------------------------------------------------------*/
/** @memo Assign a memory array to be returned when the URL is requested.

    @doc  With this function you can assign a memory array to be returned when the URL is retrieved by a client.


    @return 0 on success, negative on failure
 */
int  HTTP_ServerAddVirtualFile    (HTTPServerContext *server,  /**     Pointer to structure that was initialized by HTTP_ServerInit */
                                   const HTTP_CHAR *path,	   /**     The URL */
                                   const HTTP_UINT8 *data,	   /**     The content to return when GET is performed on the URL. */
                                   HTTP_INT32 size,			   /**     Size of the content */
                                   const HTTP_CHAR *contentType) /**   Content type: text/html, image/jpeg for example */
{
	HTTPVirtualFileInfo *vfile = _HTTP_AllocVirtualFileInfo();

	if (vfile)
	{
		vfile->data = data;
		vfile->size = size;
		vfile->contentType = contentType;
		vfile->contentEncoding = 0;

		rtp_date_get_timestamp(&vfile->creationTime);

		if (_HTTP_ServerAddPath(server, path, 1,
		                        (HTTPRequestHandler)_HTTP_VirtualFileRequestHandler,
		                        (HTTPServerPathDestructor)_HTTP_FreeVirtualFileInfo,
		                        vfile) < 0)
		{
			_HTTP_FreeVirtualFileInfo(vfile);
			return (-1);
		}

		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ServerAddPostHandler
  ---------------------------------------------------------------------------*/
/** @memo Add a URL and service callback functions to when a client POSTs to the URL.

    @doc  This function assigns a callback handler for the URL. When a client posts to the URL the callbaack
    function is called. It processes the incoming request and formats and sends a reply to the client.
    This method may be used to process HTTP POST requests. Study the HTTP server sample code for example uses of this
    function and the callback.

    @see  HTTP_ServerAddPath

    @return 0 on success, negative on failure
 */
int  HTTP_ServerAddPostHandler    (HTTPServerContext *server, /**     Pointer to structure that was initialized by HTTP_ServerInit */
                                   const HTTP_CHAR *path,	  /**     The URL */
                                   HTTPRequestHandler fn,	  /**     Function to call when the URL is accessed */
                                   void *userData/** Optional user data. This is passed to the request handler when the URL is accessed */
)
{
	if (_HTTP_ServerAddPath(server, path, 1,
	                        fn, 0, userData) < 0)
	{
		return (-1);
	}

	return (0);
}

/*---------------------------------------------------------------------------
  HTTP_ServerProcessOneRequest
  ---------------------------------------------------------------------------*/
/** @memo Wait for a client to connect and process one request.

    @doc  This function must be called to serve content. It processes one HTTP POST, PUT or GET request and then
    returs. In a multithreaded environment a background thread may be created that just calls this function continuously.
    A singlethreaded application must call this function from the main loop of the program.


    @return 0 if a request was process, negative if it timed out or failed.
 */
int  HTTP_ServerProcessOneRequest (HTTPServerContext *server, /**     Pointer to structure that was initialized by HTTP_ServerInit */
                                   HTTP_INT32 timeoutMsec)	  /**     Time to wait for a connection, 0 is forever */
{
RTP_SOCKET sockHandle;
RTP_NET_ADDR clientAddr;
int result = -1;

	if (rtp_net_read_select(server->serverSocket, timeoutMsec) < 0)
	{
		return (result);
	}

	if (rtp_net_accept(&sockHandle, server->serverSocket, clientAddr.ipAddr, &clientAddr.port, &clientAddr.type) < 0)
	{
		return (result);
	}

  #ifdef HTTP_SERVER_MULTITHREAD

	{
		HTTPServerConnection* connectionCtx = server->jobFreeList;

		if (connectionCtx)
		{
			server->jobFreeList = (HTTPServerConnection*) connectionCtx->job.next;
			connectionCtx->clientAddr = clientAddr;
			connectionCtx->socket = sockHandle;

			if (rtp_helper_thread_queue_job(&server->helperContext, &connectionCtx->job) >= 0)
			{
				rtp_helper_thread_start_cond(&server->helperContext, server->maxHelperThreads);
			}
            result = 0;
		}
		else
		{
			result = -1;
			/* do a hard close */
			rtp_net_setlinger(sockHandle, 1, 0);
			rtp_net_closesocket(sockHandle);
		}
	}

  #else /* HTTP_SERVER_SINGLETHREAD */
	result = HTTP_ServerProcessOneConnection(server, sockHandle, &clientAddr);
  #endif /* HTTP_SERVER_MULTITHREAD */

	return (result);
}

/*---------------------------------------------------------------------------*/
int HTTP_ServerStopHelperThreads   (HTTPServerContext *server,
                                    HTTP_INT32 secTimeout)
{
  #ifdef HTTP_SERVER_MULTITHREAD
	return (rtp_helper_thread_stop_all(&server->helperContext, secTimeout*1000));
  #else /* HTTP_SERVER_MULTITHREAD */
    return (0);
  #endif /* HTTP_SERVER_MULTITHREAD */
}


/*****************************************************************************/
// Request Handler API functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTP_ServerInitResponse      (HTTPServerRequestContext *ctx,
                                   HTTPSession *session,
                                   HTTPResponse *response,
                                   HTTP_INT16 httpStatus,
                                   const HTTP_CHAR *httpMessage)
{
	return (_HTTP_ServerInitResponse(ctx, session, response, httpStatus,
	                                 httpMessage));
}

/*---------------------------------------------------------------------------*/
int  _HTTP_ServerInitResponse     (HTTPServerRequestContext *ctx,
                                   HTTPSession *session,
                                   HTTPResponse *response,
                                   HTTP_INT16 httpStatus,
                                   const HTTP_CHAR *httpMessage)
{
	return (HTTP_InitResponse(session, response, ctx->server->httpMajorVersion,
	                          ctx->server->httpMinorVersion, httpStatus,
							  httpMessage));
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerSetDefaultHeaders (HTTPServerRequestContext *ctx,
                                   HTTPResponse *response)
{
	return (_HTTP_ServerSetDefaultHeaders(ctx, response));
}

/*---------------------------------------------------------------------------*/
int  _HTTP_ServerSetDefaultHeaders (HTTPServerRequestContext *ctx,
                                    HTTPResponse *response)
{
	RTP_TIMESTAMP currentTime;

	rtp_date_get_timestamp(&currentTime);

	HTTP_SetResponseHeaderTime(response, "Date", &currentTime);
	HTTP_SetResponseHeaderStr(response, "Server", ctx->server->serverName);
	if (ctx->keepAlive)
	{
		HTTP_SetResponseHeaderStr(response, "Connection", "keep-alive");
	}
	else
	{
		HTTP_SetResponseHeaderStr(response, "Connection", "close");
	}

	return (0);
}

/*****************************************************************************/
// Helper functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTP_ServerProcessOneConnection (HTTPServerContext *server,
                                      RTP_SOCKET sockHandle,
                                      RTP_NET_ADDR *clientAddr)
{
RtpNetSockStruct s;
HTTPRequest request;
HTTPSession session;
int result;
HTTP_UINT8 tempBuffer[1024];
HTTP_INT32 tempBufferSize = 1024;
int requestProcessed = 0;
HTTPServerPath *serverPath;
HTTP_INT32 len;
HTTPServerRequestContext ctx;

	s.sockHandle = sockHandle;

	_HTTP_ServerInitRequestContext(server, &ctx);

	ctx.socketHandle = sockHandle;

	s.serverKeepAlive = 0;
	s.keepAlive = 0;
	/* create an HTTP session */
	result = HTTP_InitSession (&session,
							   RtpNetSockReadFn,
							   RtpNetSockWriteFn,
							   &s);

	HTTP_SetWriteBufferSize(&session, 1452);

	if (result < 0)
	{
		/* Server error */
		_HTTP_ServerSendError (&ctx, &session, 500, "Internal Server Error");
		HTTP_FreeSession(&session);
		rtp_net_closesocket(s.sockHandle);
		return (-1);
	}

	/* validate the client by network address */
	if (_HTTP_ServerValidateClient(server, clientAddr) < 0)
	{
		_HTTP_ServerSendError (&ctx, &session, 403, "Forbidden");
		HTTP_FreeSession(&session);
		rtp_net_closesocket(s.sockHandle);
		return (-1);
	}

	/* Read the request */
	len = HTTP_ReadRequest(&session, &request, tempBuffer, tempBufferSize);

	if (len < 0)
	{
		/* Bad request */
		_HTTP_ServerSendError (&ctx, &session, 400, "Bad Request");
		HTTP_FreeSession(&session);
		rtp_net_closesocket(s.sockHandle);
		return (-1);
	}

	serverPath = _HTTP_ServerFindMatchingPath(server, request.target);


	if (serverPath)
	{
		switch (serverPath->fn(serverPath->userData, &ctx, &session, &request, clientAddr))
		{
			case HTTP_REQUEST_STATUS_DONE:
				requestProcessed = 1;
				break;

			case HTTP_REQUEST_STATUS_CONTINUE:
				break;
		}
	}

	if (!requestProcessed)
	{
		_HTTP_ServerHandleRequest(&ctx, &session, &request, clientAddr);
	}

	HTTP_FreeSession(&session);

	if (!ctx.connectionClosed)
	{
		/* tbd - add support for connection keep-alive */
		rtp_net_closesocket(s.sockHandle);
	}

	return (0);
}


/*---------------------------------------------------------------------------*/
int _HTTP_VirtualFileRequestHandler (void *userData,
                                     HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPRequest *request,
                                     RTP_NET_ADDR *clientAddr)
{
	HTTPVirtualFileInfo *vfile = (HTTPVirtualFileInfo *) userData;
	HTTPServerContext *server = ctx->server;

	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			HTTP_UINT8 tempBuffer[256];
			HTTP_INT32 tempBufferSize = 256;

			HTTP_ServerReadHeaders(
					ctx,
					session,
					 _HTTP_VirtualFileProcessHeader,
					 0,
					 tempBuffer,
					 tempBufferSize
				);

			HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
			HTTP_ServerSetDefaultHeaders(ctx, &response);
			HTTP_SetResponseHeaderStr(&response, "Content-Type", vfile->contentType);
			if (vfile->contentEncoding)
			{
				HTTP_SetResponseHeaderStr(&response, "Content-Encoding", vfile->contentEncoding);
			}
			HTTP_SetResponseHeaderInt(&response, "Content-Length", vfile->size);
			HTTP_SetResponseHeaderTime(&response, "Last-Modified", &vfile->creationTime);

			HTTP_WriteResponse(session, &response);
			HTTP_Write(session, vfile->data, vfile->size);
			HTTP_WriteFlush(session);

			HTTP_FreeResponse(&response);

			return (HTTP_REQUEST_STATUS_DONE);
		}

		case HTTP_METHOD_HEAD: /* tbd - implement HEAD method */
		default:
			return (HTTP_REQUEST_STATUS_CONTINUE);
	}
}

/*---------------------------------------------------------------------------*/
int _HTTP_VirtualFileProcessHeader(
		void *userData,
		HTTPServerRequestContext *server,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value)
{
	switch (type)
	{
		case HTTP_HEADER_IF_MODIFIED_SINCE:
			/* tbd */
			break;
	}

	return (0);
}


/*---------------------------------------------------------------------------*/
HTTP_INT32 RtpNetSockReadFn  (void *streamPtr, HTTP_UINT8 *buffer, HTTP_INT32 min, HTTP_INT32 max)
{
	HTTP_INT32 retval;
	RtpNetSockStruct *s = (RtpNetSockStruct *) streamPtr;
	retval = rtp_net_recv(s->sockHandle, buffer, max);
	RTP_LOG_WRITE("HTTP SERVER READ", buffer, retval)
	return (retval);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 RtpNetSockWriteFn (void *streamPtr, const HTTP_UINT8 *buffer, HTTP_INT32 size)
{
	RtpNetSockStruct *s = (RtpNetSockStruct *) streamPtr;
	RTP_LOG_WRITE("HTTP SERVER WRITE", buffer, size)
	return (rtp_net_send(s->sockHandle, buffer, size));
}

/*---------------------------------------------------------------------------*/
int _HTTP_ServerValidateClient (HTTPServerContext *server,
                                RTP_NET_ADDR *clientAddr)
{
int i, j, m, n, p;
unsigned char maskByte;
unsigned allowMatch = 0;
unsigned blockMatch = 0;


	for (i = 0; i < server->numAllowedClients; i++)
	{
		for (j=0; j < RTP_NET_IP_ALEN; j++)
		{
			allowMatch = 1;
			maskByte = server->allowedClientList[i].ipMask[j];
			if ((server->allowedClientList[i].ipAddr[j] & maskByte) !=
			    (clientAddr->ipAddr[j] & maskByte) )
			{
				allowMatch = 0;
				break;
			}
		}
		if (allowMatch)
		{
			break;
		}
	}

	for (m = 0; m < server->numBlockedClients; m++)
	{
		for (n=0; n < RTP_NET_IP_ALEN; n++)
		{
			blockMatch = 1;
			maskByte = server->blockedClientList[m].ipMask[n];
			if ((server->blockedClientList[m].ipAddr[n] & maskByte) !=
			    (clientAddr->ipAddr[n] & maskByte) )
			{
				blockMatch = 0;
				break;
			}
		}
		if (blockMatch)
		{
			break;
		}
	}

	if (allowMatch && blockMatch)
	{
		/*
		   if found in both the allowed and blocked list
		   we must check the level of accuracy in both
		   cases to perform the correct action

		   example:
		            want to permit all from 192.168.0.X
		            but dont want to permit 192.168.0.2
		 */
		for (p=0; p < RTP_NET_IP_ALEN; p++)
		{
			if (server->allowedClientList[i].ipAddr[p] < server->blockedClientList[m].ipAddr[p])
			{
				return (-1);
			}
		}
	}
	else if (blockMatch)
	{
		return (-1);
	}

	/*
		if allowMatch or no rule found
		for this peer - allow
	 */
	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_ServerSendError (HTTPServerRequestContext *ctx,
                          HTTPSession *session,
                          HTTP_INT16 httpStatus,
                          const HTTP_CHAR *httpMessage)
{
	return (_HTTP_ServerSendError(ctx, session, httpStatus, httpMessage));
}

/*---------------------------------------------------------------------------*/
int _HTTP_ServerSendError (HTTPServerRequestContext *ctx,
                           HTTPSession *session,
                           HTTP_INT16 httpStatus,
                           const HTTP_CHAR *httpMessage)
{
	HTTPResponse response;

	_HTTP_ServerInitResponse(ctx, session, &response, httpStatus, httpMessage);
	_HTTP_ServerSetDefaultHeaders(ctx, &response);

	HTTP_SetResponseHeaderStr(&response, "Pragma", "no-cache");
	HTTP_SetResponseHeaderStr(&response, "Cache-Control", "no-cache, must revalidate");
	HTTP_SetResponseHeaderInt(&response, "Content-Length", 0);

	HTTP_WriteResponse(session, &response);
	HTTP_WriteFlush(session);
	HTTP_FreeResponse(&response);

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ServerHandleRequest (HTTPServerRequestContext *ctx,
							   HTTPSession *session,
                               HTTPRequest *request,
                               RTP_NET_ADDR *clientAddr)
{
	HTTP_CHAR fsPath[HTTP_SERVER_PATH_LEN];
	HTTPServerContext *server = ctx->server;

	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			RTP_FILE fd;
			HTTP_INT32 bytesRead, bufferSize = 1024;
			HTTP_UINT8 buffer[1024];
			FileContentType contentType;
			unsigned chunked = 0;

			_HTTP_ServerReadHeaders(ctx, session, 0, 0, buffer, bufferSize);

			_HTTP_ConstructLocalPath(server, fsPath, (HTTP_CHAR *) request->target, HTTP_SERVER_PATH_LEN);

			if (rtp_file_open(&fd, fsPath, RTP_FILE_O_BINARY|RTP_FILE_O_RDONLY, RTP_FILE_S_IREAD) < 0)
			{
				unsigned found = 0;

				if ((server->defaultFile) && (server->defaultFile[0]) &&
				    ((fsPath[rtp_strlen(fsPath)-1] == '\\') || (fsPath[rtp_strlen(fsPath)-1] == '/')))
				{
					if ((HTTP_SERVER_PATH_LEN - rtp_strlen(server->defaultFile)) > rtp_strlen(server->defaultFile))
					{
						rtp_strcat(fsPath, server->defaultFile);
						if (rtp_file_open(&fd, fsPath, RTP_FILE_O_BINARY|RTP_FILE_O_RDONLY, RTP_FILE_S_IREAD) >= 0)
						{
							found = 1;
						}
					}
				}

				if (!found)
				{
					rtp_printf("404 error open failed on  == %s \n", fsPath);
					
					_HTTP_ServerSendError(ctx, session, 404, "Not Found");
					return (0);
				}
			}

			_HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
			_HTTP_ServerSetDefaultHeaders(ctx, &response);

			contentType = GetFileTypeByExtension(fsPath);
			if (contentType != FILE_TYPE_UNKNOWN)
			{
				HTTP_SetResponseHeaderStr(&response, "Content-Type", FileContentTypeToStr(contentType));
			}

			if (ctx->keepAlive)
			{
				if (request->httpMajorVersion >= 1 && request->httpMinorVersion >= 1)
				{
					HTTP_SetResponseHeaderStr(&response, "Transfer-Encoding", "chunked");
					chunked = 1;
				}
				else
				{
					void* dirobj;
					unsigned long fileSize;

					if (rtp_file_gfirst(&dirobj, fsPath) < 0)
					{
						_HTTP_ServerSendError(ctx, session, 500, "Internal Server Error");
						HTTP_FreeResponse(&response);
						rtp_file_close(fd);
						return (-1);
					}

					if (rtp_file_get_size(dirobj, &fileSize) < 0)
					{
						_HTTP_ServerSendError(ctx, session, 500, "Internal Server Error");
						HTTP_FreeResponse(&response);
						rtp_file_gdone(dirobj);
						rtp_file_close(fd);
						return (-1);
					}
					else
					{
						HTTP_SetResponseHeaderInt(&response, "Content-Length", fileSize);
						rtp_file_gdone(dirobj);
					}
				}
			}

			HTTP_WriteResponse(session, &response);
			HTTP_FreeResponse(&response);

			do
			{
				bytesRead = rtp_file_read(fd, buffer, bufferSize);

				if (bytesRead > 0)
				{
					if (chunked)
					{
						if (HTTP_WriteChunkStart(session, bytesRead) < 0)
						{
							break;
						}
					}

					if (HTTP_Write(session, buffer, bytesRead) < bytesRead)
					{
						break;
					}

					if (chunked)
					{
						if (HTTP_WriteChunkEnd(session) < 0)
						{
							break;
						}
					}
				}

			} while (bytesRead > 0);

			/* this tells the client the transfer is complete */
			if (chunked)
			{
				HTTP_WriteChunkStart(session, 0);
				HTTP_WriteChunkEnd(session);
			}

			HTTP_WriteFlush(session);

			rtp_file_close(fd);

			break;
		}

		default:
			break;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTPServerPath *_HTTP_ServerFindMatchingPath   (HTTPServerContext *server,
                                                const HTTP_CHAR *path)
{
HTTPServerPath *serverPath;

	/* first try to find an existing path */
	serverPath = (HTTPServerPath *) server->pathList.next;
	while (serverPath != (HTTPServerPath *) &server->pathList)
	{
		/* path name space is case-preserving, case-insensitive */
		if (_HTTP_ServerMatchPath(serverPath->path, path, serverPath->exactMatch))
		{
			return (serverPath);
		}
		serverPath = (HTTPServerPath *) serverPath->node.next;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_BOOL _HTTP_ServerMatchPath (const HTTP_CHAR *pattern,
                                 const HTTP_CHAR *path,
                                 HTTP_BOOL exactMatch)
{
	while (pattern[0])
	{
		if (pattern[0] == '/' || pattern[0] == '\\')
		{
			if (path[0] != '/' && path[0] != '\\')
			{
				return (0);
			}
		}
		else
		{
			if (UPPERCASE(path[0]) != UPPERCASE(pattern[0]))
			{
				return (0);
			}
		}
		path++;
		pattern++;
	}

	if (!exactMatch || !path[0] || path[0] == '?')
	{
		return (1);
	}

	return (0);
}


/*---------------------------------------------------------------------------*/
int HTTP_ServerReadHeaders (HTTPServerRequestContext *server,
                            HTTPSession *session,
                            HTTPServerHeaderCallback processHeader,
                            void *userData,
                            HTTP_UINT8 *buffer,
                            HTTP_INT32 size)
{
	return (_HTTP_ServerReadHeaders(server, session, processHeader, userData, buffer, size));
}


/*---------------------------------------------------------------------------*/
int _HTTP_ServerReadHeaders (HTTPServerRequestContext *ctx,
                             HTTPSession *session,
                             HTTPServerHeaderCallback processHeader,
                             void *userData,
                             HTTP_UINT8 *buffer,
                             HTTP_INT32 size)
{
	ctx->userCallback = processHeader;
	ctx->userData = userData;

	return (HTTP_ReadHeaders(session, _HTTP_ServerHeaderCallback, ctx, buffer, size));
}

/*---------------------------------------------------------------------------*/
int _HTTP_ServerHeaderCallback (void *userData,
                                HTTPSession *session,
                                HTTPHeaderType type,
                                const HTTP_CHAR *name,
                                const HTTP_CHAR *value)
{
HTTPServerRequestContext *ctx = (HTTPServerRequestContext *) userData;

	if (ctx->userCallback)
	{
		int result = ctx->userCallback(ctx->userData, ctx, session, type, name, value);
		switch (result)
		{
			case 0:
				break;

			default:
				return (result);
		}
	}

	switch (type)
	{
		case HTTP_HEADER_CONNECTION:
			if (rtp_stristr(value, "close"))
			{
				ctx->keepAlive = 0;
			}
			else if (rtp_stristr(value, "keep-alive"))
			{
				ctx->keepAlive = 1;
			}
			break;
	}

	return (0);
}


/*---------------------------------------------------------------------------*/
void _HTTP_ConstructLocalPath (HTTPServerContext *server,
                               HTTP_CHAR *fsPath,
                               HTTP_CHAR *targetURL,
                               HTTP_INT32 maxChars)
{
HTTP_INT32 len;
HTTP_CHAR altSep = ('\\' + '/') - rtp_file_get_path_seperator();

	/* just to be on the safe side... */
	maxChars--;

	rtp_strncpy(fsPath, server->rootDir, maxChars);
	len = rtp_strlen(fsPath);

	if (len > 0 && fsPath[len-1] != rtp_file_get_path_seperator())
	{
		fsPath[len++] = rtp_file_get_path_seperator();
		fsPath[len] = '\0';
	}

	if (targetURL[0] == '\\' || targetURL[0] == '/')
	{
		targetURL++;
	}

	_HTTP_ServerCollapsePath(targetURL);

	rtp_strncpy(fsPath + len, targetURL, maxChars - len);
}

/*---------------------------------------------------------------------------*/
void _HTTP_ServerCollapsePath (HTTP_CHAR *path)
{
	// compact ".." and "." directories in path
	if (path[0])
	{
		HTTP_CHAR * pos = path - 1;
		do
		{
			pos++;
			while ((pos[0] == '.') && ((pos[1] == '/') || (pos[1] == '\\')))
			{
				rtp_memmove((unsigned char *) pos,
				           (unsigned char *) (pos + 2),
				           (rtp_strlen(pos + 2) + 1) * sizeof(HTTP_CHAR));
			}
		} while ((pos = (char*)rtp_strpbrk(pos, "/\\")) != 0);

		pos = path - 1;
		do
		{
			pos++;
			while ((pos[0] == '.') && (pos[1] == '.') && ((pos[2] == '/') || (pos[2] == '\\')))
			{
				HTTP_CHAR * nextdir, * prevdir;

				if (pos == path)
				{
					break;
				}

				nextdir = pos + 2;
				prevdir = pos - 2;
				if (prevdir < path)
				{
					prevdir = path;
				}
				else
				{
					while ((prevdir > path) && (prevdir[0] != '/') && (prevdir[0] != '\\'))
					{
						prevdir--;
					}
				}

				rtp_memmove((unsigned char *) prevdir,
				           (unsigned char *) nextdir,
				           (rtp_strlen(nextdir) + 1) * sizeof(HTTP_CHAR));

				pos = prevdir;
			}
		} while ((pos = (char*)rtp_strpbrk(pos, "/\\")) != 0);
	}

}

/*---------------------------------------------------------------------------*/
HTTPServerAuthRealm *_HTTP_AllocServerAuthRealm (void)
{
	return ((HTTPServerAuthRealm *) HTTP_MALLOC(sizeof(HTTPServerAuthRealm)));
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeServerAuthRealm (HTTPServerAuthRealm *realm)
{
	HTTP_FREE(realm);
}

/*---------------------------------------------------------------------------*/
HTTPServerPath *_HTTP_AllocServerPath (void)
{
	return ((HTTPServerPath *) HTTP_MALLOC(sizeof(HTTPServerPath)));
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeServerPath (HTTPServerPath *path)
{
	HTTP_FREE(path);
}

/*---------------------------------------------------------------------------*/
HTTPVirtualFileInfo *_HTTP_AllocVirtualFileInfo (void)
{
	return ((HTTPVirtualFileInfo *) HTTP_MALLOC(sizeof(HTTPVirtualFileInfo)));
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeVirtualFileInfo (HTTPVirtualFileInfo *vfile)
{
	HTTP_FREE(vfile);
}

/*---------------------------------------------------------------------------*/
int  _HTTP_ServerInitRequestContext  (HTTPServerContext *server,
                                      HTTPServerRequestContext *ctx)
{
	rtp_memset(ctx, 0, sizeof(HTTPServerRequestContext));

	ctx->server = server;

	return (0);
}

#ifdef HTTP_SERVER_MULTITHREAD
/*---------------------------------------------------------------------------*/
void _HTTP_ServerExecuteJob (RTP_HELPER_THREAD_JOB* job)
{
	HTTPServerConnection* connection = (HTTPServerConnection*) job;

	HTTP_ServerProcessOneConnection (
			connection->server,
			connection->socket,
			&connection->clientAddr
		);

	/* free the job */
	HTTP_SERVER_ENTER_CRITICAL(connection->server);
	job->next = &connection->server->jobFreeList->job;
	connection->server->jobFreeList = connection;
	HTTP_SERVER_EXIT_CRITICAL(connection->server);
}
#endif //#ifdef HTTP_SERVER_MULTITHREAD

/*---------------------------------------------------------------------------*/
int  HTTP_ServerConnectSetKeepAlive (HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTP_BOOL doKeepAlive)
{
#ifdef HTTP_SERVER_KEEP_ALIVE
	ctx->keepAlive = doKeepAlive;
#else
	ctx->keepAlive = 0;
#endif

	return (0);
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerConnectionClose     (HTTPServerRequestContext *ctx,
                                     HTTPSession *session)
{
	if (!ctx->connectionClosed)
	{
		ctx->connectionClosed = 1;
		rtp_net_closesocket(ctx->socketHandle);
	}
	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_ServerProcessHeaderStub(
		void *userData,
		HTTPServerRequestContext *server,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value)
{
	return (0);
}

/*---------------------------------------------------------------------------*/
int  HTTP_ServerRequestGetLocalAddr (
		HTTPServerRequestContext *ctx,
		RTP_NET_ADDR *local)
{
	return (rtp_net_getsockname(ctx->socketHandle, local->ipAddr, &local->port, &local->type));
}

/*---------------------------------------------------------------------------*/
HTTP_UINT16 HTTP_ServerGetPort      (HTTPServerContext *server)
{
RTP_NET_ADDR serverAddr;

	if (rtp_net_getsockname(server->serverSocket,
	                        serverAddr.ipAddr,
                            &serverAddr.port,
                            &serverAddr.type) < 0)
	{
		return (0);
	}

	return (serverAddr.port);
}
