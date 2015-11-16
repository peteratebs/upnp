//
// HTTPAUTH.C -
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

#include "httpauth.h"
#include "rtpstdup.h"
#include "md5.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTP_CFG_MAX_HOSTNAME_LEN   80
#define HTTP_CFG_MAX_PASSWORD_LEN   16
#define HTTP_CFG_MAX_PATH_LEN       256
#define HTTP_CFG_MAX_REALMNAME_LEN  36
#define HTTP_CFG_MAX_USERNAME_LEN   32
#define HTTP_CFG_AUTH_TTL_SEC       (60*3)
#define HTTP_CFG_MAX_NONCE_LEN      64
#define HTTP_CFG_MAX_OPAQUE_LEN     128

#ifdef EBSMIN
#undef EBSMIN
#endif

#ifdef EBSMAX
#undef EBSMAX
#endif

#ifdef EBSCLIP
#undef EBSCLIP
#endif

#define EBSMIN(X,Y)    ((X)<(Y)?(X):(Y))
#define EBSMAX(X,Y)    ((X)>(Y)?(X):(Y))
#define EBSCLIP(S,A,B) (EBSMAX(A,EBSMIN(S,B)))

#define IS_WHITESPACE(C) ((C)==' ' ||  (C)=='\r' || (C)=='\n' || (C)=='\t' || (C)=='\f' || (C)=='\v')

#ifdef HTTP_CLIENT_AUTH_THREADSAFE
#define HTTP_CLIENT_AUTH_API_ENTER(X)       rtp_sig_mutex_claim((X)->lock)
#define HTTP_CLIENT_AUTH_API_EXIT(X)        rtp_sig_mutex_release((X)->lock)
#else
#define HTTP_CLIENT_AUTH_API_ENTER(X)
#define HTTP_CLIENT_AUTH_API_EXIT(X)
#endif /* HTTP_CLIENT_AUTH_THREADSAFE */

#define HTTP_AUTH_TIMESTAMP_COMPARE(X,Y)    ((HTTP_INT32)((X)-(Y)))

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef struct s_HTTPAuthenticationHost  HTTPAuthenticationHost;
typedef struct s_HTTPAuthenticationRealm HTTPAuthenticationRealm;
typedef enum e_HTTPAuthQoPType           HTTPAuthQoPType;
typedef enum e_HTTPAuthHashAlgorithmType HTTPAuthHashAlgorithmType;

enum e_HTTPAuthQoPType
{
	HTTP_QOP_UNRECOGNIZED = -1,
	HTTP_QOP_NONE = 0,
	HTTP_QOP_AUTH,
	HTTP_QOP_AUTH_INT,
	HTTP_NUM_QOP_TYPES
};

enum e_HTTPAuthHashAlgorithmType
{
	HTTP_ALGORITHM_UNKNOWN = -1,
	HTTP_ALGORITHM_MD5 = 0,
	HTTP_ALGORITHM_MD5_SESS,
	HTTP_NUM_ALGORITHM_TYPES
};

struct s_HTTPAuthenticationRealm
{
	DLListNode node;

	HTTP_CHAR realmName[HTTP_CFG_MAX_REALMNAME_LEN];
	HTTP_CHAR path[HTTP_CFG_MAX_PATH_LEN];
	RTP_TIMESTAMP expires;
	HTTP_INT32 ttlSec;
	HTTPAuthenticationScheme scheme; // "Basic", "Digest", etc
	HTTP_UINT32 timestamp;
	unsigned rejected;

	HTTP_CHAR userName[HTTP_CFG_MAX_USERNAME_LEN];
	HTTP_CHAR password[HTTP_CFG_MAX_PASSWORD_LEN];

	union
	{
		struct
		{
			HTTP_CHAR serverNonce[HTTP_CFG_MAX_NONCE_LEN];
			HTTP_CHAR serverOpaque[HTTP_CFG_MAX_OPAQUE_LEN];
			HTTP_INT32 nonceCount;
			HTTP_CHAR clientNonce[HTTP_CFG_MAX_NONCE_LEN];
			HTTPAuthQoPType qop;
			HTTPAuthHashAlgorithmType hashAlgorithm;
		} digest;

	} param;

	HTTPAuthenticationHost *host;
};

struct s_HTTPAuthenticationHost
{
	DLListNode  node;
	HTTP_CHAR   hostName[HTTP_CFG_MAX_HOSTNAME_LEN];
	HTTP_UINT16 port;
	DLListNode  realmList;
};

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

/* Authentication Realm lookup functions */

int _HTTP_AuthContextInit (
		HTTPAuthContext* ctx
	);

HTTPAuthStatus _HTTP_AuthProcessChallenge (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		HTTP_UINT32 authTimestamp,
		const HTTP_CHAR* challenge,
		HTTP_CHAR** realm
	);

/* Authentication formatting functions */
HTTP_INT32 _HTTP_PrintAuthorization (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* method,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream,
		HTTP_UINT32* authTimestamp
	);

HTTPAuthenticationRealm * _HTTP_CreateAuthenticationRealm (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		const HTTP_CHAR* realmName,
		HTTP_INT32 ttlSec,
		HTTPAuthenticationScheme scheme
	);

HTTPAuthenticationRealm* _HTTP_GetAuthenticationRealm (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		HTTP_CHAR* realmName
	);

HTTPAuthenticationRealm* _HTTP_GetAuthenticationRealmByPath (
		HTTPAuthContext* ctx,
		const HTTP_CHAR *hostName,
		HTTP_UINT16 port,
		const HTTP_CHAR *path
	);

HTTPAuthenticationHost* _HTTP_GetAuthenticationHost (
		HTTPAuthContext* ctx,
		const HTTP_CHAR *hostName,
		HTTP_UINT16 port
	);

void _HTTP_AddAuthenticationHost (
		HTTPAuthContext* ctx,
		HTTPAuthenticationHost *host
	);

void _HTTP_RemoveAuthenticationHost (
		HTTPAuthenticationHost *host
	);

/* Authentication Host management functions */
HTTPAuthenticationHost* _HTTP_NewHost (
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port
	);

void _HTTP_DeleteHost (
		HTTPAuthenticationHost *host
	);

void _HTTP_HostAddRealm (
		HTTPAuthenticationHost *host,
		HTTPAuthenticationRealm *realm
	);

void _HTTP_HostRemoveRealm (
		HTTPAuthenticationHost *host,
		HTTPAuthenticationRealm *realm
	);

/* Authentication Realm management functions */
HTTPAuthenticationRealm* _HTTP_NewRealm (
		const HTTP_CHAR* realmName,
		const HTTP_CHAR* path,
		HTTP_INT32 ttlSec,
		HTTPAuthenticationScheme scheme
	);

void _HTTP_DeleteRealm (
		HTTPAuthenticationRealm *realm
	);

void _HTTP_RealmSetName (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* name
	);

void _HTTP_RealmSetUser (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* user
	);

void _HTTP_RealmSetPassword (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* password
	);

void _HTTP_RealmSetTtl (
		HTTPAuthenticationRealm *realm,
		HTTP_INT32 ttlSec
	);

HTTP_INT32 _HTTP_RealmPrintAuthorization (
		HTTPAuthenticationRealm *realm,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	);

void _HTTP_RealmUpdatePath (
		HTTPAuthenticationRealm *realm,
		const HTTP_CHAR* path
	);

int  _HTTP_RealmExpired (
		HTTPAuthenticationRealm *realm
	);

void _HTTP_CheckRealmExpirations (
		HTTPAuthContext* ctx
	);

void _HTTP_HashToHex (
		HTTP_CHAR* str,
		HTTP_UINT8* hash,
		HTTP_INT32 len
	);

void _HTTP_UINT32ToHex (
		HTTP_CHAR* str,
		HTTP_UINT32 i
	);

HTTP_INT32 _HTTP_Base64Encode (
		HTTP_CHAR* dst,
		HTTP_UINT8* src,
		HTTP_INT32 size
	);

void _HTTP_ClearAllAuthorizations (
		HTTPAuthContext* ctx
	);

/*****************************************************************************/
// Data
/*****************************************************************************/

static const char _base64[64] =
{
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
	'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
	'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
	'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

static const char _hexDigit[16] =
{
	'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

static const char HTTP_CLIENT_BASIC_STR[] = {
		'B','a','s','i','c',0
	};

static const HTTP_CHAR* schemeNames[] = {"unknown", "Basic", "Digest"};

static const HTTP_CHAR* httpAuthQoPNames[] = {
		"",         /* HTTP_QOP_NONE */
		"auth",     /* HTTP_QOP_AUTH */
		"auth-int", /* HTTP_QOP_AUTH_INT */
	};

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------
  HTTP_AuthContextInit
  ---------------------------------------------------------------------------*/
/** @memo Initialize an HTTP Client-side Authentication context.

    @doc  The HTTPAuthContext struct stores a database of user/password
          creditials and provides an interface to the HTTPManagedClient
          so that the managed client can use this database to send authorization
          information to HTTP servers that require it for certain requests.

          This function initializes the HTTPAuthContext with an empty credential
          database.  Once the HTTPAuthContext is initialized, it can be
          passed into \Ref{HTTP_ManagedClientInit} to enable HTTP authentication.

    @see  HTTP_AuthContextDestroy

    @return 0 on success, negative on failure
 */
int HTTP_AuthContextInit (
		HTTPAuthContext* ctx /** pointer to an uninitialized HTTPAuthContext struct; this
		                         struct must not be freed or reused until \Ref{HTTP_AuthContextDestroy}
		                         is called */
	)
{
#ifdef HTTP_CLIENT_AUTH_THREADSAFE
	return (-1);
#else
	return (_HTTP_AuthContextInit(ctx));
#endif
}

#ifdef HTTP_CLIENT_AUTH_THREADSAFE
/*---------------------------------------------------------------------------
  HTTP_AuthContextInitMT
  ---------------------------------------------------------------------------*/
/** @memo Initialize a thread-safe HTTPAuthContext

    @doc  This function performs the exact same duty as \Ref{HTTP_AuthContextInit},
          with the addition of initializing the HTTPAuthContext for
          thread-safe operation.  The httpauth module MUST be compiled with
          the preprocessor symbol HTTP_CLIENT_AUTH_THREADSAFE defined to enable
          thread-safe mode in order for this function to be used.

    @see  HTTP_AuthContextInit

    @return 0 on success, negative on failure
 */
int HTTP_AuthContextInitMT (
		HTTPAuthContext* ctx, /** pointer to an uninitialized HTTPAuthContext struct; this
		                          struct must not be freed or reused until \Ref{HTTP_AuthContextDestroy}
		                          is called */
		RTP_MUTEX lock        /** a mutex created by rtp_sig_mutex_alloc use to protect
		                          the HTTPAuthContext from being re-entered.  This mutex
		                          must be freed AFTER \Ref{HTTP_AuthContextDestroy} is called
		                          to destroy the HTTPAuthContext, and must not be used for
		                          any other purpose while the HTTPAuthContext is active */
	)
{
	if (_HTTP_AuthContextInit(ctx) >= 0)
	{
		ctx->lock = lock;
		return (0);
	}

	return (-1);
}
#endif

/*---------------------------------------------------------------------------
  HTTP_AuthContextDestroy
  ---------------------------------------------------------------------------*/
/** @memo Destroy an HTTPAuthContext

    @doc  This function will free all data stored in the credential database
          for this HTTPAuthContext.  It should only be called after any
          HTTPManagedClient's which use it have been destroyed using
          \Ref{HTTP_ManagedClientDestroy}.

    @see  HTTP_AuthContextInit

    @return 0 on success, negative on failure
 */
void HTTP_AuthContextDestroy (
		HTTPAuthContext* ctx   /** The HTTP Client Authentication context to destroy */
	)
{
	_HTTP_ClearAllAuthorizations(ctx);
}

/*---------------------------------------------------------------------------
  HTTP_SetAuthorization
  ---------------------------------------------------------------------------*/
/** @memo Create or update credentials for a set of URLs.

    @doc  This function sets the user name and password to be used to
          access all URLs beginning with the given path string on the given
          server.  If there are no credentials already in the database for
          the given server/path, then a new entry is created with the
          given user/password using Basic authentication method.  If the
          server/path is already in the database, the user name and password
          used for access on that server/path are updated and the authentication
          method is left unchanged.

          Only one user (with password) is stored per server/path.

    @see  HTTP_AuthContextInit

    @return 0 on success, negative on failure
 */
int HTTP_SetAuthorization (
		HTTPAuthContext* ctx, /** authorization database context */
		HTTP_CHAR* host,      /** host name to set credentials for */
		HTTP_UINT16 port,     /** server port number */
		HTTP_CHAR* path,      /** the credentials are set for all paths on the
		                          given server that begin with this path string */
		HTTP_CHAR* userName,  /** the user name to use when sending requests to
		                          the given server/path */
		HTTP_CHAR* password,  /** the password to associate with the user name */
		HTTP_INT32 ttlSec     /** how long to use this authorization before discarding it
		                          from the database */
	)
{
	HTTPAuthenticationRealm* realm = 0;

	HTTP_CLIENT_AUTH_API_ENTER(ctx);

	/* first try to find an existing authentication realm that matches */
	if (path)
	{
		/* find by common subtree */
		realm = _HTTP_GetAuthenticationRealmByPath(ctx, host, port, path);
	}

	/* no such realm is known; create a new one */
	if (!realm)
	{
		realm = _HTTP_CreateAuthenticationRealm (
				ctx,
				host,
				port,
				path,
				0,
				ttlSec,
				HTTP_AUTHENTICATION_BASIC
			);

		if (!realm)
		{
			HTTP_CLIENT_AUTH_API_EXIT(ctx);
			return (-1);
		}
	}

	_HTTP_RealmSetTtl(realm, ttlSec);
	_HTTP_RealmSetUser(realm, userName);
	_HTTP_RealmSetPassword(realm, password);
	realm->timestamp = ctx->currentTimestamp;
	realm->rejected = 0;

	ctx->currentTimestamp++;

	HTTP_CLIENT_AUTH_API_EXIT(ctx);

	return (0);
}

/*---------------------------------------------------------------------------
  HTTP_ClearAllAuthorizations
  ---------------------------------------------------------------------------*/
/** @memo Clear all authorizations from a client authentication database.

    @doc  Clears all user/password credentials from an HTTPAuthContext, returning
          it to its initialized state.

    @see  HTTP_AuthContextDestroy, HTTP_SetAuthorization

    @return 0 on success, negative on failure
 */
void HTTP_ClearAllAuthorizations (
		HTTPAuthContext* ctx /** the authentication database */
	)
{
	HTTP_CLIENT_AUTH_API_ENTER(ctx);
	_HTTP_ClearAllAuthorizations(ctx);
	HTTP_CLIENT_AUTH_API_EXIT(ctx);
}

/*---------------------------------------------------------------------------
  HTTP_ClearAuthorization
  ---------------------------------------------------------------------------*/
/** @memo Delete the user name and password for a given server.

    @doc  Clears the user/password credentials from an HTTPAuthContext for the
          given server/path, if they exist.

    @see  HTTP_AuthContextDestroy, HTTP_SetAuthorization

    @return 0 on success, negative on failure
 */
void HTTP_ClearAuthorization (
		HTTPAuthContext* ctx, /** the authentication database */
		HTTP_CHAR *host,      /** the host name */
		HTTP_UINT16 port,     /** the server port number */
		HTTP_CHAR *path       /** the path on the server */
	)
{
	HTTPAuthenticationRealm* realm;

	HTTP_CLIENT_AUTH_API_ENTER(ctx);

	realm = _HTTP_GetAuthenticationRealmByPath(ctx, host, port, path);
	if (realm)
	{
		HTTPAuthenticationHost * host = realm->host;

		_HTTP_HostRemoveRealm(host, realm);
		_HTTP_DeleteRealm(realm);

		if (host->node.next == &host->node)
		{
			_HTTP_RemoveAuthenticationHost(host);
			_HTTP_DeleteHost(host);
		}
	}

	HTTP_CLIENT_AUTH_API_EXIT(ctx);
}

/*---------------------------------------------------------------------------
  HTTP_AuthSchemeToStr
  ---------------------------------------------------------------------------*/
/** @memo Get the name of an authentication scheme.

    @return string, the name of the scheme if recognized, 0 otherwise
 */
const HTTP_CHAR *HTTP_AuthSchemeToStr (
		HTTPAuthenticationScheme scheme
	)
{
	int n = EBSCLIP((int) scheme, 0, 2);
	return (schemeNames[n]);
}

/*---------------------------------------------------------------------------*/
HTTPAuthStatus _HTTP_AuthProcessChallenge (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		HTTP_UINT32 authTimestamp,
		const HTTP_CHAR* challenge,
		HTTP_CHAR** realmName
	)
{
	HTTPAuthStatus status = HTTP_AUTH_STATUS_ERROR;
	HTTPAuthenticationScheme scheme = HTTP_AUTHENTICATION_UNKNOWN;
	HTTP_CHAR name[10], param[64];

	/* The format of WWW-Authenticate is:
	  WWW-Authenticate: scheme (param)*\r\n
	
	  The schemes we recognize are "Basic" and "Digest
	*/

	HTTP_CLIENT_AUTH_API_ENTER(ctx);

	// parse the scheme
	while (IS_WHITESPACE(challenge[0]))
	{
		challenge++;
	}

	if (!tc_strncmp(challenge, "Basic", 5))
	{
		/* parse Basic authentication paramaters */
		scheme = HTTP_AUTHENTICATION_BASIC;
		challenge += 5;
	}
	else if (!tc_strncmp(challenge, "Digest", 6))
	{
		/* parse Basic authentication paramaters */
		scheme = HTTP_AUTHENTICATION_DIGEST;
		challenge += 6;
	}

	if (scheme != HTTP_AUTHENTICATION_UNKNOWN)
	{
		HTTP_INT32 paramLen = 0;
		HTTP_CHAR* realmParam;

		realmParam = tc_stristr(challenge, "realm");

		/* first we must find and parse the realm parameter */
		if (realmParam)
		{
			if (HTTP_ParseNameValuePair(realmParam, name, param, 10, 64) >= 0)
			{
				HTTPAuthenticationRealm *realm = _HTTP_GetAuthenticationRealm(ctx, host, port, param);

				if (!realm)
				{
					realm = _HTTP_GetAuthenticationRealmByPath(ctx, host, port, path);
					if (realm)
					{
						_HTTP_RealmSetName(realm, param);
					}
				}

				if (realm)
				{
					status = HTTP_AUTH_STATUS_EXISTING_REJECTED;

					_HTTP_RealmUpdatePath(realm, path);

					if (scheme != realm->scheme)
					{
						realm->scheme = scheme;
						realm->timestamp = ctx->currentTimestamp;
						ctx->currentTimestamp++;
						status = HTTP_AUTH_STATUS_RETRY_REQUEST;
					}
					else
					{
						if (HTTP_AUTH_TIMESTAMP_COMPARE(authTimestamp, realm->timestamp) < 0 &&
						    !realm->rejected)
						{
							/* if the request that has been rejected was sent before the authorization
							   was updated (and this authorization has not yet been rejected), then
							   try again */
							status = HTTP_AUTH_STATUS_RETRY_REQUEST;
						}
						else
						{
							realm->rejected = 1;
						}
					}
				}
				else
				{
					status = HTTP_AUTH_STATUS_NONE_EXISTS;

					realm = _HTTP_CreateAuthenticationRealm (
							ctx,
							host,
							port,
							path,
							param,
							300, /* set this one to live for 5 minutes (300 seconds) */
							scheme
						);
				}

				if (realmName)
				{
					*realmName = rtp_strdup(param);
				}

				if (realm && scheme != HTTP_AUTHENTICATION_BASIC)
				{
					/* now we know the realm; parse all remaining parameters
					   (if necessary according to the scheme being used */

					if (scheme == HTTP_AUTHENTICATION_DIGEST)
					{
						realm->param.digest.qop = HTTP_QOP_NONE;
					}

					while (1)
					{
						paramLen = HTTP_ParseNameValuePair(challenge, name, param, 10, 64);

						if (paramLen <= 0)
						{
							break;
						}

						/* process parameters according to scheme */

						switch (scheme)
						{
							case HTTP_AUTHENTICATION_DIGEST:
								if (!tc_stricmp(name, "domain"))
								{
									/* process domain URI */
									/* tbd - add realms for each host specified here */
								}
								else if (!tc_stricmp(name, "nonce"))
								{
									/* set the server-assigned nonce */
									if (realm->param.digest.serverNonce[0] == 0)
									{
										status = HTTP_AUTH_STATUS_RETRY_REQUEST;
									}

									tc_strncpy(realm->param.digest.serverNonce, param, HTTP_CFG_MAX_NONCE_LEN-1);
									realm->param.digest.serverNonce[HTTP_CFG_MAX_NONCE_LEN-1] = 0;
								}
								else if (!tc_stricmp(name, "opaque"))
								{
									/* store the server's opaque value */
									tc_strncpy(realm->param.digest.serverOpaque, param, HTTP_CFG_MAX_OPAQUE_LEN-1);
									realm->param.digest.serverOpaque[HTTP_CFG_MAX_OPAQUE_LEN-1] = 0;
								}
								else if (!tc_stricmp(name, "stale"))
								{
									/* perform stale processing on this realm */
									if (!tc_stricmp(param, "TRUE"))
									{
										status = HTTP_AUTH_STATUS_RETRY_REQUEST;
									}
								}
								else if (!tc_stricmp(name, "algorithm"))
								{
									/* set the algorithm to use for this session */
									/* tbd - we will only support MD5 algorithm for the initial release */
								}
								else if (!tc_stricmp(name, "qop"))
								{
									/* set quality of protection options */

									int n;

									realm->param.digest.qop = HTTP_QOP_UNRECOGNIZED;

									for (n=0; n < HTTP_NUM_QOP_TYPES; n++)
									{
										if (!tc_stricmp(param, httpAuthQoPNames[n]))
										{
											realm->param.digest.qop = n;
										}
									}
								}
								break;
						}

						challenge += paramLen;

						while (challenge[0] == ',' || challenge[0] == ' ')
						{
							challenge++;
						}
					}
				}
			}
		}
	}

	ctx->currentTimestamp++;

	HTTP_CLIENT_AUTH_API_EXIT(ctx);

	return (status);
}

/*---------------------------------------------------------------------------*/
HTTPAuthenticationRealm * _HTTP_CreateAuthenticationRealm (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		const HTTP_CHAR* realmName,
		HTTP_INT32 ttlSec,
		HTTPAuthenticationScheme scheme
	)
{
	unsigned hostCreated = 0;
	HTTPAuthenticationRealm *realm = 0;
	HTTPAuthenticationHost *host = _HTTP_GetAuthenticationHost(ctx, hostName, port);

	if (!host)
	{
		host = _HTTP_NewHost(hostName, port);
		if (!host)
		{
			return (0);
		}
		hostCreated = 1;
	}

	realm = _HTTP_NewRealm(realmName, path, ttlSec, scheme);
	if (!realm)
	{
		if (hostCreated)
		{
			_HTTP_DeleteHost(host);
		}
		return (0);
	}

	_HTTP_AddAuthenticationHost(ctx, host);
	_HTTP_HostAddRealm(host, realm);

	return (realm);
}


/*---------------------------------------------------------------------------*/
HTTPAuthenticationRealm* _HTTP_GetAuthenticationRealm (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		HTTP_CHAR* realmName
	)
{
	HTTPAuthenticationRealm* realm = 0;
	HTTPAuthenticationHost* host = _HTTP_GetAuthenticationHost(ctx, hostName, port);

	if (host)
	{
		realm = (HTTPAuthenticationRealm*) host->realmList.next;
		while (realm != (HTTPAuthenticationRealm*) &host->realmList)
		{
			if (!tc_strcmp(realm->realmName, realmName))
			{
				break;
			}
			realm = (HTTPAuthenticationRealm*) realm->node.next;
		}

		if (realm == (HTTPAuthenticationRealm*) &host->realmList)
		{
			realm = 0;
		}
	}

	return (realm);
}

/*---------------------------------------------------------------------------*/
HTTPAuthenticationRealm * _HTTP_GetAuthenticationRealmByPath (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port,
		const HTTP_CHAR* path
	)
{
	HTTPAuthenticationRealm * realm = 0;
	HTTPAuthenticationHost * host = _HTTP_GetAuthenticationHost(ctx, hostName, port);
	HTTP_INT32 pathLen, realmPathLen;

	if (host)
	{
		pathLen = tc_strlen(path);
		realm = (HTTPAuthenticationRealm*) host->realmList.next;
		while (realm != (HTTPAuthenticationRealm*) &host->realmList)
		{
			realmPathLen = tc_strlen(realm->path);
			if (realmPathLen > 0 && realmPathLen <= pathLen)
			{
				if (!tc_strncmp(realm->path, path, realmPathLen))
				{
					break;
				}
			}
			realm = (HTTPAuthenticationRealm*) realm->node.next;
		}

		if (realm == (HTTPAuthenticationRealm*) &host->realmList)
		{
			realm = 0;
		}
	}

	return (realm);
}

/*---------------------------------------------------------------------------*/
HTTPAuthenticationHost * _HTTP_GetAuthenticationHost (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port
	)
{
	HTTPAuthenticationHost* host = (HTTPAuthenticationHost*) ctx->hostList.next;

	while (host != (HTTPAuthenticationHost*) &ctx->hostList)
	{
		if (!tc_stricmp(hostName, host->hostName) && port == host->port)
		{
			return (host);
		}

		host = (HTTPAuthenticationHost*) host->node.next;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _HTTP_AddAuthenticationHost (
		HTTPAuthContext* ctx,
		HTTPAuthenticationHost *host
	)
{
	/* use a most-recently-created storage heuristic */
	DLLIST_INSERT_AFTER(&ctx->hostList, &host->node);
}

/*---------------------------------------------------------------------------*/
void _HTTP_RemoveAuthenticationHost (
		HTTPAuthenticationHost *host
	)
{
	DLLIST_REMOVE(&host->node);
}

/*---------------------------------------------------------------------------*/
/* Authentication Host management functions */

/*---------------------------------------------------------------------------*/
HTTPAuthenticationHost * _HTTP_NewHost (
		const HTTP_CHAR* hostName,
		HTTP_UINT16 port
	)
{
	HTTPAuthenticationHost * host = (HTTPAuthenticationHost *) rtp_malloc(sizeof(HTTPAuthenticationHost));

	if (host)
	{
		tc_memset(host, 0, sizeof(HTTPAuthenticationHost));
		tc_strncpy(host->hostName, hostName, HTTP_CFG_MAX_HOSTNAME_LEN-1);
		host->hostName[HTTP_CFG_MAX_HOSTNAME_LEN-1] = 0;
		host->port = port;
		DLLIST_INIT(&host->realmList);
	}

	return (host);
}

/*---------------------------------------------------------------------------*/
void _HTTP_DeleteHost (
		HTTPAuthenticationHost *host
	)
{
	rtp_free(host);
}

/*---------------------------------------------------------------------------*/
void _HTTP_HostAddRealm (
		HTTPAuthenticationHost *host,
		HTTPAuthenticationRealm *realm
	)
{
	/* use a most-recently-created storage heuristic */
	DLLIST_INSERT_AFTER(&host->realmList, &realm->node);
	realm->host = host;
}

/*---------------------------------------------------------------------------*/
void _HTTP_HostRemoveRealm (
		HTTPAuthenticationHost *host,
		HTTPAuthenticationRealm *realm
	)
{
	DLLIST_REMOVE(&realm->node);
	realm->host = 0;
}

/*---------------------------------------------------------------------------*/
/* Authentication Realm management functions */

/*---------------------------------------------------------------------------*/
HTTPAuthenticationRealm * _HTTP_NewRealm (
		const HTTP_CHAR* realmName,
		const HTTP_CHAR* path,
		HTTP_INT32 ttlSec,
		HTTPAuthenticationScheme scheme
	)
{
	HTTPAuthenticationRealm * realm = (HTTPAuthenticationRealm *) rtp_malloc(sizeof(HTTPAuthenticationRealm));

	if (realm)
	{
		tc_memset(realm, 0, sizeof(HTTPAuthenticationRealm));

		if (realmName)
		{
			tc_strncpy(realm->realmName, realmName, HTTP_CFG_MAX_REALMNAME_LEN-1);
			realm->realmName[HTTP_CFG_MAX_REALMNAME_LEN-1] = 0;
		}

		if (path)
		{
			tc_strncpy(realm->path, path, HTTP_CFG_MAX_PATH_LEN-1);
			realm->path[HTTP_CFG_MAX_PATH_LEN-1] = 0;
		}
		else
		{
			realm->path[0] = 0;
		}

		realm->scheme = scheme;
	}

	return (realm);
}

/*---------------------------------------------------------------------------*/
void _HTTP_DeleteRealm (
		HTTPAuthenticationRealm *realm
	)
{
	rtp_free(realm);
}

/*---------------------------------------------------------------------------*/
void _HTTP_RealmSetTtl (
		HTTPAuthenticationRealm *realm,
		HTTP_INT32 ttlSec
	)
{
	if (ttlSec == RTP_TIMEOUT_INFINITE)
	{
		ttlSec = 0;
	}

	if (!ttlSec)
	{
		rtp_date_set_time_forever(&realm->expires);
	}
	else
	{
		rtp_date_get_timestamp(&realm->expires);
		rtp_date_add_seconds(&realm->expires, ttlSec);
	}
	realm->ttlSec = ttlSec;
}

/*---------------------------------------------------------------------------*/
void _HTTP_RealmUpdatePath (
		HTTPAuthenticationRealm *realm,
		const HTTP_CHAR* path
	)
{
	HTTP_INT32 n;

	if (!realm->path[0])
	{
		tc_strncpy(realm->path, path, HTTP_CFG_MAX_PATH_LEN-1);
		realm->path[HTTP_CFG_MAX_PATH_LEN-1] = 0;
		return;
	}

	/* Find the longest prefix that both paths share and set that as the new path */

	for (n=0; realm->path[n] == path[n]; n++)
	{
		if (!path[n] || !realm->path[n])
		{
			break;
		}
	}

	realm->path[n] = 0;
}

/*---------------------------------------------------------------------------*/
int _HTTP_RealmExpired (
		HTTPAuthenticationRealm *realm
	)
{
	RTP_TIMESTAMP current;

	rtp_date_get_timestamp(&current);
	return (rtp_date_compare_time(&realm->expires, &current) < 0);
}

/*---------------------------------------------------------------------------*/
void _HTTP_RealmSetName (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* name
	)
{
	tc_strncpy(realm->realmName, name, HTTP_CFG_MAX_REALMNAME_LEN-1);
	realm->realmName[HTTP_CFG_MAX_REALMNAME_LEN-1] = 0;
}

/*---------------------------------------------------------------------------*/
void _HTTP_RealmSetUser (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* user
	)
{
	tc_strncpy(realm->userName, user, HTTP_CFG_MAX_USERNAME_LEN-1);
	realm->userName[HTTP_CFG_MAX_USERNAME_LEN-1] = 0;
}

/*---------------------------------------------------------------------------*/
void _HTTP_RealmSetPassword (
		HTTPAuthenticationRealm *realm,
		HTTP_CHAR* password
	)
{
	tc_strncpy(realm->password, password, HTTP_CFG_MAX_PASSWORD_LEN-1);
	realm->password[HTTP_CFG_MAX_PASSWORD_LEN-1] = 0;
}

/*---------------------------------------------------------------------------*/
void _HTTP_CheckRealmExpirations (
		HTTPAuthContext* ctx
	)
{
	HTTPAuthenticationRealm* realm;
	HTTPAuthenticationRealm* nextRealm;
	HTTPAuthenticationHost* host = (HTTPAuthenticationHost*) ctx->hostList.next;
	HTTPAuthenticationHost* nextHost;

	while (host != (HTTPAuthenticationHost*) &ctx->hostList)
	{
		realm = (HTTPAuthenticationRealm*) host->realmList.next;
		while (realm != (HTTPAuthenticationRealm*) &host->realmList)
		{
			nextRealm = (HTTPAuthenticationRealm*) realm->node.next;
			if (_HTTP_RealmExpired(realm))
			{
				_HTTP_HostRemoveRealm(host, realm);
				_HTTP_DeleteRealm(realm);
			}
			realm = nextRealm;
		}

		nextHost = (HTTPAuthenticationHost*) host->node.next;
		if (host->realmList.next == &host->realmList)
		{
			_HTTP_RemoveAuthenticationHost(host);
			_HTTP_DeleteHost(host);
		}
		host = nextHost;
	}
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_PrintAuthorization (
		HTTPAuthContext* ctx,
		const HTTP_CHAR* method,
		const HTTP_CHAR* host,
		HTTP_UINT16 port,
		const HTTP_CHAR* path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream,
		HTTP_UINT32* authTimestamp
	)
{
	HTTPAuthenticationRealm *realm;

	HTTP_CLIENT_AUTH_API_ENTER(ctx);

	_HTTP_CheckRealmExpirations(ctx);

	if (authTimestamp)
	{
		*authTimestamp = ctx->currentTimestamp;
	}

	ctx->currentTimestamp++;

	realm = _HTTP_GetAuthenticationRealmByPath(ctx, host, port, path);
	if (realm)
	{
		HTTP_INT32 len = _HTTP_RealmPrintAuthorization(realm, method, path, writeFn, requestStream);

		HTTP_CLIENT_AUTH_API_EXIT(ctx);
		return (len);
	}

	HTTP_CLIENT_AUTH_API_EXIT(ctx);
	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_RealmPrintAuthorization (
		HTTPAuthenticationRealm *realm,
		const HTTP_CHAR* method,
		const HTTP_CHAR* path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	)
{
	switch (realm->scheme)
	{
		case HTTP_AUTHENTICATION_BASIC:
			if (realm->userName[0])
			{
				HTTP_INT32 len = 0;
				char auth[HTTP_CFG_MAX_USERNAME_LEN + HTTP_CFG_MAX_PASSWORD_LEN];
				char encoded[((HTTP_CFG_MAX_USERNAME_LEN + HTTP_CFG_MAX_PASSWORD_LEN) * 4 + 2) /3];

				len =  sizeof(HTTP_CLIENT_BASIC_STR) / sizeof(HTTP_CHAR) - sizeof(HTTP_CHAR);
				writeFn(requestStream, HTTP_CLIENT_BASIC_STR, len);
				writeFn(requestStream, " ", 1);

				tc_strcpy(auth, realm->userName);
				tc_strcat(auth, ":");
				tc_strcat(auth, realm->password);

				len = _HTTP_Base64Encode(encoded, (HTTP_UINT8*) auth, (HTTP_INT32) tc_strlen(auth));

				writeFn(requestStream, encoded, len);

				return (len);
			}
			break;

		case HTTP_AUTHENTICATION_DIGEST:
			if (realm->userName[0])
			{
				HTTP_INT32 len = 0;
				HTTP_MD5_CTX md5Ctx;
				HTTP_CHAR hashA1[32];
				HTTP_CHAR hashA2[32];
				HTTP_CHAR finalHash[33];

				/* according to RFC 2617,
				      credentials = "Digest" digest-response

				   where
				      digest-response = "username=\"" + <user-name> +
					                    "\", realm=\"" + <realm-name> +
										"\", nonce=\"" + <server-nonce> +
										"\", uri=\"" + <path> +
										"\", response=\"" +
										   (qop == auth || qop == auth-int)?
										      KD(H(A1), <nonce> + ":" + <nonce-count> + ":" + <cnonce> + ":" + <qop> + ":" + H(A2)) :
											  KD(H(A1), <nonce> + ":" + H(A2))

				      A1 = <user-name> + ":" + <realm-name> + ":" + <password>

					  A2 = <method (GET,POST,etc)> + ":" + <path>

					  H(X) = MD5(X), as a 32-character HEX string

					  KD(I,J) = H(I + ":" + J);
				*/

				len = 17 /*Digest username="*/ + tc_strlen(realm->userName) + 10 /*", realm="*/ +
				      tc_strlen(realm->realmName) + 10 /*", nonce="*/ +
					  tc_strlen(realm->param.digest.serverNonce) + 8 /*", uri="*/ +
					  tc_strlen(path) + 13 /*", response="*/ + 32 /* MD5 hash */ + 1;

				writeFn(requestStream, "Digest username=\"", 17);
				writeFn(requestStream, realm->userName, tc_strlen(realm->userName));
				writeFn(requestStream, "\", realm=\"", 10);
				writeFn(requestStream, realm->realmName, tc_strlen(realm->realmName));
				writeFn(requestStream, "\", nonce=\"", 10);
				writeFn(requestStream, realm->param.digest.serverNonce, tc_strlen(realm->param.digest.serverNonce));
				writeFn(requestStream, "\", uri=\"", 8);
				writeFn(requestStream, path, tc_strlen(path));
				writeFn(requestStream, "\", response=\"", 13);

				/*-----------------------------------------------------*/
				/* find H(A1)                                          */
				/* if (algorithm == MD5) */
				HTTP_MD5_Init(&md5Ctx);
				HTTP_MD5_Update(&md5Ctx, realm->userName, tc_strlen(realm->userName)); /* TBD!!! - what if HTTP_CHAR is not ASCII 8bit ? */
				HTTP_MD5_Update(&md5Ctx, ":", 1);
				HTTP_MD5_Update(&md5Ctx, realm->realmName, tc_strlen(realm->realmName));
				HTTP_MD5_Update(&md5Ctx, ":", 1);
				HTTP_MD5_Update(&md5Ctx, realm->password, tc_strlen(realm->password));
				HTTP_MD5_Final(&md5Ctx);

     			if (realm->param.digest.hashAlgorithm == HTTP_ALGORITHM_MD5_SESS)
				{
					HTTP_UINT8 HA1[16];

					tc_memcpy(HA1, md5Ctx.digest, 16);

					HTTP_MD5_Init(&md5Ctx);
					HTTP_MD5_Update(&md5Ctx, HA1, 16);
					HTTP_MD5_Update(&md5Ctx, ":", 1);
					HTTP_MD5_Update(&md5Ctx, realm->param.digest.serverNonce, tc_strlen(realm->param.digest.serverNonce));
					HTTP_MD5_Update(&md5Ctx, ":", 1);
					HTTP_MD5_Update(&md5Ctx, realm->param.digest.clientNonce, tc_strlen(realm->param.digest.clientNonce));
					HTTP_MD5_Final(&md5Ctx);
				}

				_HTTP_HashToHex(hashA1, md5Ctx.digest, 16);

				/*-----------------------------------------------------*/
				/* find H(A2)                                          */
				/* if (QOP == auth || QOP == none) */
				HTTP_MD5_Init(&md5Ctx);
				HTTP_MD5_Update(&md5Ctx, method, tc_strlen(method));
				HTTP_MD5_Update(&md5Ctx, ":", 1);
				HTTP_MD5_Update(&md5Ctx, path, tc_strlen(path));

				if (realm->param.digest.qop == HTTP_QOP_AUTH_INT)
				{
					HTTP_MD5_Update(&md5Ctx, ":", 1);
					/* What is HEntity!?!? */
					/*HTTP_MD5_Update(&md5Ctx, HEntity, HASHHEXLEN);*/
				}

				HTTP_MD5_Final(&md5Ctx);
				_HTTP_HashToHex(hashA2, md5Ctx.digest, 16);

				HTTP_MD5_Init(&md5Ctx);
				HTTP_MD5_Update(&md5Ctx, hashA1, 32);
				HTTP_MD5_Update(&md5Ctx, ":", 1);
				HTTP_MD5_Update(&md5Ctx, realm->param.digest.serverNonce, tc_strlen(realm->param.digest.serverNonce));
				HTTP_MD5_Update(&md5Ctx, ":", 1);

				if (realm->param.digest.qop == HTTP_QOP_AUTH ||
					realm->param.digest.qop == HTTP_QOP_AUTH_INT)
				{
					HTTP_CHAR nonceCountStr[8];

					_HTTP_UINT32ToHex(nonceCountStr, realm->param.digest.nonceCount);

					HTTP_MD5_Update(&md5Ctx, nonceCountStr, 8);
					HTTP_MD5_Update(&md5Ctx, ":", 1);
					HTTP_MD5_Update(&md5Ctx, realm->param.digest.clientNonce, tc_strlen(realm->param.digest.clientNonce));
					HTTP_MD5_Update(&md5Ctx, ":", 1);
					HTTP_MD5_Update(&md5Ctx, httpAuthQoPNames[realm->param.digest.qop], tc_strlen(httpAuthQoPNames[realm->param.digest.qop]));
					HTTP_MD5_Update(&md5Ctx, ":", 1);
				}

				HTTP_MD5_Update(&md5Ctx, hashA2, 32);
				HTTP_MD5_Final(&md5Ctx);

				_HTTP_HashToHex(finalHash, md5Ctx.digest, 16);
				finalHash[32] = '\"';

				writeFn(requestStream, finalHash, 33);

				return (len);
			}
			break;

		default:
			break;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
void _HTTP_HashToHex (
		HTTP_CHAR* str,
		HTTP_UINT8* hash,
		HTTP_INT32 len
	)
{
	while (len > 0)
	{
		str[0] = _hexDigit[((*hash & 0xf0) >> 4)];
		str[1] = _hexDigit[(*hash & 0x0f)];

		str += 2;
		hash++;
		len--;
	}
}

/*---------------------------------------------------------------------------*/
void _HTTP_UINT32ToHex (
		HTTP_CHAR* str,
		HTTP_UINT32 i
	)
{
	str[0] = _hexDigit[((i & 0xf0000000) >> 28)];
	str[1] = _hexDigit[((i & 0x0f000000) >> 24)];
	str[2] = _hexDigit[((i & 0x00f00000) >> 20)];
	str[3] = _hexDigit[((i & 0x000f0000) >> 16)];
	str[4] = _hexDigit[((i & 0x0000f000) >> 12)];
	str[5] = _hexDigit[((i & 0x00000f00) >> 8)];
	str[6] = _hexDigit[((i & 0x000000f0) >> 4)];
	str[7] = _hexDigit[((i & 0x0000000f))];
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_Base64Encode (
		HTTP_CHAR* dst,
		HTTP_UINT8* src,
		HTTP_INT32 size
	)
{
	HTTP_INT32 len = 0;

	while (size > 0)
	{
		switch (size)
		{
			case 1:
				/* encode 1 byte */
				if (dst)
				{
					dst[0] = _base64[((src[0] >> 2) & 0x3f)];
					dst[1] = _base64[((src[0] << 4) & 0x30)];
					dst[2] = '=';
					dst[3] = '=';
				}

				src++;
				size--;
				break;

			case 2:
				/* encode 2 bytes */
				if (dst)
				{
					dst[0] = _base64[((src[0] >> 2) & 0x3f)];
					dst[1] = _base64[((src[0] << 4) & 0x30) | ((src[1] >> 4) & 0x0f)];
					dst[2] = _base64[((src[1] << 2) & 0x3c)];
					dst[3] = '=';
				}

				src += 2;
				size -= 2;
				break;

			default:
				/* encode 3 bytes */
				if (dst)
				{
					dst[0] = _base64[((src[0] >> 2) & 0x3f)];
					dst[1] = _base64[((src[0] << 4) & 0x30) | ((src[1] >> 4) & 0x0f)];
					dst[2] = _base64[((src[1] << 2) & 0x3c) | ((src[2] >> 6) & 0x03)];
					dst[3] = _base64[(src[2] & 0x3f)];
				}

				src += 3;
				size -= 3;
				break;
		}

		len += 4;
		if (dst)
		{
			dst += 4;
		}
	}

	return (len);
}

/*---------------------------------------------------------------------------*/
int _HTTP_AuthContextInit (
		HTTPAuthContext* ctx
	)
{
	DLLIST_INIT(&ctx->hostList);

	ctx->currentTimestamp = 0;
	ctx->printAuthorization = _HTTP_PrintAuthorization;
	ctx->processChallenge = _HTTP_AuthProcessChallenge;

	return (0);
}

/*---------------------------------------------------------------------------*/
void _HTTP_ClearAllAuthorizations (
		HTTPAuthContext* ctx
	)
{
	HTTPAuthenticationRealm* realm;
	HTTPAuthenticationRealm* nextRealm;
	HTTPAuthenticationHost* host = (HTTPAuthenticationHost*) ctx->hostList.next;
	HTTPAuthenticationHost* nextHost;

	while (host != (HTTPAuthenticationHost*) &ctx->hostList)
	{
		realm = (HTTPAuthenticationRealm*) host->realmList.next;
		while (realm != (HTTPAuthenticationRealm*) &host->realmList)
		{
			nextRealm = (HTTPAuthenticationRealm*) realm->node.next;
			_HTTP_HostRemoveRealm(host, realm);
			_HTTP_DeleteRealm(realm);
			realm = nextRealm;
		}

		nextHost = (HTTPAuthenticationHost*) host->node.next;
		_HTTP_RemoveAuthenticationHost(host);
		_HTTP_DeleteHost(host);
		host = nextHost;
	}
}
