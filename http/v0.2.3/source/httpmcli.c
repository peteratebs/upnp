//
// HTTPMCLI.C -
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

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "httpmcli.h"
#include "filetype.h"
#include "rtptime.h"
#include "rtpstr.h"
#include "rtpstdup.h"
#include "rtpscnv.h"
#include "rtpdutil.h"
#include "rtplog.h"

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_SSL
#include "httpscli.h"
#endif

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define IS_WHITESPACE(C)                       ((C)==' ' ||  (C)=='\r' || \
                                                (C)=='\n' || (C)=='\t' || \
                                                (C)=='\f' || (C)=='\v')

#define HTTP_SESSION_SET_SIZE 32
#define HTTP_CFG_MAX_SESSION_IDLE_MSECS        15000

#ifdef HTTP_MANAGED_CLIENT_THREADSAFE

#define HTTP_MANAGED_CLIENT_API_ENTER(X)       rtp_sig_mutex_claim((X)->lock)
#define HTTP_MANAGED_CLIENT_API_EXIT(X)        rtp_sig_mutex_release((X)->lock)

#else

#define HTTP_MANAGED_CLIENT_API_ENTER(X)
#define HTTP_MANAGED_CLIENT_API_EXIT(X)

#endif /* HTTP_MANAGED_CLIENT_THREADSAFE */

/* for possible future use */
#define HTTP_MANAGED_SESSION_API_ENTER(X)
#define HTTP_MANAGED_SESSION_API_EXIT(X)

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_HTTPManagedClientSessionInfo   HTTPManagedClientSessionInfo;
typedef struct s_HTTPManagedClientSecureSession HTTPManagedClientSecureSession;
typedef struct s_HTTPManagedClientHost          HTTPManagedClientHost;
typedef struct s_HTTPManagedClientRequestInfo   HTTPManagedClientRequestInfo;
typedef struct s_HTTPClientProcessHeaderContext HTTPClientProcessHeaderContext;

struct s_HTTPManagedClientRequestInfo
{
	HTTPManagedClient*           client;
	HTTPManagedClientSession*    session;
	HTTPRequest*                 request;
};

struct s_HTTPManagedClientSessionInfo
{
	DLListNode                   node;
	HTTPManagedClient*           client;
	HTTPManagedSessionType       type;
	HTTPManagedSessionState      state;
	HTTP_UINT32                  idleSinceMsec;
	HTTP_INT32                   idleTimeoutMsec;
	unsigned                     keepAlive;
	const HTTP_CHAR*             host;
	HTTP_UINT16                  port;
	const HTTP_CHAR*             target;
	HTTP_UINT32                  authTimestamp;
	HTTPResponseInfo             responseInfo;
};

struct s_HTTPManagedClientSession
{
	HTTPManagedClientSessionInfo info;
	HTTPClientSession            session;
};

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP

struct s_HTTPUDPClientSession
{
	HTTPClientSession            session;
	HTTP_UINT8*                  buffer;
	HTTP_INT32                   readPos;
	HTTP_INT32                   bufferUsed;
};

typedef struct s_HTTPUDPClientSession HTTPUDPClientSession;

struct s_HTTPManagedClientUDPSession
{
	HTTPManagedClientSessionInfo info;
	HTTPUDPClientSession         session;
};

typedef struct s_HTTPManagedClientUDPSession HTTPManagedClientUDPSession;

#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_SSL
struct s_HTTPManagedClientSecureSession
{
	HTTPManagedClientSessionInfo info;
	HTTPSClientSession           session;
};
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_SSL */

struct s_HTTPManagedClientHost
{
	const HTTP_CHAR*             hostName;
	HTTP_UINT16                  port;
};

struct s_HTTPClientProcessHeaderContext
{
	HTTPManagedClientSession*    session;
	HTTPHeaderCallback           processHeaderFn;
	void*                        processHeaderData;

};

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

int _HTTP_ManagedClientInit (
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

int _HTTP_ManagedClientSetDefaultHeaders (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		HTTPRequest* request
	);

void HTTP_ManagedClientCloseCond (
		HTTPManagedClient* client,
		unsigned testFn (
				HTTPManagedClient* client,
				HTTPManagedClientSession* session,
				void* data
			),
		void* data
	);

unsigned _HTTP_ManagedClientMatchAll (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	);

unsigned _HTTP_ManagedClientMatchHost (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	);

unsigned _HTTP_ManagedClientMatchStale (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	);

int _HTTP_ManagedClientCacheSession (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session
	);

void _HTTP_ManagedClientCloseSession (
		HTTPManagedClientSession* session /** the session to close */
	);

HTTPManagedClientSession* _HTTP_AllocManagedSession (
		HTTPManagedClient* client
	);

void _HTTP_FreeManagedSession (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session
	);

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
HTTPManagedClientUDPSession* _HTTP_AllocManagedUDPSession (
		HTTPManagedClient* client
	);

void _HTTP_FreeManagedUDPSession (
		HTTPManagedClient* client,
		HTTPManagedClientUDPSession* session
	);
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */

HTTPManagedClientSecureSession* _HTTP_AllocManagedSecureSession (
		HTTPManagedClient* client
	);

void _HTTP_FreeManagedSecureSession (
		HTTPManagedClient* client,
		HTTPManagedClientSecureSession* session
	);

HTTPManagedClientSession* HTTP_ManagedClientFindOpenSession (
		HTTPManagedClient* client,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTPManagedSessionType type
	);

HTTPManagedClientSession* _HTTP_ManagedClientCreateSession (
		HTTPManagedClient* client,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTPManagedSessionType type,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	);

int _HTTP_ManagedClientReadResponse (
		HTTPManagedClientSession* session,
		HTTP_UINT8* buffer,
		HTTP_INT32 size,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData
	);

int _HTTP_ManagedClientProcessHeader (
		HTTPClientProcessHeaderContext* ctx,
		HTTPSession* session,
		HTTPHeaderType type,
		const HTTP_CHAR* name,
		const HTTP_CHAR* value
	);

int _HTTP_ManagedToClientSessionList (
		HTTPClientSession** sList,
		HTTP_INT16* sNum,
		HTTPManagedClientSession** mList,
		HTTP_INT16 mNum
	);

int _HTTP_ClientToManagedSessionList (
		HTTPManagedClientSession** mList,
		HTTP_INT16* mNum,
		HTTPClientSession** sList,
		HTTP_INT16 sNum
	);

unsigned _HTTP_ClientSessionListIsSet (
		HTTPClientSession** sList,
		HTTP_INT16 sNum,
		HTTPClientSession* session
	);

HTTP_INT32 _HTTP_ManagedClientAuthWrite (
		HTTPManagedClientRequestInfo* requestInfo,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	);

HTTP_INT32 _HTTP_ManagedClientCookieWrite (
		HTTPManagedClientRequestInfo* requestInfo,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	);

int _HTTP_AddPostHeaders (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		HTTPRequest* request,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength
	);

void _HTTP_ManagedClientFreeResponseInfo (
		HTTPResponseInfo* responseInfo
	);

int _HTTP_ManagedClientRequestEx (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength,
		HTTPRequest* request
	);

int _HTTP_ManagedClientRequestHeaderDone (
		HTTPManagedClientSession* session,
		HTTPRequest* request
	);

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
HTTP_INT32 _HTTP_ReadUDP (
		void* streamPtr,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	);

HTTP_INT32 _HTTP_WriteUDP (
		void* streamPtr,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	);
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */

/*****************************************************************************/
// Data
/*****************************************************************************/

const HTTP_CHAR HTTP_CLIENT_KEEP_ALIVE_STR[] = {
		'k','e','e','p','-','a','l','i','v','e',0
	};

const HTTP_CHAR HTTP_CLIENT_CLOSE_STR[] = {
		'c','l','o','s','e',0
	};

const HTTP_CHAR HTTP_CLIENT_CHUNKED_STR[] = {
		'c','h','u','n','k','e','d',0
	};

const httpDefaultPort[HTTP_NUM_SESSION_TYPES] = {
		80,
		443,
		1900
	};

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------
  HTTP_ManagedClientInit
  ---------------------------------------------------------------------------*/
/** @memo Initialize an HTTP Managed Client instance.

    @doc  This function must be called before any other managed client calls
          to create a context in which the client will operate.  Multiple
          independent client contexts may be created from within the same
          application.

    @see  HTTP_ManagedClientDestroy

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientInit (
		HTTPManagedClient* client,        /** pointer to uninitialized HTTPManagedClient structure */
		const HTTP_CHAR* userAgent,       /** string to use to identify client to remote hosts */
		const HTTP_CHAR* acceptTypes,     /** string to send to server to indicate what data types we accept */
		unsigned useKeepAlive,            /** boolean: if non-zero, try to keep connections open to speed things up */
		HTTPCookieContext* cookieContext, /** optional: initialized instance of HTTPCookieContext for cookie support */
		HTTPAuthContext* authContext,     /** optional: initialized instance of HTTPAuthContext for authentication */
		RTP_HANDLE sslContext,            /** optional: initialized RTSSL context if secure HTTP is to be utilitized */
		unsigned sslEnabled,              /** boolean: if sslContext is valid this MUST be non-zero; otherwise 0 */
		HTTP_INT32 writeBufferSize,       /** number of bytes to buffer when sending */
		HTTP_INT32 maxTotalConnections,   /** max total connections to open simultaneously */
		HTTP_INT32 maxHostConnections     /** max connections allowed to any one remote host */
	)
{
#ifdef HTTP_MANAGED_CLIENT_THREADSAFE
	return (-1);
#else
	return (_HTTP_ManagedClientInit (
			client,
			userAgent,
			acceptTypes,
			useKeepAlive,
			cookieContext,
			authContext,
			sslContext,
			sslEnabled,
			writeBufferSize,
			maxTotalConnections,
			maxHostConnections
		));
#endif
}

#ifdef HTTP_MANAGED_CLIENT_THREADSAFE
/*---------------------------------------------------------------------------
  HTTP_ManagedClientInitMT
  ---------------------------------------------------------------------------*/
/** @memo Initialize a thread-safe HTTP Managed Client instance.

    @doc  This function must be called before any other managed client calls
          to create a context in which the client will operate.  Multiple
          independent client contexts may be created from within the same
          application.  The HTTP_MANAGED_CLIENT_THREADSAFE symbol must be defined
          at compile time (enabling thread-safe mode) to use this function.
          If threadsafe mode is enabled,
          then this function must be used in place of \Ref{HTTP_ManagedClientInit}.

          The mutex passed into this function must not be used for any other
          purpose while this managed client instance is in operation.  When the
          managed client is destroyed, the application must also dispose of the
          mutex in an appropriate manner.

    @see  HTTP_ManagedClientDestroy

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientInitMT (
		HTTPManagedClient* client,        /** pointer to uninitialized HTTPManagedClient structure */
		const HTTP_CHAR* userAgent,       /** string to use to identify client to remote hosts */
		const HTTP_CHAR* acceptTypes,     /** string to send to server to indicate what data types we accept */
		unsigned useKeepAlive,            /** boolean: if non-zero, try to keep connections open to speed things up */
		HTTPCookieContext* cookieContext, /** optional: initialized instance of HTTPCookieContext for cookie support */
		HTTPAuthContext* authContext,     /** optional: initialized instance of HTTPAuthContext for authentication */
		RTP_HANDLE sslContext,            /** optional: initialized RTSSL context if secure HTTP is to be utilitized */
		unsigned sslEnabled,              /** boolean: if sslContext is valid this MUST be non-zero; otherwise 0 */
		HTTP_INT32 writeBufferSize,       /** number of bytes to buffer when sending */
		HTTP_INT32 maxTotalConnections,   /** max total connections to open simultaneously */
		HTTP_INT32 maxHostConnections,    /** max connections allowed to any one remote host */
		RTP_MUTEX lock                    /** a mutex to use for thread synchronization */
	)
{
	if (_HTTP_ManagedClientInit (
			client,
			userAgent,
			acceptTypes,
			useKeepAlive,
			cookieContext,
			authContext,
			sslContext,
			sslEnabled,
			writeBufferSize,
			maxTotalConnections,
			maxHostConnections
		) >= 0)
	{
		client->lock = lock;
		return (0);
	}

	return (-1);
}
#endif

/*---------------------------------------------------------------------------
  HTTP_ManagedClientDestroy
  ---------------------------------------------------------------------------*/
/** @memo Destroy an HTTP Managed Client instance.

    @doc  This function must be called once all managed client operations have
          been completed to free any resources being used by the managed client.

    @return nothing
 */
void HTTP_ManagedClientDestroy (
		HTTPManagedClient* client /** the client instance to destroy */
	)
{
	HTTP_ManagedClientCloseAll(client);
	HTTP_ClientDestroy(&client->clientContext);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientCloseAll
  ---------------------------------------------------------------------------*/
/** @memo Close any open connections

    @doc  Closes any connections that were left open because of the keepAlive
          option.  The application can be assured that all connections (from
          this managed client, anyhow) are closed when this function returns.

    @return nothing
 */
void HTTP_ManagedClientCloseAll (
		HTTPManagedClient* client /** the client whose connections to close */
	)
{
	HTTP_ManagedClientCloseCond (
			client,
			_HTTP_ManagedClientMatchAll,
			0
		);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientCloseHost
  ---------------------------------------------------------------------------*/
/** @memo Close any open connections to a particular host

    @doc  Closes any connections to specific host that were left open because
          of the keepAlive option.

    @return nothing
 */
void HTTP_ManagedClientCloseHost (
		HTTPManagedClient* client, /** the managed client instance */
		const HTTP_CHAR* hostName, /** the host name to close */
		HTTP_UINT16 port           /** the port number of open connections or 0 for all */
	)
{
	HTTPManagedClientHost hostInfo;

	hostInfo.hostName = hostName;
	hostInfo.port = port;

	HTTP_ManagedClientCloseCond (
			client,
			_HTTP_ManagedClientMatchHost,
			&hostInfo
		);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientCloseStale
  ---------------------------------------------------------------------------*/
/** @memo Close any open connections that have been reset or have timed out.

    @doc  Calling this function periodically will assure that the managed
          client in quest is a good citizen in terms of not holding onto
          sockets (connected or otherwise) for too long.

    @return nothing
 */
void HTTP_ManagedClientCloseStale (
		HTTPManagedClient* client /** the managed client instance */
	)
{
	HTTP_INT32 currentTimeMsec = rtp_get_system_msec();

	HTTP_ManagedClientCloseCond (
			client,
			_HTTP_ManagedClientMatchStale,
			&currentTimeMsec
		);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientStartTransaction
  ---------------------------------------------------------------------------*/
/** @memo Initiate a transaction with an HTTP server.

    @doc  Every HTTP operation provided by the managed client can be seen as
          a single transaction, or request/response pair.  Each transaction
          takes place within the context of an HTTPManagedClientSession.
          An HTTPManagedClientSession corresponds to a single connection to
          a remote server.  The managed client is designed to handle the
          specifics of establishing and closing connections automatically;
          thus, while calling \Ref{HTTP_ManagedClientStartTransaction} may
          cause a new connection to be established, it may just as easily
          find an existing open connection and pass it back through the session
          pointer (see "session" argument below).

          This function, therefore, should not be viewed as explicitly opening
          a connection, but rather as setting up the managed client to send an
          HTTP request to a given remote host.

          The session returned by this function is only good for a single
          transaction.  Once the transaction is complete (or has been aborted),
          \Ref{HTTP_ManagedClientFinishTransaction} or \Ref{HTTP_ManagedClientCloseSession}
          (to physically close the connection) must be called.

          If the blocking option is set, this function may block until a
          connection is established with the remote host.  If it is not,
          this function will return immediately; but in this case, the
          application should use \Ref{HTTP_ManagedClientSelect} to determine
          when a request can be sent.

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientStartTransaction (
		HTTPManagedClient* client,         /** a managed client instance */
		const HTTP_CHAR* host,             /** the host name of the server */
		HTTP_UINT16 port,                  /** the port on the remote host, or 0 for the default */
        HTTP_INT16 ipType,                 /** Type of address IPv4 or IPv6 */
		HTTPManagedSessionType type,       /** the type of connection (TCP, secure TCP, UDP mulitcast, etc.) */
		unsigned blocking,                 /** boolean: use blocking I/O or non-blocking */
		HTTPManagedClientSession** session /** pointer to HTTPManagedClientSession pointer to return the session */
	)
{
	int result = -1;

	if (session && type >= 0 && type < HTTP_NUM_SESSION_TYPES)
	{
		*session = HTTP_ManagedClientFindOpenSession (client, host, port, type);

		if (*session)
		{
			HTTP_ClientSessionSetBlocking(&(*session)->session, blocking);
			result = 0;
		}
		else
		{
			HTTP_MANAGED_CLIENT_API_ENTER(client);

			/* set to the default port for this type of session if
			   none specified */
			if (port == 0)
			{
				port = httpDefaultPort[type];
			}

			*session = _HTTP_ManagedClientCreateSession (
					client,
					host,
					port,
					type,
					blocking,
					RTP_TIMEOUT_INFINITE
				);

			if (*session)
			{
				(*session)->info.state = HTTP_SESSION_STATE_OPEN;
				(*session)->info.idleTimeoutMsec = client->maxSessionIdleMsecs;
				(*session)->info.keepAlive = client->useKeepAlive;
				(*session)->info.host = host;
				(*session)->info.port = port;

				result = 0;
			}

			HTTP_MANAGED_CLIENT_API_EXIT(client);
		}
	}

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientGet
  ---------------------------------------------------------------------------*/
/** @memo Send a GET request

    @doc  Downloads a file using the HTTP GET method, with optional
          if-modified-since date.

    @precondition a session pointer must have been returned by
          \Ref{HTTP_ManagedClientStartTransaction} for each time this function is
          called

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientGet (
		HTTPManagedClientSession* session, /** the session over which to perform the GET */
		const HTTP_CHAR* path,             /** the file to get on the server */
		RTP_TIMESTAMP* ifModifiedSince     /** instruct the server to send the file only
		                                       if it has been modified after this date */
	)
{
	int result = -1;
	HTTPRequest request;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	if (_HTTP_ManagedClientRequestEx (
			session,
			HTTP_MethodTypeToString(HTTP_METHOD_GET),
			path,
			ifModifiedSince,
			0, /* contentType */
			HTTP_CONTENT_LENGTH_NONE,
			&request
		) >= 0)
	{
		if (_HTTP_ManagedClientRequestHeaderDone(session, &request) >= 0)
		{
			if (HTTP_WriteFlush(
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session)
				) >= 0)
			{
				result = 0;
			}
		}
	}

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientPut
  ---------------------------------------------------------------------------*/
/** @memo Send a PUT request

    @doc  Uploads a file using the HTTP PUT method.  The application should
          always specify the content MIME type.  The contentLength is optional,
          however.  If the contentLength is not specified, then the data is
          sent in chunked transfer-encoding (each call to \Ref{HTTP_ManagedClientWrite}
          will generate a single chunk).  In either case, \Ref{HTTP_ManagedClientWriteDone}
          must be called once all data has been written.

    @precondition a session pointer must have been returned by
          \Ref{HTTP_ManagedClientStartTransaction} for each time this function is
          called

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientPut (
		HTTPManagedClientSession* session, /** a managed client instance */
		const HTTP_CHAR* path,             /** the server-side file name */
		HTTP_CHAR* contentType,            /** the mime type of the data ("text/xml", for example) */
		HTTP_INT32 contentLength          /** optional: pointer to 32-bit integer indicating the
		                                       size of the file being sent; NULL if not specified */
	)
{
	int result = -1;
	HTTPRequest request;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	if (_HTTP_ManagedClientRequestEx (
			session,
			HTTP_MethodTypeToString(HTTP_METHOD_PUT),
			path,
			0, /* ifModifiedSince */
			contentType,
			contentLength,
			&request
		) >= 0)
	{
		if (_HTTP_ManagedClientRequestHeaderDone(session, &request) >= 0)
		{
			result = 0;
		}
	}

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientPost
  ---------------------------------------------------------------------------*/
/** @memo Send a POST request

    @doc  Uploads a file and requests response data using the HTTP POST method.
          The application should always specify the content MIME type.  The
          contentLength is optional, however.  If the contentLength is not
          specified, then the data is sent in chunked transfer-encoding (each
          call to \Ref{HTTP_ManagedClientWrite} will generate a single chunk).
          In either case, \Ref{HTTP_ManagedClientWriteDone} must be called once
          all data has been written.

    @precondition a session pointer must have been returned by
          \Ref{HTTP_ManagedClientStartTransaction} for each time this function is
          called

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientPost (
		HTTPManagedClientSession* session, /** a managed client instance */
		const HTTP_CHAR* path,             /** the server-side file name */
		HTTP_CHAR* contentType,            /** the mime type of the data ("application/x-www-form-urlencoded", for example) */
		HTTP_INT32 contentLength          /** optional: pointer to 32-bit integer indicating the
		                                       size of the file being sent; NULL if not specified */
	)
{
	int result = -1;
	HTTPRequest request;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	if (_HTTP_ManagedClientRequestEx (
			session,
			HTTP_MethodTypeToString(HTTP_METHOD_POST),
			path,
			0, /* ifModifiedSince */
			contentType,
			contentLength,
			&request
		) >= 0)
	{
		if (_HTTP_ManagedClientRequestHeaderDone(session, &request) >= 0)
		{
			result = 0;
		}
	}

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientRequestEx
  ---------------------------------------------------------------------------*/
/** @memo Send a request

    @doc

    @precondition a session pointer must have been returned by
          \Ref{HTTP_ManagedClientStartTransaction} for each time this function is
          called

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientRequestEx (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength,
		HTTPRequest* request
	)
{
	int result = -1;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	result = _HTTP_ManagedClientRequestEx (
			session,
			method,
			path,
			ifModifiedSince,
			contentType,
			contentLength,
			request
		);

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientRequestHeaderDone
  ---------------------------------------------------------------------------*/
/** @memo

    @doc

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientRequestHeaderDone (
		HTTPManagedClientSession* session,
		HTTPRequest* request
	)
{
	int result = -1;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	result = _HTTP_ManagedClientRequestHeaderDone (session, request);

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientReadResponseInfo
  ---------------------------------------------------------------------------*/
/** @memo Get information about the server's response.

    @doc  This function populates the structure pointed to by responseInfo
          with various information about the server response to a client-initiated
          request, such as the mime type of the data being received, the status
          code (indicating success, file moved, authentication required, file
          not modified, etc.).

    @precondition A request must have been sent over the given session using
          \Ref{HTTP_ManagedClientGet}, \Ref{HTTP_ManagedClientPut},
          or \Ref{HTTP_ManagedClientPost}

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientReadResponseInfo (
		HTTPManagedClientSession* session, /** the session for which to get info */
		HTTPResponseInfo* responseInfo     /** uninitialized HTTPResponseInfo structure to hold info */
	)
{
	int result = -1;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	switch (session->info.state)
	{
		case HTTP_SESSION_STATE_REQUEST_SENT:
		{
			HTTP_INT32 bufferSize = 512;
			HTTP_UINT8 buffer[512];

			if (_HTTP_ManagedClientReadResponse (session, buffer, bufferSize, 0, 0) < 0)
			{
				break;
			}
		}

		case HTTP_SESSION_STATE_RESPONSE_READ:
			rtp_memcpy(responseInfo, &session->info.responseInfo, sizeof(HTTPResponseInfo));
			result = 0;
			break;
	}

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientReadResponseInfo
  ---------------------------------------------------------------------------*/
/** @memo Get information about the server's response.

    @doc  This function populates the structure pointed to by responseInfo
          with various information about the server response to a client-initiated
          request, such as the mime type of the data being received, the status
          code (indicating success, file moved, authentication required, file
          not modified, etc.).

    @precondition A request must have been sent over the given session using
          \Ref{HTTP_ManagedClientGet}, \Ref{HTTP_ManagedClientPut},
          or \Ref{HTTP_ManagedClientPost}

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientReadResponseInfoEx (
		HTTPManagedClientSession* session,
		HTTPResponseInfo* responseInfo,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData
	)
{
	int result = -1;

	HTTP_MANAGED_SESSION_API_ENTER(session);

	switch (session->info.state)
	{
		case HTTP_SESSION_STATE_REQUEST_SENT:
		{
			HTTP_INT32 bufferSize = 512;
			HTTP_UINT8 buffer[512];

			if (_HTTP_ManagedClientReadResponse (
					session,
					buffer,
					bufferSize,
					processHeaderFn,
					processHeaderData
				) < 0)
			{
				break;
			}
		}

		case HTTP_SESSION_STATE_RESPONSE_READ:
			rtp_memcpy(responseInfo, &session->info.responseInfo, sizeof(HTTPResponseInfo));
			result = 0;
			break;
	}

	HTTP_MANAGED_SESSION_API_EXIT(session);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientReadSelect
  ---------------------------------------------------------------------------*/
/** @memo Wait for input on a managed client session

    @doc  This function blocks for at most timeoutMsec milliseconds waiting
          for data to arrive over a managed client session.

    @see HTTP_ManagedClientSelect, HTTP_ManagedClientRead

    @return non-negative on success, negative on failure
 */
int HTTP_ManagedClientReadSelect (
		HTTPManagedClientSession* session,
		HTTP_INT32 timeoutMsec
	)
{
	HTTPManagedClientSession* list[1];
	HTTPManagedClientSession* listErr[1];
	HTTP_INT16 num = 1;
	HTTP_INT16 dummy = 0;
	HTTP_INT16 numErr = 1;

	list[0] = session;
	listErr[0] = session;

	HTTP_ManagedClientSelect(0, &dummy, list, &num, listErr, &numErr, timeoutMsec);

	if (num == 1)
	{
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientWriteSelect
  ---------------------------------------------------------------------------*/
/** @memo Wait for a managed client session to be ready to send request/data

    @doc  This function blocks for at most timeoutMsec milliseconds waiting
          for a managed client session to enter a state were it is able to
          send a new request or data.

    @see HTTP_ManagedClientSelect, HTTP_ManagedClientWrite

    @return non-negative on success, negative on failure
 */
int HTTP_ManagedClientWriteSelect (
		HTTPManagedClientSession* session,
		HTTP_INT32 timeoutMsec
	)
{
	HTTPManagedClientSession* list[1];
	HTTPManagedClientSession* listErr[1];
	HTTP_INT16 num = 1;
	HTTP_INT16 dummy = 0;
	HTTP_INT16 numErr = 1;

	list[0] = session;
	listErr[0] = session;

	HTTP_ManagedClientSelect(list, &num, 0, &dummy, listErr, &numErr, timeoutMsec);

	if (num == 1)
	{
		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientSelect
  ---------------------------------------------------------------------------*/
/** @memo Select sessions for read-ready, write-ready or error condition

    @doc  This function serves a purpose very similar to the sockets API call
          "select".  It blocks for at most timeoutMsec milliseconds (possibly
          less, if one of the sessions is in the specified condition).  When
          it returns, all sessions NOT in the condition associated with each
          of the three arrays will have been removed from those respective arrays
          (and the associated sizes of the arrays modified accordingly).

          If a session is in non-blocking mode, then selecting for write
          using this function will indicate the connection is in a state to
          send a request using \Ref{HTTP_ManagedClientGet},
          \Ref{HTTP_ManagedClientPut}, or \Ref{HTTP_ManagedClientPost}

    @return non-negative on success, negative on failure
 */
int HTTP_ManagedClientSelect (
		HTTPManagedClientSession** writeList, /** pointer to an array of HTTPManagedClientSession pointers to be selected for writing */
		HTTP_INT16* writeNum,                 /** pointer to an integer storing the number of elements in writeList */
		HTTPManagedClientSession** readList,  /** pointer to an array of HTTPManagedClientSession pointers to be selected for reading */
		HTTP_INT16* readNum,                  /** pointer to an integer storing the number of elements in readList */
		HTTPManagedClientSession** errList,   /** pointer to an array of HTTPManagedClientSession pointers to be selected for errors */
		HTTP_INT16* errNum,                   /** pointer to an integer storing the number of elements in errList */
		HTTP_INT32 timeoutMsec                /** maximum time to block for (milliseconds) */
	)
{
	int result;

	HTTPClientSession* writeSessionList[HTTP_SESSION_SET_SIZE];
	HTTPClientSession* readSessionList[HTTP_SESSION_SET_SIZE];
	HTTPClientSession* errSessionList[HTTP_SESSION_SET_SIZE];

	HTTP_INT16 writeSessionNum = HTTP_SESSION_SET_SIZE;
	HTTP_INT16 readSessionNum  = HTTP_SESSION_SET_SIZE;
	HTTP_INT16 errSessionNum   = HTTP_SESSION_SET_SIZE;

	_HTTP_ManagedToClientSessionList (
			writeSessionList,
			&writeSessionNum,
			writeList,
			*writeNum
		);

	_HTTP_ManagedToClientSessionList (
			readSessionList,
			&readSessionNum,
			readList,
			*readNum
		);

	_HTTP_ManagedToClientSessionList (
			errSessionList,
			&errSessionNum,
			errList,
			*errNum
		);

	result = HTTP_ClientSessionSelect (
			writeSessionList,
			&writeSessionNum,
			readSessionList,
			&readSessionNum,
			errSessionList,
			&errSessionNum,
			timeoutMsec
		);

	_HTTP_ClientToManagedSessionList (
			writeList,
			writeNum,
			writeSessionList,
			writeSessionNum
		);

	_HTTP_ClientToManagedSessionList (
			readList,
			readNum,
			readSessionList,
			readSessionNum
		);

	_HTTP_ClientToManagedSessionList (
			errList,
			errNum,
			errSessionList,
			errSessionNum
		);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientWrite
  ---------------------------------------------------------------------------*/
/** @memo Write data to a remote server.

    @doc  This function is called after \Ref{HTTP_ManagedClientPut}, or
          \Ref{HTTP_ManagedClientPost} to upload data to the server.  When
          all data for the given request has been sent, \Ref{HTTP_ManagedClientWriteDone}
          must be called to signal that all data has been written.

    @return number of bytes written on success, negative on failure
 */
HTTP_INT32 HTTP_ManagedClientWrite (
		HTTPManagedClientSession* session, /** session over which to send data */
		HTTP_UINT8* buffer,                /** pointer to the data to send */
		HTTP_INT32 size                    /** number of bytes out of buffer to send */
	)
{
	switch (session->info.state)
	{
		case HTTP_SESSION_STATE_WRITING_DATA:
			return (HTTP_Write (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
					buffer,
					size
				));

		case HTTP_SESSION_STATE_WRITING_CHUNKS:
		{
			HTTP_INT32 result;

			HTTP_WriteChunkStart (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
					size
				);

			result = HTTP_Write (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
					buffer,
					size
				);

			HTTP_WriteChunkEnd (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session)
				);

			return (result);
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientWriteDone
  ---------------------------------------------------------------------------*/
/** @memo Indicate that all data for a given request has been sent.

    @doc  This function must be called after the request is sent once all
          associated data has been written to the session.

    @return 0 on success, negative on failure
 */
int HTTP_ManagedClientWriteDone (
		HTTPManagedClientSession* session /** session for which to signal done writing */
	)
{
	switch (session->info.state)
	{
		case HTTP_SESSION_STATE_WRITING_CHUNKS:

			/* a chunk of size 0 indicates we are done sending data */
			HTTP_WriteChunkStart (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
					0
				);

			HTTP_WriteChunkEnd (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session)
				);

			/* intentional fall-through */

		case HTTP_SESSION_STATE_WRITING_DATA:

			HTTP_WriteFlush (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session)
				);

			session->info.state = HTTP_SESSION_STATE_REQUEST_SENT;

			return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientRead
  ---------------------------------------------------------------------------*/
/** @memo Read data from a session

    @doc  This function must be called after the request and all associated
          data has been sent.  If the session is in non-blocking mode, this
          function may return a HTTP_EWOULDBLOCK value to indicate that no
          data is yet available.  A return value of 0 indicates that all
          data has been read from this session and the session should be
          closed using \Ref{HTTP_ManagedClientCloseSession} or
          \Ref{HTTP_ManagedClientFinishTransaction}.

    @return bytes read or error code
 */
HTTP_INT32 HTTP_ManagedClientRead (
		HTTPManagedClientSession* session, /** session from which to read data */
		HTTP_UINT8* buffer,                /** buffer into which to read data */
		HTTP_INT32 size                    /** max number of bytes to read */
	)
{
	switch (session->info.state)
	{
		case HTTP_SESSION_STATE_REQUEST_SENT:
		{
			HTTP_INT32 bufferSize = 512;
			HTTP_UINT8 buffer[512];

			_HTTP_ManagedClientReadResponse (session, buffer, bufferSize, 0, 0);
		}
		/* intentional fall-through */
		case HTTP_SESSION_STATE_RESPONSE_READ:
		{
			HTTP_INT32 bytesRead;

			bytesRead = HTTP_Read (
					HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
					buffer,
					size
				);

			if (bytesRead == 0)
			{
				session->info.state = HTTP_SESSION_STATE_DONE;
			}
			else
			{
				if (bytesRead == -1)
				{
					session->info.state = HTTP_SESSION_STATE_ERROR;
				}
			}

			return (bytesRead);
		}

		case HTTP_SESSION_STATE_DONE:
			return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientReadFrom
  ---------------------------------------------------------------------------*/
/** @memo Read a datagram-type HTTP response

    @doc  This function is only relevant to HTTP-over-UDP sessions.

    @see HTTP_ManagedClientRead, HTTP_ManagedClientStartTransaction

    @return nothing
 */
HTTP_INT32 HTTP_ManagedClientReadFrom (
		HTTPManagedClientSession* session,
		HTTPResponseInfo* responseInfo,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData,
		HTTP_UINT8* buffer,
		HTTP_INT32 size,
		RTP_NET_ADDR* fromAddr
	)
{
#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
	/* can only be called on UDP sessions */
	if (session->info.type == HTTP_SESSION_TYPE_UDP)
	{
		HTTPManagedClientUDPSession* udpSession = (HTTPManagedClientUDPSession*) session;
		HTTP_INT32 bytesRead;

		bytesRead = rtp_net_recvfrom (
				udpSession->session.session.netSock,
				buffer,
				size,
				fromAddr->ipAddr,
				&fromAddr->port,
				&fromAddr->type
			);

		if (bytesRead > 0)
		{
			HTTP_UINT8 headerBuffer[512];
			HTTP_INT32 headerBufferSize = 512;

			udpSession->session.buffer = buffer;
			udpSession->session.readPos = 0;
			udpSession->session.bufferUsed = bytesRead;

			RTP_LOG_WRITE("HTTP_ManagedClientReadFrom READ", buffer, bytesRead)

			if (_HTTP_ManagedClientReadResponse (
					session,
					headerBuffer,
					headerBufferSize,
					processHeaderFn,
					processHeaderData
				) >= 0)
			{
				bytesRead = HTTP_Read (
						HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
						buffer,
						size
					);

				udpSession->session.buffer = 0;
				udpSession->session.readPos = 0;
				udpSession->session.bufferUsed = 0;

				HTTP_SessionSetState (
						HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
						HTTP_STATE_REQUEST_SENT
					);

				return (bytesRead);
			}

			udpSession->session.buffer = 0;
			udpSession->session.readPos = 0;
			udpSession->session.bufferUsed = 0;
		}
	}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */
	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientFinishTransaction
  ---------------------------------------------------------------------------*/
/** @memo Finish a transaction

    @doc  This function (or \Ref{HTTP_ManagedClientCloseSession}, but not both)
          must be called once a transaction is complete to allow the managed
          client to reclaim resources used by the session.

    @see HTTP_ManagedClientCloseSession

    @return nothing
 */
void HTTP_ManagedClientFinishTransaction (
		HTTPManagedClientSession* session /** the session whose transction is complete */
	)
{
	HTTP_MANAGED_CLIENT_API_ENTER(session->info.client);

	_HTTP_ManagedClientFreeResponseInfo(&session->info.responseInfo);

	if (_HTTP_ManagedClientCacheSession(session->info.client, session) < 0)
	{
		HTTP_MANAGED_CLIENT_API_EXIT(session->info.client);
		HTTP_ManagedClientCloseSession(session);
		return;
	}

	HTTP_MANAGED_CLIENT_API_EXIT(session->info.client);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientCloseSession
  ---------------------------------------------------------------------------*/
/** @memo Close a session and its associated connection

    @doc  This function performs the same task as \Ref{HTTP_ManagedClientFinishTransaction}
          except that it also closes the network connection associated with
          the given session.  This ensures that this session's connection will
          not be used for any future transactions.

    @see HTTP_ManagedClientFinishTransaction

    @return nothing
 */
void HTTP_ManagedClientCloseSession (
		HTTPManagedClientSession* session /** the session to close */
	)
{
	_HTTP_ManagedClientFreeResponseInfo(&session->info.responseInfo);

	switch (session->info.type)
	{
		case HTTP_SESSION_TYPE_TCP:
			HTTP_ClientSessionDestroy(&session->session);
			_HTTP_FreeManagedSession(session->info.client, session);
			break;

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_SSL
		case HTTP_SESSION_TYPE_SECURE_TCP:
		{
			HTTPManagedClientSecureSession* secureSession = (HTTPManagedClientSecureSession*) session;

			HTTPS_ClientSessionDestroy(&secureSession->session);
			_HTTP_FreeManagedSecureSession(session->info.client, secureSession);
			break;
		}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_SSL */

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
		case HTTP_SESSION_TYPE_UDP:
		{
			HTTPManagedClientUDPSession* udpSession = (HTTPManagedClientUDPSession*) session;

			HTTP_ClientSessionDestroy(&udpSession->session.session);
			_HTTP_FreeManagedUDPSession(session->info.client, udpSession);
			break;
		}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */
	}
}

/*---------------------------------------------------------------------------*/
void _HTTP_ManagedClientFreeResponseInfo (
		HTTPResponseInfo* responseInfo
	)
{
	if (responseInfo->charset)
	{
		rtp_strfree(responseInfo->charset);
	}

	if (responseInfo->location)
	{
		rtp_strfree(responseInfo->location);
	}

	if (responseInfo->authRealm)
	{
		rtp_strfree(responseInfo->authRealm);
	}

	rtp_memset(responseInfo, 0, sizeof(HTTPResponseInfo));
}


/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientReadResponse (
		HTTPManagedClientSession* session,
		HTTP_UINT8* buffer,
		HTTP_INT32 size,
		HTTPHeaderCallback processHeaderFn,
		void* processHeaderData
	)
{
	HTTPResponse response;
	HTTP_INT32 bytesRead;
	HTTPClientProcessHeaderContext ctx;

	rtp_memset(&response, 0, sizeof(response));

	bytesRead = HTTP_ReadResponse (
			HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
			&response,
			buffer,
			size
		);

	if (bytesRead < 0)
	{
		return (-1);
	}

	session->info.responseInfo.status = response.httpStatus;
	session->info.responseInfo.contentLength = HTTP_CONTENT_LENGTH_NONE;
	/* Added httpMessage (message on the same line as the status) to responsinfo in case
	   other fields are not returned from the server */
// HEREHERE
	rtp_strncpy(session->info.responseInfo.httpReply, response.httpReply, sizeof(session->info.responseInfo.httpReply));
	ctx.session = session;
	ctx.processHeaderFn = processHeaderFn;
	ctx.processHeaderData = processHeaderData;

	HTTP_ReadHeaders (
			HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
			(HTTPHeaderCallback)_HTTP_ManagedClientProcessHeader,
			&ctx,
			buffer + bytesRead,
			size - bytesRead
		);

	/* check for an HTTP version before 1.1; never use keep-alive option
	   on anything before this version */
	if (response.httpMajorVersion < 1 ||
	    (response.httpMajorVersion == 1 && response.httpMinorVersion < 1))
	{
		session->info.keepAlive = 0;
	}
	else
	{
		if (session->info.keepAlive)
		{
			HTTP_SessionRequireSizeKnown(&session->session.session);
		}
	}

	session->info.state = HTTP_SESSION_STATE_RESPONSE_READ;

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientProcessHeader (
		HTTPClientProcessHeaderContext* ctx,
		HTTPSession* session,
		HTTPHeaderType type,
		const HTTP_CHAR* name,
		const HTTP_CHAR* value
	)
{
	HTTPManagedClientSession* managedSession = ctx->session;

	if (ctx->processHeaderFn)
	{
		int result = ctx->processHeaderFn (
				ctx->processHeaderData,
				session,
				type,
				name,
				value
			);

		if (result != 0)
		{
			return (result);
		}
	}

	switch (type)
	{
		case HTTP_HEADER_CACHE_CONTROL:
			break;

		case HTTP_HEADER_CONNECTION:
			if (rtp_stristr(value, HTTP_CLIENT_CLOSE_STR))
			{
				/* don't try to keep the connection alive if the server
				   doesn't want to */
				managedSession->info.keepAlive = 0;
			}
			break;

		case HTTP_HEADER_CONTENT_ENCODING:
			break;

		case HTTP_HEADER_CONTENT_LANGUAGE:
			break;

		case HTTP_HEADER_CONTENT_LENGTH:
			managedSession->info.responseInfo.contentLength = rtp_atol(value);
			break;

		case HTTP_HEADER_CONTENT_LOCATION:
			break;

		case HTTP_HEADER_CONTENT_MD5:
			break;

		case HTTP_HEADER_CONTENT_RANGE:
			break;

		case HTTP_HEADER_CONTENT_TYPE:
		{
			HTTP_CHAR* charset;

			managedSession->info.responseInfo.contentType = StrToFileContentType(value);

			// now look for charset specifier
			charset = rtp_stristr(value, "charset");
			if (charset)
			{
				charset += 7;
				while (IS_WHITESPACE(*charset))
				{
					charset++;
				}
				while (*charset)
				{
					if (*charset == '=')
					{
						charset++;
						while (IS_WHITESPACE(*charset))
						{
							charset++;
						}
						managedSession->info.responseInfo.charset = rtp_strdup(charset);
						break;
					}
					charset++;
				}
			}
			break;
		}

		case HTTP_HEADER_DATE:
			rtp_date_parse_time(&managedSession->info.responseInfo.date, value);
			break;

		case HTTP_HEADER_EXPIRES:
			rtp_date_parse_time(&managedSession->info.responseInfo.expires, value);
			break;

		case HTTP_HEADER_KEEP_ALIVE:
			break;

		case HTTP_HEADER_LAST_MODIFIED:
			rtp_date_parse_time(&managedSession->info.responseInfo.lastModified, value);
			break;

		case HTTP_HEADER_LOCATION:
			managedSession->info.responseInfo.location = rtp_strdup(value);
			break;

		case HTTP_HEADER_REFRESH:
			break;

		case HTTP_HEADER_SERVER:
			break;

		case HTTP_HEADER_SET_COOKIE:
			if (managedSession->info.client->cookieContext)
			{
				managedSession->info.client->cookieContext->setCookieFn (
						managedSession->info.client->cookieContext,
						value,
						managedSession->info.host,
						managedSession->info.target
					);
			}
			break;

		case HTTP_HEADER_WWW_AUTHENTICATE:
			/* only process authentication challenges if authentication is enabled */
			if (managedSession->info.client->authContext)
			{
				managedSession->info.responseInfo.authStatus =
					managedSession->info.client->authContext->processChallenge (
							managedSession->info.client->authContext,
							managedSession->info.host,
							managedSession->info.port,
							managedSession->info.target,
							managedSession->info.authTimestamp,
							value,
							&managedSession->info.responseInfo.authRealm
						);
			}
			break;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedToClientSessionList (
		HTTPClientSession** sList,
		HTTP_INT16* sNum,
		HTTPManagedClientSession** mList,
		HTTP_INT16 mNum
	)
{
	HTTP_INT16 n;

	if (*sNum < mNum)
	{
		mNum = *sNum;
	}

	for (n = 0; n < mNum; n++)
	{
		sList[n] = &mList[n]->session;
	}

	*sNum = mNum;

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ClientToManagedSessionList (
		HTTPManagedClientSession** mList,
		HTTP_INT16* mNum,
		HTTPClientSession** sList,
		HTTP_INT16 sNum
	)
{
	HTTP_INT16 n;

	for (n = 0; n < *mNum;)
	{
		if (!_HTTP_ClientSessionListIsSet(sList, sNum, &mList[n]->session))
		{
			(*mNum)--;
			if (*mNum > 0)
			{
				mList[n] = mList[*mNum];
			}
		}
		else
		{
			n++;
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_ClientSessionListIsSet (
		HTTPClientSession** sList,
		HTTP_INT16 sNum,
		HTTPClientSession* session
	)
{
	HTTP_INT16 n;

	for (n = 0; n < sNum; n++)
	{
		if (sList[n] == session)
		{
			return (1);
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTPManagedClientSession* _HTTP_AllocManagedSession (
		HTTPManagedClient* client
	)
{
	return ( (HTTPManagedClientSession*) rtp_malloc(sizeof(HTTPManagedClientSession)) );
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeManagedSession (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session
	)
{
	rtp_free(session);
}

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
/*---------------------------------------------------------------------------*/
HTTPManagedClientUDPSession* _HTTP_AllocManagedUDPSession (
		HTTPManagedClient* client
	)
{
	return ( (HTTPManagedClientUDPSession*) rtp_malloc(sizeof(HTTPManagedClientUDPSession)) );
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeManagedUDPSession (
		HTTPManagedClient* client,
		HTTPManagedClientUDPSession* session
	)
{
	rtp_free(session);
}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_SSL
/*---------------------------------------------------------------------------*/
HTTPManagedClientSecureSession* _HTTP_AllocManagedSecureSession (
		HTTPManagedClient* client
	)
{
	return ( (HTTPManagedClientSecureSession*) rtp_malloc(sizeof(HTTPManagedClientSecureSession)) );
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeManagedSecureSession (
		HTTPManagedClient* client,
		HTTPManagedClientSecureSession* session
	)
{
	rtp_free(session);
}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_SSL */

/*---------------------------------------------------------------------------*/
HTTPManagedClientSession* HTTP_ManagedClientFindOpenSession (
		HTTPManagedClient* client,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		HTTPManagedSessionType type
	)
{
	HTTPManagedClientSession* session;

	/* first get rid of any stale connections */
	HTTP_ManagedClientCloseStale(client);

	HTTP_MANAGED_CLIENT_API_ENTER(client);

	session = (HTTPManagedClientSession*) client->cachedSessions.next;

	while (session != (HTTPManagedClientSession*) &client->cachedSessions)
	{
		if (!rtp_stricmp(session->info.host, hostName) &&
	        (session->info.port == port) &&
	        (session->info.type == type))
		{
			DLLIST_REMOVE(&session->info.node);
			DLLIST_INIT(&session->info.node);
			break;
		}

		session = (HTTPManagedClientSession*) session->info.node.next;
	}

	if (session == (HTTPManagedClientSession*) &client->cachedSessions)
	{
		session = 0;
	}

	HTTP_MANAGED_CLIENT_API_EXIT(client);

	return (session);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientCacheSession (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session
	)
{
	/* if the session has been reset by the other side or if keep alive is
	   disabled either for this connection or on the client in general,
	   then fail */
	if (!client->useKeepAlive || !session->info.keepAlive ||
	    !HTTP_ClientSessionIsAlive(&session->session))
	{
		return (-1);
	}

	/* if there is data left to be read on this session (as the result of
	   an uncomplete transaction) then read up to what has been locally
	   received (i.e. read as much as we can without blocking); if this
	   clears out all the data from the last transaction, then cache it;
	   otherwise, close the socket and cut our losses */

	if (session->info.state != HTTP_SESSION_STATE_DONE)
	{
		if (session->info.state == HTTP_SESSION_STATE_RESPONSE_READ)
		{
			/* for now don't cache when in this state; we could try to read
			   any remaining data from buffers (without blocking, of course)
			   to see if this puts the session in the 'done' state */
			return (-1);
		}
		else
		{
			/* in some strange state - don't even try to cache it */
			return (-1);
		}
	}

	if (HTTP_ClientSessionReset(&session->session) < 0)
	{
		return (-1);
	}

	session->info.state = HTTP_SESSION_STATE_OPEN;
	session->info.idleSinceMsec = rtp_get_system_msec();
	DLLIST_INSERT_AFTER(&client->cachedSessions, &session->info.node);

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTPManagedClientSession* _HTTP_ManagedClientCreateSession (
		HTTPManagedClient* client,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTPManagedSessionType type,
		unsigned blocking,
		HTTP_INT32 timeoutMsec
	)
{
	switch (type)
	{
		case HTTP_SESSION_TYPE_TCP:
		{
			HTTPManagedClientSession* newSession = 0;

			newSession = _HTTP_AllocManagedSession(client);
			if (newSession)
			{
				rtp_memset(newSession, 0, sizeof(HTTPManagedClientSession));

				if (HTTP_ClientSessionInit (
						&client->clientContext,
						&newSession->session
					) >= 0)
				{
					newSession->info.client = client;
					DLLIST_INIT(&newSession->info.node);
					newSession->info.state = HTTP_SESSION_STATE_NEW;
					newSession->info.type = type;

					HTTP_SetWriteBufferSize (
							HTTP_CLIENT_SESSION_TO_SESSION(&newSession->session),
							client->writeBufferSize
						);

					if (HTTP_ClientSessionOpenHost (
							&newSession->session,
							host,
							port,
							blocking,
							RTP_TIMEOUT_INFINITE
						) >= 0)
					{
						return (newSession);
					}

					HTTP_ClientSessionDestroy(&newSession->session);
				}

				_HTTP_FreeManagedSession(client, newSession);
				newSession = 0;
			}
			break;
		}

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_SSL
		case HTTP_SESSION_TYPE_SECURE_TCP:
		{
			HTTPManagedClientSecureSession* newSession = 0;

			newSession = _HTTP_AllocManagedSecureSession(client);
			if (newSession)
			{
				rtp_memset(newSession, 0, sizeof(HTTPManagedClientSecureSession));

				if (HTTPS_ClientSessionInit (
						&client->clientContext,
						&newSession->session,
						0, /* cert filter fn */
						0  /* data for fn */
					) >= 0)
				{
					newSession->info.client = client;
					DLLIST_INIT(&newSession->info.node);
					newSession->info.state = HTTP_SESSION_STATE_NEW;
					newSession->info.type = type;

					HTTP_SetWriteBufferSize (
							HTTP_CLIENT_SESSION_TO_SESSION(&newSession->session.session),
							client->writeBufferSize
						);

					if (HTTPS_ClientSessionOpenHost (
							&newSession->session,
							host,
							port,
							blocking,
							RTP_TIMEOUT_INFINITE
						) >= 0)
					{
						return ((HTTPManagedClientSession*) newSession);
					}

					HTTPS_ClientSessionDestroy(&newSession->session);
				}

				_HTTP_FreeManagedSecureSession(client, newSession);
				newSession = 0;
			}
			break;
		}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_SSL */

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
		case HTTP_SESSION_TYPE_UDP:
		{
			HTTPManagedClientUDPSession* newSession = 0;

			newSession = _HTTP_AllocManagedUDPSession(client);
			if (newSession)
			{
				rtp_memset(newSession, 0, sizeof(HTTPManagedClientUDPSession));

				if (HTTP_ClientSessionInitEx (
						&client->clientContext,
						&newSession->session.session,
						_HTTP_ReadUDP,
						_HTTP_WriteUDP
					) >= 0)
				{
					HTTP_BOOL resolvedHostAddr = 0;

					newSession->info.client = client;
					DLLIST_INIT(&newSession->info.node);
					newSession->info.state = HTTP_SESSION_STATE_NEW;
					newSession->info.type = type;

					HTTP_SetWriteBufferSize (
							HTTP_CLIENT_SESSION_TO_SESSION(&newSession->session.session),
							client->writeBufferSize
						);

					newSession->session.session.netAddr.port = port;

					if (rtp_net_gethostbyname (newSession->session.session.netAddr.ipAddr,
											   &newSession->session.session.netAddr.type,
											   (char*) host) < 0)
					{
						if (rtp_net_str_to_ip(newSession->session.session.netAddr.ipAddr, (char*) host, &newSession->session.session.netAddr.type) >= 0)
						{
							char testStr[100];

							rtp_net_ip_to_str(testStr, newSession->session.session.netAddr.ipAddr, newSession->session.session.netAddr.type);

							if (!rtp_strcmp(testStr, host))
							{
								resolvedHostAddr = 1;
							}
						}
					}
					else
					{
						resolvedHostAddr = 1;
					}

					if (resolvedHostAddr);
					{
						if (newSession->session.session.host)
						{
							rtp_strfree(newSession->session.session.host);
						}

						newSession->session.session.host = rtp_strdup(host);

						if (newSession->session.session.host)
						{
							if (rtp_net_socket_datagram_dual (&newSession->session.session.netSock,
                                                         newSession->session.session.netAddr.type) >= 0)
							{
								/* attempt to put the socket into non-blocking mode */
								rtp_net_setblocking (newSession->session.session.netSock, blocking);

								newSession->session.session.socketOpen = 1;

								/* bind to any IP address (of the same type as the server) and port */
								if (rtp_net_bind_dual (newSession->session.session.netSock,
												  RTP_NET_DATAGRAM, /* bind a datagram socket */
												  0 /* addr = ANY */,
												  0 /* port = ANY*/,
												  newSession->session.session.netAddr.type) >= 0)
								{
									return ((HTTPManagedClientSession*) newSession);
								}

								newSession->session.session.socketOpen = 0;

								rtp_net_closesocket(newSession->session.session.netSock);
							}
						}
					}

					HTTP_ClientSessionDestroy(&newSession->session.session);
				}

				_HTTP_FreeManagedUDPSession(client, newSession);
				newSession = 0;
			}
			break;
		}
#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ManagedClientAuthWrite (
		HTTPManagedClientRequestInfo* requestInfo,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	)
{
	return (requestInfo->client->authContext->printAuthorization (
			requestInfo->client->authContext,
			requestInfo->request->method,
			requestInfo->session->info.host,
			requestInfo->session->info.port,
			requestInfo->request->target,
			writeFn,
			requestStream,
			&requestInfo->session->info.authTimestamp
		));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ManagedClientCookieWrite (
		HTTPManagedClientRequestInfo* requestInfo,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	)
{
	return (requestInfo->client->cookieContext->printCookiesFn (
			requestInfo->client->cookieContext,
			requestInfo->session->info.host,
			requestInfo->request->target,
			writeFn,
			requestStream
		));
}


/*---------------------------------------------------------------------------*/
int _HTTP_AddPostHeaders (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		HTTPRequest* request,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength
	)
{
	/* the client SHOULD always specify the content type! */
	if (_HTTP_ManagedClientSetDefaultHeaders (
			client,
			session,
			request
		) >= 0)
	{
		if (!contentType ||
			HTTP_SetRequestHeaderStr (
					request,
					HTTP_HeaderTypeToString(HTTP_HEADER_CONTENT_TYPE),
					contentType
				) >= 0)
		{
			/* if the content length is specified ahead of time, then
			   put it in the MIME header; otherwise, use chunked transfer
			   encoding */
			if ((contentLength >= 0 &&
				 HTTP_SetRequestHeaderInt (
			    		request,
			    		HTTP_HeaderTypeToString(HTTP_HEADER_CONTENT_LENGTH),
			    		contentLength
					) >= 0) ||
				(contentLength == HTTP_CONTENT_LENGTH_UNKNOWN &&
				 HTTP_SetRequestHeaderStr (
						request,
						HTTP_HeaderTypeToString(HTTP_HEADER_TRANSFER_ENCODING),
						HTTP_CLIENT_CHUNKED_STR
					) >= 0) ||
				(contentLength == HTTP_CONTENT_LENGTH_NONE))
			{
				return (0);
			}
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
void HTTP_ManagedClientCloseCond (
		HTTPManagedClient* client,
		unsigned testFn (
				HTTPManagedClient* client,
				HTTPManagedClientSession* session,
				void* data
			),
		void* data
	)
{
	HTTPManagedClientSession* session;
	HTTPManagedClientSession* next;

	HTTP_MANAGED_CLIENT_API_ENTER(client);

	session = (HTTPManagedClientSession*) client->cachedSessions.next;

	while (session != (HTTPManagedClientSession*) &client->cachedSessions)
	{
		next = (HTTPManagedClientSession*) session->info.node.next;

		if (testFn(client, session, data))
		{
			DLLIST_REMOVE(&session->info.node);
			HTTP_MANAGED_CLIENT_API_EXIT(client);

			HTTP_ManagedClientCloseSession(session);

			HTTP_MANAGED_CLIENT_API_ENTER(client);
		}

		session = next;
	}

	HTTP_MANAGED_CLIENT_API_EXIT(client);
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_ManagedClientMatchAll (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	)
{
	return (1);
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_ManagedClientMatchHost (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	)
{
	HTTPManagedClientHost* hostInfo = (HTTPManagedClientHost*) data;

	return (!rtp_stricmp(session->info.host, hostInfo->hostName) &&
	        (hostInfo->port == 0 || hostInfo->port == session->info.port));
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_ManagedClientMatchStale (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		void* data
	)
{
	HTTP_UINT32* currentTimeMsec = (HTTP_INT32*) data;

	return (!HTTP_ClientSessionIsAlive(&session->session) ||
	        (HTTP_INT32)(*currentTimeMsec - session->info.idleSinceMsec) >
			 session->info.idleTimeoutMsec);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientInit (
		HTTPManagedClient* client,        /** pointer to uninitialized HTTPManagedClient structure */
		const HTTP_CHAR* userAgent,       /** string to use to identify client to remote hosts */
		const HTTP_CHAR* acceptTypes,     /** string to send to server to indicate what data types we accept */
		unsigned useKeepAlive,            /** boolean: if non-zero, try to keep connects open to speed things up */
		HTTPCookieContext* cookieContext, /** optional: initialized instance of HTTPCookieContext for cookie support */
		HTTPAuthContext* authContext,     /** optional: initialized instance of HTTPAuthContext for authentication */
		RTP_HANDLE sslContext,            /** optional: initialized RTSSL context if secure HTTP is to be utilitized */
		unsigned sslEnabled,              /** boolean: if sslContext is valid this MUST be non-zero; otherwise 0 */
		HTTP_INT32 writeBufferSize,       /** number of bytes to buffer when sending */
		HTTP_INT32 maxTotalConnections,   /** max total connections to open simultaneously */
		HTTP_INT32 maxHostConnections     /** max connections allowed to any one remote host */
	)
{
	rtp_memset(client, 0, sizeof(HTTPManagedClient));

	if (HTTP_ClientInit(&client->clientContext, sslContext, sslEnabled) >= 0)
	{
		client->authContext = authContext;
		client->cookieContext = cookieContext;
		client->userAgent = userAgent;
		client->acceptTypes = acceptTypes;
		client->maxSessionIdleMsecs = HTTP_CFG_MAX_SESSION_IDLE_MSECS;
		client->authContext = authContext;
		client->cookieContext = cookieContext;
		client->useKeepAlive = useKeepAlive;
		client->writeBufferSize = writeBufferSize;
		client->maxTotalConnections = maxTotalConnections;
		client->maxHostConnections = maxHostConnections;

		DLLIST_INIT(&client->cachedSessions);

		return (0);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientSetDefaultHeaders (
		HTTPManagedClient* client,
		HTTPManagedClientSession* session,
		HTTPRequest* request
	)
{
	HTTPManagedClientRequestInfo requestInfo;

	requestInfo.client = client;
	requestInfo.session = session;
	requestInfo.request = request;

	if (client->cookieContext)
	{
		HTTP_SetRequestHeaderFn (
				request,
				HTTP_HeaderTypeToString(HTTP_HEADER_COOKIE),
				(WriteString)_HTTP_ManagedClientCookieWrite,
				&requestInfo
			);
	}

	if (client->authContext)
	{
		HTTP_SetRequestHeaderFn (
				request,
				HTTP_HeaderTypeToString(HTTP_HEADER_AUTHORIZATION),
				(WriteString)_HTTP_ManagedClientAuthWrite,
				&requestInfo
			);
	}

	if (client->userAgent)
	{
		HTTP_SetRequestHeaderStr (
				request,
				HTTP_HeaderTypeToString(HTTP_HEADER_USER_AGENT),
				client->userAgent
			);
	}

	if (client->acceptTypes)
	{
		HTTP_SetRequestHeaderStr (
				request,
				HTTP_HeaderTypeToString(HTTP_HEADER_ACCEPT),
				client->acceptTypes
			);
	}

	if (session->info.type != HTTP_SESSION_TYPE_UDP)
	{
		if (client->useKeepAlive)
		{
			HTTP_SetRequestHeaderStr (
					request,
					HTTP_HeaderTypeToString(HTTP_HEADER_CONNECTION),
					HTTP_CLIENT_KEEP_ALIVE_STR
				);

			HTTP_SetRequestHeaderInt (
					request,
					HTTP_HeaderTypeToString(HTTP_HEADER_KEEP_ALIVE),
					(client->maxSessionIdleMsecs + 999) / 1000
				);
		}
		else
		{
			HTTP_SetRequestHeaderStr (
					request,
					HTTP_HeaderTypeToString(HTTP_HEADER_CONNECTION),
					HTTP_CLIENT_CLOSE_STR
				);
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientRequestEx (
		HTTPManagedClientSession* session,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* ifModifiedSince,
		HTTP_CHAR* contentType,
		HTTP_INT32 contentLength,
		HTTPRequest* request
	)
{
	int result = -1;

	if (session && session->info.state == HTTP_SESSION_STATE_OPEN)
	{
		if (HTTP_ClientRequestInit (
				&session->session,
				request,
				method,  /* method */
				path,    /* target */
				1, 1     /* HTTP version */
			) >= 0)
		{
			if (_HTTP_AddPostHeaders (
					session->info.client,
					session,
					request,
					contentType,
					contentLength
				) >= 0)
			{
				if (!ifModifiedSince ||
					HTTP_SetRequestHeaderTime (
			    			request,
			    			HTTP_HeaderTypeToString(HTTP_HEADER_IF_MODIFIED_SINCE),
			    			ifModifiedSince
			    		) >= 0)
				{
					/* write the request to the buffered stream */
					switch (contentLength)
					{
						default:
							session->info.state = HTTP_SESSION_STATE_BUILDING_HEADER;
							break;

						case HTTP_CONTENT_LENGTH_UNKNOWN:
							session->info.state = HTTP_SESSION_STATE_BUILDING_HEADER_CHUNKED;
							break;

						case HTTP_CONTENT_LENGTH_NONE:
							session->info.state = HTTP_SESSION_STATE_BUILDING_HEADER_NO_DATA;
							break;
					}
					session->info.target = path;
					result = 0;
				}
			}

			if (result < 0)
			{
				session->info.state = HTTP_SESSION_STATE_ERROR;
				HTTP_FreeRequest(request);
			}
		}
	}

	return (result);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ManagedClientRequestHeaderDone (
		HTTPManagedClientSession* session,
		HTTPRequest* request
	)
{
	int result = -1;

	if (HTTP_WriteRequest (
			HTTP_CLIENT_SESSION_TO_SESSION(&session->session),
			request
		) >= 0)
	{
		switch (session->info.state)
		{
			case HTTP_SESSION_STATE_BUILDING_HEADER:
				session->info.state = HTTP_SESSION_STATE_WRITING_DATA;
				break;

			case HTTP_SESSION_STATE_BUILDING_HEADER_CHUNKED:
				session->info.state = HTTP_SESSION_STATE_WRITING_CHUNKS;
				break;

			/*case HTTP_SESSION_STATE_BUILDING_HEADER_NO_DATA:*/
			default:
				HTTP_WriteFlush(HTTP_CLIENT_SESSION_TO_SESSION(&session->session));
				session->info.state = HTTP_SESSION_STATE_REQUEST_SENT;
				break;
		}

		result = 0;
	}

	if (result < 0)
	{
		session->info.state = HTTP_SESSION_STATE_ERROR;
	}

	HTTP_FreeRequest(request);

	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_ManagedClientGetSessionSocket
  ---------------------------------------------------------------------------*/
/** @memo get the socket accociated with the http session

    @doc
    @return
 */
RTP_SOCKET HTTP_ManagedClientGetSessionSocket (
		HTTPManagedClientSession* session
	)
{
	return(session->session.netSock);
}

#ifdef HTTP_MANAGED_CLIENT_INCLUDE_UDP
/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ReadUDP (
		void* streamPtr,
		HTTP_UINT8* buffer,
		HTTP_INT32 min,
		HTTP_INT32 max
	)
{
	HTTPUDPClientSession* session = (HTTPUDPClientSession*) streamPtr;

	if (max > session->bufferUsed - session->readPos)
	{
		max = session->bufferUsed - session->readPos;
	}

	rtp_memmove(buffer, session->buffer, max);
	session->readPos += max;

	return (max);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_WriteUDP (
		void* streamPtr,
		const HTTP_UINT8* buffer,
		HTTP_INT32 size
	)
{
	HTTPUDPClientSession* clientSession = (HTTPUDPClientSession*) streamPtr;

	RTP_LOG_WRITE("_HTTP_WriteUDP WRITE", buffer, size)
	return (rtp_net_sendto (
			clientSession->session.netSock,
			buffer,
			size,
			clientSession->session.netAddr.ipAddr,
			clientSession->session.netAddr.port,
			clientSession->session.netAddr.type
		));
}

#endif /* HTTP_MANAGED_CLIENT_INCLUDE_UDP */
