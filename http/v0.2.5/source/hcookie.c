//
// HCOOKIE.C - 
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

#include "hcookie.h"
#include "rtpstdup.h"
#include "rtputil.h"
#include "rtpdutil.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTP_MAX_COOKIE_LEN                    384
#define HTTP_MAX_COOKIE_DEFAULT_DOMAIN_LEN     128
#define HTTP_MAX_COOKIE_DEFAULT_PATH_LEN       256
#define IS_WHITESPACE(C)                       ((C)==' ' ||  (C)=='\r' || \
                                                (C)=='\n' || (C)=='\t' || \
                                                (C)=='\f' || (C)=='\v')

#define HTTP_CLIENT_COOKIE_CFG_DEFAULT_TTL_SEC 300

#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
#define HTTP_CLIENT_COOKIE_API_ENTER(C)        rtp_sig_mutex_claim((C)->lock)
#define HTTP_CLIENT_COOKIE_API_EXIT(C)         rtp_sig_mutex_release((C)->lock)
#else
#define HTTP_CLIENT_COOKIE_API_ENTER(C)
#define HTTP_CLIENT_COOKIE_API_EXIT(C)
#endif

/*****************************************************************************/
// Types
/*****************************************************************************/

typedef HTTPCookieInfo*                   HTTPCookieIterator;
typedef struct s_HTTPAttribValuePairParse HTTPAttribValuePairParse;
typedef struct s_HTTPCookieParse          HTTPCookieParse;

struct s_HTTPAttribValuePairParse
{
	struct 
	{
		const HTTP_CHAR* str;
		HTTP_INT32       len;
	}
	name;

	struct 
	{
		const HTTP_CHAR* str;
		HTTP_INT32       len;
	}
	value;
};

struct s_HTTPCookieParse
{
	HTTPAttribValuePairParse cookie;

	struct 
	{
		const HTTP_CHAR* str;
		HTTP_INT32       len;
	}
	domain;

	struct 
	{
		const HTTP_CHAR* str;
		HTTP_INT32       len;
	}
	path;

	RTP_TIMESTAMP            expires;
};

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

long _HTTP_ParseSetCookie (
		HTTPCookieParse* cookieParse,
		const HTTP_CHAR* cookieStr
	);

long _HTTP_CookieParseWhitespace (
		const HTTP_CHAR* str
	);

long _HTTP_CookieParseAVPair (
		HTTPAttribValuePairParse* parse,
		const HTTP_CHAR* str
	);

HTTPCookieInfo* _HTTP_CreateCookie (
		HTTPCookieContext* ctx,
		HTTPCookieParse* parse
	);

int _HTTP_DeleteCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

void HTTP_CookieSetNameLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* name,
		HTTP_INT32 len
	);
	
void HTTP_CookieSetValueLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* value,
		HTTP_INT32 len
	);
	
void HTTP_CookieSetDomainLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* domain,
		HTTP_INT32 len
	);
	
void HTTP_CookieSetPathLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* path,
		HTTP_INT32 len
	);
	
int _HTTP_InsertCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

void _HTTP_RemoveCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

long _HTTP_SizeOfCookie (
		HTTPCookieInfo* cookie
	);
	
long _HTTP_CookieSpecificity (
		HTTPCookieInfo* cookie
	);
	
void _HTTP_CheckAllCookieExpirations (
		HTTPCookieContext* ctx
	);
	
int  _HTTP_CheckCookieExpiration (
		HTTPCookieInfo* cookie	
	);

void _HTTP_FreeCookieData (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

unsigned _HTTP_CookieMatch (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path
	);

HTTPCookieInfo* _HTTP_GetFirstCookie (
		HTTPCookieContext* ctx,
		HTTPCookieIterator* pCI
	);
	
HTTPCookieInfo* _HTTP_GetNextCookie (
		HTTPCookieContext* ctx,
		HTTPCookieIterator* pCI
	);
	
HTTPCookieInfo* _HTTP_GetFirstMatchingCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path, 
		HTTPCookieIterator* pCI
	);
	
HTTPCookieInfo *_HTTP_GetNextMatchingCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path, 
		HTTPCookieIterator* pCI
	);

void _HTTP_StoreString (
		HTTP_CHAR* str,
		long (*writeFn)(void*,HTTP_UINT8*,long), 
		void *handle
	);

HTTPCookieInfo* _HTTP_FindExactCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* name, 
		const HTTP_CHAR* domain, 
		const HTTP_CHAR* path
	);

unsigned _HTTP_CookieFreeBytes (
		HTTPCookieContext* ctx,
		HTTP_INT32 size
	);

HTTPCookieInfo* _HTTP_AllocCookie (
		HTTPCookieContext* ctx
	);

void _HTTP_FreeCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

int _HTTP_InitCookieContext (
		HTTPCookieContext* ctx,
		HTTP_INT32 maxCookieBytes
	);
	
/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/


/*---------------------------------------------------------------------------
  HTTP_InitCookieContext
  ---------------------------------------------------------------------------*/
/** @memo Initialize a cookie database context.

    @doc  Initilizes a HTTPCookieContext struct, which holds cookie 
          information for a HTTP client.  Old cookies will be discarded
          automatically to ensure that the total memory usage of the
          HTTPCookieContext never exceeds maxCookieBytes.

    @see  HTTP_InitCookieContextMT, HTTP_DestroyCookieContext

    @return 0 on success, negative on failure
 */
int HTTP_InitCookieContext (
		HTTPCookieContext* ctx,   /** uninitialized struct to hold cookie data */
		HTTP_INT32 maxCookieBytes /** size limit on cookie storage */
	)
{
#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
	return (-1);
#else
	return (_HTTP_InitCookieContext(ctx, maxCookieBytes));
#endif	
}

#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
/*---------------------------------------------------------------------------
  HTTP_InitCookieContextMT
  ---------------------------------------------------------------------------*/
/** @memo Initialize a thread-safe cookie context.

    @doc  Does the same as \Ref{HTTP_InitCookieContext}, but also enables
          threadsafe operation.  The HTTP client cookie module must be
          compiled with the preprocessor symbol HTTP_CLIENT_COOKIE_THREADSAFE
          defined (which enables thread-safe mode) to use this function.

    @see  HTTP_InitCookieContextMT, HTTP_DestroyCookieContext

    @return 0 on success, negative on failure
 */
int HTTP_InitCookieContextMT (
		HTTPCookieContext* ctx,    /** uninitialized struct to hold cookie data */
		HTTP_INT32 maxCookieBytes, /** size limit on cookie storage */
		RTP_MUTEX lock             /** a mutex handle created by rtp_sig_alloc_mutex;
		                               the mutex must be seperately destroyed after
		                               \Ref{HTTP_DestroyCookieContext} is called, and
		                               must not be used for any other purpose while the
		                               cookie context is valid. */
	)
{
	if (_HTTP_InitCookieContext(ctx, maxCookieBytes) >= 0)
	{
		ctx->lock = lock;
		return (0);
	}
	
	return (-1);
}
#endif
	
/*---------------------------------------------------------------------------
  HTTP_DestroyCookieContext
  ---------------------------------------------------------------------------*/
/** @memo Destroy a cookie context.

    @doc  Frees all memory associated with an HTTP client-side cookie database.          

    @see  HTTP_InitCookieContext, HTTP_InitCookieContextMT

    @return 0 on success, negative on failure
 */
void HTTP_DestroyCookieContext (
		HTTPCookieContext* ctx
	)
{
	HTTP_DeleteAllCookies(ctx);
}

/*---------------------------------------------------------------------------
  HTTP_SetCookieFilter
  ---------------------------------------------------------------------------*/
/** @memo Set a function for a given cookie context to filter incoming cookies.

    @doc  Provides a way to select which cookies are accepted and stored 
          locally by the HTTP client.  All incoming cookies will be given
          to the filter function; if it returns 0, the cookie is rejected and
          not stored; otherwise, the cookie is accepted and stored in the 
          local database.

    @see  HTTP_SetCookie

    @return 0 on success, negative on failure
 */
void HTTP_SetCookieFilter (
		HTTPCookieContext* ctx,         /** cookie context to set filter for */
		unsigned (*filterFn) (
				void* filterData,
				HTTPCookieContext* ctx,
				HTTPCookieInfo* cookie
			),                          /** filter function */
		void* filterData                /** opaque data passed to the filter function
		                                    to provide context for the filter */
	)
{
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);

	ctx->filterFn = filterFn;
	ctx->filterData = filterData;

	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
}

/*---------------------------------------------------------------------------
  HTTP_SetCookie
  ---------------------------------------------------------------------------*/
/** @memo Enter a cookie into the database.

    @doc  This function is normally called automatically by the HTTP client
          when a server response contains a "Set-Cookie" header option.  It
          will pass the cookie through the user-specified filter function, if
          it has been set.
          
          In case the cookie does not explicitly specify a DNS domain or 
          server path for which the cookie applies, the host name that the 
          original request was sent to, along with the path on that server that
          the request was for, should be passed into this function as 
          the defaultDomain and defaultPath parameters.

    @see  HTTP_SetCookieFilter

    @return 0 on success, negative on failure
 */
int HTTP_SetCookie (
		HTTPCookieContext* ctx,         /** the cookie database in which to store
		                                    the cookie if it is accepted */
		const HTTP_CHAR* cookieStr,     /** the contents of the HTTP "Set-Cookie" 
		                                    option header */
		const HTTP_CHAR* defaultDomain, /** the host name that the original request
		                                    was sent to */
		const HTTP_CHAR* defaultPath    /** the path requested on the server */
	)
{
	HTTPCookieInfo* cookie;
	HTTPCookieParse cookieParse;

	HTTP_CLIENT_COOKIE_API_ENTER(ctx);

	_HTTP_ParseSetCookie (&cookieParse, cookieStr);

	if (!cookieParse.domain.str || !cookieParse.domain.len)
	{
		cookieParse.domain.str = defaultDomain;
		cookieParse.domain.len = tc_strlen(defaultDomain);
	}

	if (!cookieParse.path.str || !cookieParse.path.len)
	{
		cookieParse.path.str = defaultPath;
		cookieParse.path.len = tc_strlen(defaultPath);
	}

	cookie = _HTTP_CreateCookie (ctx, &cookieParse);

	if (cookie)
	{
		/* add the cookie to the database only if there is no filter or
		   the filter accepts it */
		if (!ctx->filterFn || ctx->filterFn(ctx->filterData, ctx, cookie))
		{			
			if (_HTTP_InsertCookie(ctx, cookie) >= 0)
			{
				HTTP_CLIENT_COOKIE_API_EXIT(ctx);								
				return (0);							
			}
		}
			
		_HTTP_DeleteCookie(ctx, cookie);
	}

	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
	
	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_SetCookieUC
  ---------------------------------------------------------------------------*/
/** @memo Set a cookie in 16-bit Unicode format.

    @doc  Performs the same function as \Ref{HTTP_SetCookie}, except that the
          strings passed are 16-bit Unicode instead of 8-bit ASCII.

    @see  HTTP_SetCookie

    @return 0 on success, negative on failure
 */
int HTTP_SetCookieUC (
		HTTPCookieContext* ctx,           /** the cookie database in which to store
		                                      the cookie if it is accepted */
		const HTTP_UINT16* cookieStr,     /** the contents of the HTTP "Set-Cookie" 
		                                      option header */
		const HTTP_UINT16* defaultDomain, /** the host name that the original request
		                                      was sent to */
		const HTTP_UINT16* defaultPath    /** the path requested on the server */
	)
{
	HTTP_CHAR asciiCookie[HTTP_MAX_COOKIE_LEN];
	HTTP_CHAR asciiDomain[HTTP_MAX_COOKIE_DEFAULT_DOMAIN_LEN];
	HTTP_CHAR asciiPath[HTTP_MAX_COOKIE_DEFAULT_PATH_LEN];
	int n;
	
	for (n=0; n<HTTP_MAX_COOKIE_LEN-1; n++)
	{
		asciiCookie[n] = (HTTP_CHAR) cookieStr[n];
		if (!cookieStr[n])
		{
			break;
		}
	}
	asciiCookie[HTTP_MAX_COOKIE_LEN-1] = '\0';

	for (n=0; n<HTTP_MAX_COOKIE_DEFAULT_DOMAIN_LEN-1; n++)
	{
		asciiDomain[n] = (HTTP_CHAR) defaultDomain[n];
		if (!defaultDomain[n])
		{
			break;
		}
	}
	asciiDomain[HTTP_MAX_COOKIE_DEFAULT_DOMAIN_LEN-1] = '\0';

	for (n=0; n<HTTP_MAX_COOKIE_DEFAULT_PATH_LEN-1; n++)
	{
		asciiPath[n] = (HTTP_CHAR) defaultPath[n];
		if (!defaultPath[n])
		{
			break;
		}
	}
	asciiPath[HTTP_MAX_COOKIE_DEFAULT_PATH_LEN-1] = '\0';
	
	return (HTTP_SetCookie(ctx, asciiCookie, asciiDomain, asciiPath));
}

/*---------------------------------------------------------------------------
  HTTP_DeleteAllCookies
  ---------------------------------------------------------------------------*/
/** @memo Clears all cookies from the local database.

    @doc  Discards all existing cookies and associated data.

    @see  HTTP_DeleteCookie

    @return nothing
 */
void HTTP_DeleteAllCookies (
		HTTPCookieContext* ctx  /** the cookie database */
	)
{
	HTTPCookieIterator ci;
	HTTPCookieInfo* cookie;

	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
		
	cookie = _HTTP_GetFirstCookie(ctx, &ci);
	while (cookie)
	{		
		_HTTP_RemoveCookie(ctx, cookie);
		_HTTP_DeleteCookie(ctx, cookie);
		
		cookie = _HTTP_GetFirstCookie(ctx, &ci);
	}	
	
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
}
	
/*---------------------------------------------------------------------------
  HTTP_PrintMatchingCookies
  ---------------------------------------------------------------------------*/
/** @memo Write any cookies that match a given request to a stream.

    @doc  This function is normally used by the HTTP client to fill in the
          contents of the HTTP "Cookie:" option header.  

    @see  HTTP_SetCookie

    @return number of characters written on success, negative on failure
 */
long HTTP_PrintMatchingCookies (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	)
{
	HTTPCookieInfo* cookie = 0;
	HTTPCookieIterator ci;
	long len = 0;
	unsigned firstCookie = 1;

	HTTP_CLIENT_COOKIE_API_ENTER(ctx);

	// is there somewhere better to garbage collect expired cookies?
	_HTTP_CheckAllCookieExpirations(ctx);
	cookie = _HTTP_GetFirstMatchingCookie(ctx, host, path, &ci);

	while (cookie)
	{
		if (cookie->name)
		{
			if (!firstCookie)
			{
				len += writeFn(requestStream, "; ", 2);
			}

			len += writeFn(requestStream, cookie->name, tc_strlen(cookie->name));

			if (cookie->value)
			{
				len += writeFn(requestStream, "=", 1);
				len += writeFn(requestStream, cookie->value, tc_strlen(cookie->value));
			}
	
			firstCookie = 0;
		}

		cookie = _HTTP_GetNextMatchingCookie(ctx, host, path, &ci);
	}

	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
	
	return (len);
}

	
/*---------------------------------------------------------------------------
  HTTP_StoreCookies
  ---------------------------------------------------------------------------*/
/** @memo Not yet implemented

    @doc  

    @see  

    @return 0 on success, negative on failure
 */
int HTTP_StoreCookies (
		HTTPCookieContext* ctx,
		long (*writeFn)(void*,HTTP_UINT8*,long), 
		void *handle
	)
{
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);

	return (-1);
}
	
/*---------------------------------------------------------------------------
  HTTP_RetrieveCookies
  ---------------------------------------------------------------------------*/
/** @memo Not yet implemented

    @doc  

    @see  

    @return 0 on success, negative on failure
 */
int HTTP_RetrieveCookies (
		HTTPCookieContext* ctx,
		long (*readFn)(void*,HTTP_UINT8*,long), 
		void *handle
	)
{
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);

	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_CreateCookie
  ---------------------------------------------------------------------------*/
/** @memo Create a new cookie for a cookie database.

    @doc  Creates a new cookie.  This cookie can be manipulated freely by
          \Ref{HTTP_CookieSetName}, \Ref{HTTP_CookieSetName}, etc., until
          it is inserted back into the database by \Ref{HTTP_ReleaseCookie}.
          
          The newly created cookie will not be sent to any servers until it
          is released using \Ref{HTTP_ReleaseCookie}.
          
          All strings passed into this function will be copied into buffers
          controlled by the cookie context.

    @see  HTTP_ReleaseCookie

    @return pointer to cookie if found, 0 otherwise
 */
HTTPCookieInfo* HTTP_CreateCookie (
		HTTPCookieContext* ctx,   /** the cookie database to create the cookie for */
		const HTTP_CHAR* name,    /** the cookie name */
		const HTTP_CHAR* value,   /** the cookie value */
		const HTTP_CHAR* domain,  /** the domain to send this cookie to */
		const HTTP_CHAR* path,    /** the path prefix of request targets to send this
		                              cookie to */
		RTP_TIMESTAMP* expires    /** the expiration time of the cookie */
	)
{
	HTTPCookieInfo* cookie;
	HTTPCookieParse parse;
	
	parse.cookie.name.str = name;
	parse.cookie.name.len = name? tc_strlen(name) : 0;
	parse.cookie.value.str = value;
	parse.cookie.value.len = value? tc_strlen(value) : 0;
	parse.domain.str = domain;
	parse.domain.len = domain? tc_strlen(domain) : 0;
	parse.path.str = path;
	parse.path.len = path? tc_strlen(path) : 0;
	parse.expires = *expires;

	HTTP_CLIENT_COOKIE_API_ENTER(ctx);

	cookie = _HTTP_CreateCookie (ctx, &parse);
	
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);

	return (cookie);
}

	
/*---------------------------------------------------------------------------
  HTTP_FindMatchingCookie
  ---------------------------------------------------------------------------*/
/** @memo Find a cookie in the database that matches the given host/path

    @doc  There may be a number of cookies that match a given host and path,
          so this function returns the nth such cookie.  This function actually
          temporarily removes the cookie from the database; so when the caller
          of this function is done manipulating the cookie, it must be released
          using either \Ref{HTTP_ReleaseCookie} (which will give it back to the
          database) or \Ref{HTTP_DeleteCookie} (which deletes it).  Until either
          of these functions are called, the cookie will not be sent along with
          any outgoing HTTP client requests.
          
    @see  HTTP_ReleaseCookie, HTTP_DeleteCookie, HTTP_FindExactCookie

    @return pointer to cookie if found, 0 otherwise
 */
HTTPCookieInfo* HTTP_FindMatchingCookie (
		HTTPCookieContext* ctx, /** the cookie database to search */
		const HTTP_CHAR* name,  /** option: the name of the cookie; 0 if unspecified */
		const HTTP_CHAR* host,  /** the host to match */
		const HTTP_CHAR* path,  /** the path to match */
		int num                 /** the index of the cookie */
	)
{
	HTTPCookieInfo* cookie;
	HTTPCookieIterator ci;
	
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
	
	cookie = _HTTP_GetFirstMatchingCookie(ctx, host, path, &ci);
	while (num > 0 && cookie)
	{
		if (!name || !tc_strcmp(name, cookie->name))
		{
			num--;
		}

		cookie = _HTTP_GetNextMatchingCookie(ctx, host, path, &ci);
	}
		
	if (cookie)
	{
		/* remove it from the context; so when we modify it it will
		   be safe; release cookie will put it back */
		_HTTP_RemoveCookie(ctx, cookie);
	}
	
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);

	return (cookie);
}
	
/*---------------------------------------------------------------------------
  HTTP_FindExactCookie
  ---------------------------------------------------------------------------*/
/** @memo Find a specific cookie in the database

    @doc  Locates a cookie that is an exact match for the given name, domain,
          and path.
    
          This function actually
          temporarily removes the cookie from the database; so when the caller
          of this function is done manipulating the cookie, it must be released
          using either \Ref{HTTP_ReleaseCookie} (which will give it back to the
          database) or \Ref{HTTP_DeleteCookie} (which deletes it).  Until either
          of these functions are called, the cookie will not be sent along with
          any outgoing HTTP client requests.

    @see  HTTP_ReleaseCookie, HTTP_DeleteCookie, HTTP_FindMatchingCookie

    @return pointer to cookie if found, 0 otherwise
 */
HTTPCookieInfo* HTTP_FindExactCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* name, 
		const HTTP_CHAR* domain, 
		const HTTP_CHAR* path
	)
{
	HTTPCookieInfo* cookie;
	
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
	
	cookie = _HTTP_FindExactCookie (ctx, name, domain, path);
	
	if (cookie)
	{
		/* remove it from the context; so when we modify it it will
		   be safe; release cookie will put it back */
		_HTTP_RemoveCookie(ctx, cookie);
	}
	
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);

	return (cookie);
}
		
/*---------------------------------------------------------------------------
  HTTP_CookieGetName
  ---------------------------------------------------------------------------*/
/** @memo Get the name of a cookie

    @doc  Warning: DO NOT modify the value returned by this function; to set
          a cookie's name, use \Ref{HTTP_CookieSetName}.

    @see  

    @return the name of the cookie
 */
const HTTP_CHAR* HTTP_CookieGetName (
		HTTPCookieInfo* cookie
	)
{
	return (cookie->name);
}

/*---------------------------------------------------------------------------
  HTTP_CookieGetValue
  ---------------------------------------------------------------------------*/
/** @memo Get the value of a cookie

    @doc  Warning: DO NOT modify the value returned by this function; to set
          a cookie's value, use \Ref{HTTP_CookieSetValue}.

    @see  

    @return the value of the cookie
 */
const HTTP_CHAR* HTTP_CookieGetValue (
		HTTPCookieInfo* cookie
	)
{
	return (cookie->value);
}
	
/*---------------------------------------------------------------------------
  HTTP_CookieGetDomain
  ---------------------------------------------------------------------------*/
/** @memo Get the domain of a cookie

    @doc  Warning: DO NOT modify the value returned by this function; to set
          a cookie's domain, use \Ref{HTTP_CookieSetDomain}.

    @see  

    @return the domain of the cookie
 */
const HTTP_CHAR* HTTP_CookieGetDomain (
		HTTPCookieInfo* cookie
	)
{
	return (cookie->domain);
}
	
/*---------------------------------------------------------------------------
  HTTP_CookieGetPath
  ---------------------------------------------------------------------------*/
/** @memo Get the path of a cookie

    @doc  Warning: DO NOT modify the value returned by this function; to set
          a cookie's path, use \Ref{HTTP_CookieSetPath}.

    @see  

    @return the path of the cookie
 */
const HTTP_CHAR *HTTP_CookieGetPath (
		HTTPCookieInfo* cookie
	)
{
	return (cookie->path);
}

/*---------------------------------------------------------------------------
  HTTP_CookieSetName
  ---------------------------------------------------------------------------*/
/** @memo Set the name of a cookie

    @doc  

    @see  

    @return nothing
 */
void HTTP_CookieSetName (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* name
	)
{
	if (name)
	{
		HTTP_CookieSetNameLen(cookie, name, tc_strlen(name));
	}
}
	
/*---------------------------------------------------------------------------
  HTTP_CookieSetValue
  ---------------------------------------------------------------------------*/
/** @memo Set the value of a cookie

    @doc  

    @see  

    @return nothing
 */
void HTTP_CookieSetValue (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* value
	)
{
	if (value)
	{
		HTTP_CookieSetValueLen(cookie, value, tc_strlen(value));
	}
}
	
/*---------------------------------------------------------------------------
  HTTP_CookieSetDomain
  ---------------------------------------------------------------------------*/
/** @memo Set the domain of a cookie

    @doc  

    @see  

    @return nothing
 */
void HTTP_CookieSetDomain (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* domain
	)
{
	if (domain)
	{
		HTTP_CookieSetDomainLen(cookie, domain, tc_strlen(domain));
	}
}
	
/*---------------------------------------------------------------------------
  HTTP_CookieSetPath
  ---------------------------------------------------------------------------*/
/** @memo Set the path of a cookie

    @doc  

    @see  

    @return nothing
 */
void HTTP_CookieSetPath (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* path
	)
{
	if (path)
	{
		HTTP_CookieSetPathLen(cookie, path, tc_strlen(path));
	}
}
	
/*---------------------------------------------------------------------------
  HTTP_StoreSingleCookie
  ---------------------------------------------------------------------------*/
/** @memo Not yet implemented

    @doc  

    @see  

    @return 0 on success, negative on failure
 */
int HTTP_StoreSingleCookie (
		HTTPCookieInfo* pCookie,
		long (*writeFn)(void*,HTTP_UINT8*,long), 
		void *handle
	)
{
	return (-1);
}
	
/*---------------------------------------------------------------------------
  HTTP_RetrieveSingleCookie
  ---------------------------------------------------------------------------*/
/** @memo Not yet implemented

    @doc  

    @see  

    @return 0 on success, negative on failure
 */
int HTTP_RetrieveSingleCookie (
		HTTPCookieInfo* pCookie,
		long (*readFn)(void*,HTTP_UINT8*,long), 
		void *handle
	)
{
	return (-1);
}

/*---------------------------------------------------------------------------
  HTTP_ReleaseCookie
  ---------------------------------------------------------------------------*/
/** @memo Give a cookie back to the cookie database.

    @doc  Inserts a cookie into the given cookie database.  The cookie is 
          created by \Ref{HTTP_CreateCookie} or obtained using \Ref{HTTP_FindMatchingCookie}
          or \Ref{HTTP_FindExactCookie}.

    @see  HTTP_CreateCookie, HTTP_FindMatchingCookie, HTTP_FindExactCookie

    @return 0 on success, negative on failure
 */
int HTTP_ReleaseCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	int result = 0;
	
	HTTP_CLIENT_COOKIE_API_ENTER(ctx);
	
	if (_HTTP_InsertCookie(ctx, cookie) < 0)
	{
		_HTTP_DeleteCookie(ctx, cookie);
		result = -1;
	}
	
	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
	
	return (result);
}

/*---------------------------------------------------------------------------
  HTTP_DeleteCookie
  ---------------------------------------------------------------------------*/
/** @memo Destroy a cookie.

    @doc  Permanently deletes a cookie.  The cookie is 
          created by \Ref{HTTP_CreateCookie} or obtained using \Ref{HTTP_FindMatchingCookie}
          or \Ref{HTTP_FindExactCookie}.

    @see  HTTP_ReleaseCookie

    @return 0 on success, negative on failure
 */
int HTTP_DeleteCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	int result;

	HTTP_CLIENT_COOKIE_API_ENTER(ctx);

	result = _HTTP_DeleteCookie (ctx, cookie);

	HTTP_CLIENT_COOKIE_API_EXIT(ctx);
	
	return (result);	
}

/*---------------------------------------------------------------------------*/
long _HTTP_ParseSetCookie (
		HTTPCookieParse* cookieParse,
		const HTTP_CHAR* cookieStr
	)
{
	HTTPAttribValuePairParse avParse;
	const HTTP_CHAR* origStr = cookieStr;	
	unsigned expiresSet = 0;
	
	tc_memset(cookieParse, 0, sizeof(HTTPCookieParse));
	
	/* parse cookie name and value */
	cookieStr += _HTTP_CookieParseAVPair (&cookieParse->cookie, cookieStr);
	
	/* parse cookie attributes */
	while (*cookieStr)
	{
		cookieStr += _HTTP_CookieParseAVPair (&avParse, cookieStr);
		
		if (!tc_strnicmp(avParse.name.str, "path", 4))
		{
			cookieParse->path.str = avParse.value.str;
			cookieParse->path.len = avParse.value.len;
		}
		else if (!tc_strnicmp(avParse.name.str, "domain", 5))
		{
			cookieParse->domain.str = avParse.value.str;
			cookieParse->domain.len = avParse.value.len;
		}
		else if (!tc_strnicmp(avParse.name.str, "expires", 7))
		{
			expiresSet = 1;
			rtp_date_parse_time (&cookieParse->expires, avParse.value.str);
		}
	}

	if (!expiresSet)
	{
		rtp_date_get_timestamp(&cookieParse->expires);
		rtp_date_add_seconds(&cookieParse->expires, HTTP_CLIENT_COOKIE_CFG_DEFAULT_TTL_SEC);
	}

	/* return number of HTTP_CHARacters processed */
	return (cookieStr - origStr);
}

/*---------------------------------------------------------------------------*/
long _HTTP_CookieParseWhitespace (
		const HTTP_CHAR* str
	)
{
	const HTTP_CHAR* origStr = str;
	
	/* skip leading whitespace */
	while (IS_WHITESPACE(*str))
	{
		str++;
	}

	return (str - origStr);		
}

/*---------------------------------------------------------------------------*/
long _HTTP_CookieParseAVPair (
		HTTPAttribValuePairParse* parse,
		const HTTP_CHAR* str
	)
{
	const HTTP_CHAR* origStr = str;
	
	/* skip leading whitespace */
	str += _HTTP_CookieParseWhitespace(str);
	
	if (*str)
	{
		/* parse the attribute name */
		parse->name.str = str;
		parse->name.len = 0;
		
		while (*str && *str != '=')
		{
			parse->name.len++;
			str++;
		}
		
		if (*str)
		{
			/* step past the '=' */
			str++;

			/* skip leading whitespace */
			str += _HTTP_CookieParseWhitespace(str);
			
			/* parse the value */
			parse->value.str = str;
			parse->value.len = 0;
			
			if (*str)
			{
				while (*str && *str != ';')
				{
					parse->value.len++;
					str++;
				}
				
				if (*str == ';')
				{
					str++;
				}
			}
		}
	}
	
	return (str - origStr);
}

/*---------------------------------------------------------------------------*/
long _HTTP_PrintSetCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie,
		HTTP_INT32 (*writeFn) (
				void* streamPtr,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* streamPtr	
	)
{
	long len = 0;
	HTTP_CHAR dateStr[RTP_MAX_DATE_STR_LEN];

	len += writeFn(streamPtr, cookie->name, tc_strlen(cookie->name));
	
	if (cookie->value)
	{
		len += writeFn(streamPtr, "=", 1);
		len += writeFn(streamPtr, cookie->value, tc_strlen(cookie->value));
	}
	
	if (cookie->domain)
	{
		len += writeFn(streamPtr, "; domain=", 9);
		len += writeFn(streamPtr, cookie->domain, tc_strlen(cookie->domain));
	}

	if (cookie->path)
	{
		len += writeFn(streamPtr, "; path=", 7);
		len += writeFn(streamPtr, cookie->path, tc_strlen(cookie->path));
	}

	rtp_date_print_time(dateStr, &cookie->expires, 0);
	len += writeFn(streamPtr, "; expires=", 10);
	len += writeFn(streamPtr, dateStr, tc_strlen(dateStr));
	
	return (len);
}

/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_CreateCookie (
		HTTPCookieContext* ctx,
		HTTPCookieParse* parse
	)
{
	HTTPCookieInfo* cookie;
	
	cookie = _HTTP_AllocCookie(ctx);

	if (cookie)
	{
		cookie->domain = 0;
		cookie->path   = 0;
		cookie->name   = 0;
		cookie->value  = 0;
		
		HTTP_CookieSetNameLen(cookie, parse->cookie.name.str, parse->cookie.name.len);
		HTTP_CookieSetValueLen(cookie, parse->cookie.value.str, parse->cookie.value.len);
		HTTP_CookieSetDomainLen(cookie, parse->domain.str, parse->domain.len);
		HTTP_CookieSetPathLen(cookie, parse->path.str, parse->path.len);

		cookie->expires = parse->expires;		
	}
	
	return (cookie);
}
	
/*---------------------------------------------------------------------------*/
void HTTP_CookieSetNameLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* name,
		HTTP_INT32 len
	)
{
	HTTP_CHAR* newName = rtp_malloc(sizeof(HTTP_CHAR) * (len + 1));
	
	if (newName)
	{
		tc_memcpy(newName, name, sizeof(HTTP_CHAR) * len);
		newName[len] = 0;
		
		if (cookie->name)
		{
			rtp_free(cookie->name);
		}
		cookie->name = newName;
	}
}
	
/*---------------------------------------------------------------------------*/
void HTTP_CookieSetValueLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* value,
		HTTP_INT32 len
	)
{
	HTTP_CHAR* newValue = rtp_malloc(sizeof(HTTP_CHAR) * (len + 1));
	
	if (newValue)
	{
		tc_memcpy(newValue, value, sizeof(HTTP_CHAR) * len);
		newValue[len] = 0;

		if (cookie->value)
		{
			rtp_free(cookie->value);
		}
		cookie->value = newValue;
	}
}
	
/*---------------------------------------------------------------------------*/
void HTTP_CookieSetDomainLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* domain,
		HTTP_INT32 len
	)
{
	HTTP_CHAR* newDomain = rtp_malloc(sizeof(HTTP_CHAR) * (len + 1));
	
	if (newDomain)
	{
		tc_memcpy(newDomain, domain, sizeof(HTTP_CHAR) * len);
		newDomain[len] = 0;

		if (cookie->domain)
		{
			rtp_free(cookie->domain);
		}
		cookie->domain = newDomain;
	}
}
	
/*---------------------------------------------------------------------------*/
void HTTP_CookieSetPathLen (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* path,
		HTTP_INT32 len
	)
{
	HTTP_CHAR* newPath = rtp_malloc(sizeof(HTTP_CHAR) * (len + 1));
	
	if (newPath)
	{
		tc_memcpy(newPath, path, sizeof(HTTP_CHAR) * len);
		newPath[len] = 0;

		if (cookie->path)
		{
			rtp_free(cookie->path);
		}
		cookie->path = newPath;
	}
}

/*---------------------------------------------------------------------------*/
long _HTTP_SizeOfCookie (
		HTTPCookieInfo* cookie
	)
{
	return ((tc_strlen(cookie->name) + 
	         tc_strlen(cookie->value) + 
	         tc_strlen(cookie->domain) + 
	         tc_strlen(cookie->path) + 4) * sizeof(HTTP_CHAR) + 
	        sizeof(HTTPCookieInfo));
}
	
/*---------------------------------------------------------------------------*/
long _HTTP_CookieSpecificity (
		HTTPCookieInfo* cookie
	)
{
	long specificity = (tc_strlen(cookie->path) << 15) + 
	                    tc_strlen(cookie->domain);

	return (specificity);
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_CookieFreeBytes (
		HTTPCookieContext* ctx,
		HTTP_INT32 size
	)
{
	HTTPCookieIterator ci;
	HTTPCookieInfo* cookie;
	HTTPCookieInfo* next;
	
	cookie = _HTTP_GetFirstCookie(ctx, &ci);
	while (cookie && size > 0)
	{
		next = _HTTP_GetNextCookie(ctx, &ci);

		/* remove cookie to free up space */
		_HTTP_RemoveCookie(ctx, cookie);
		_HTTP_DeleteCookie(ctx, cookie);
		
		cookie = next;
	}

	return (size <= 0);
}

/*---------------------------------------------------------------------------*/	
void _HTTP_CheckAllCookieExpirations (
		HTTPCookieContext* ctx
	)
{
	HTTPCookieIterator ci;
	HTTPCookieInfo* cookie;
	HTTPCookieInfo* next;
	
	cookie = _HTTP_GetFirstCookie(ctx, &ci);
	while (cookie)
	{
		next = _HTTP_GetNextCookie(ctx, &ci);
		if (_HTTP_CheckCookieExpiration(cookie) != 0)
		{
			// remove expired cookie
			_HTTP_RemoveCookie(ctx, cookie);
			_HTTP_DeleteCookie(ctx, cookie);
		}
		cookie = next;
	}
}
	
/*---------------------------------------------------------------------------*/
int  _HTTP_CheckCookieExpiration (
		HTTPCookieInfo* cookie	
	)
{
	// is this a valid cookie with a valid expiration
	if (cookie && cookie->expires.year != 0 && cookie->expires.second != 0)
	{
		RTP_TIMESTAMP now;
		if (rtp_date_get_timestamp(&now) == 0)
		{
			if (rtp_date_compare_time(&cookie->expires, &now) < 0)
			{
				// expired cookie
				return(1);
			}
		}
	}

    return(0);
}

/*---------------------------------------------------------------------------*/
void _HTTP_RemoveCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	ctx->usedCookieBytes -= _HTTP_SizeOfCookie(cookie);
	DLLIST_REMOVE(&cookie->node);
}

/*---------------------------------------------------------------------------*/
int _HTTP_InsertCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	HTTPCookieInfo* existingCookie;
	HTTP_INT32 cookieSize;
	
	if (cookie->name && cookie->value)
	{
		if (!cookie->path)
		{
			HTTP_CookieSetPath(cookie, "");
		}

		if (!cookie->domain)
		{
			HTTP_CookieSetDomain(cookie, "");
		}

		if (cookie->path && cookie->domain)
		{
			/* this cookie always replaces the old version */
			existingCookie = _HTTP_FindExactCookie(ctx, cookie->name, cookie->domain, cookie->path);
			if (existingCookie)
			{
				_HTTP_RemoveCookie(ctx, existingCookie);
				_HTTP_DeleteCookie(ctx, existingCookie);
			}
					
			/* don't add this cookie if it's already expired */
			if (!_HTTP_CheckCookieExpiration(cookie))
			{						
				cookieSize = _HTTP_SizeOfCookie(cookie);
				if (cookieSize <= ctx->maxCookieBytes)
				{						
					/* First free up some cookie memory to store this cookie */
					if (_HTTP_CookieFreeBytes(ctx, (ctx->usedCookieBytes + cookieSize) - ctx->maxCookieBytes))
					{
						/* This is a new cookie; add it to the list
							tbd - add in order of path specificity */
						DLLIST_INSERT_BEFORE(&ctx->cookieList, &cookie->node);
						ctx->usedCookieBytes += cookieSize;						

						return (0);
					}
				}
			}
		}
	}
	
	return (-1);
}

/*---------------------------------------------------------------------------*/
int _HTTP_DeleteCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	_HTTP_FreeCookieData(ctx, cookie);
	_HTTP_FreeCookie(ctx, cookie);
	return (0);	
}

/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_FindExactCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* name, 
		const HTTP_CHAR* domain, 
		const HTTP_CHAR* path
	)
{
	HTTPCookieInfo *cookie = 0;
	HTTPCookieIterator ci;
	
	cookie = _HTTP_GetFirstCookie(ctx, &ci);
	while (cookie)
	{
		if (!tc_stricmp(name, cookie->name) &&
		    !tc_stricmp(domain, cookie->domain) &&
		    !tc_stricmp(path, cookie->path))
	    {
	    	return (cookie);
	    }
		cookie = _HTTP_GetNextCookie(ctx, &ci);
	}
	
	return (0);
	
}

/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_AllocCookie (
		HTTPCookieContext* ctx
	)
{
	return ((HTTPCookieInfo*) rtp_malloc(sizeof(HTTPCookieInfo)));
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	rtp_free(cookie);
}

/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_GetFirstCookie (
		HTTPCookieContext* ctx,
		HTTPCookieIterator* ci
	)
{
	if (ctx->cookieList.next == &ctx->cookieList)
	{
		return (0);
	}
	
	*ci = (HTTPCookieInfo*) ctx->cookieList.next;
	
	return (*ci);
}
	
/*---------------------------------------------------------------------------*/
HTTPCookieInfo *_HTTP_GetNextCookie (
		HTTPCookieContext* ctx,
		HTTPCookieIterator* ci
	)
{
	if (*ci != 0)
	{
		*ci = (HTTPCookieInfo*) (*ci)->node.next;
		
		if (*ci == (HTTPCookieInfo*) &ctx->cookieList)
		{
			*ci = 0;
		}
	}
	
	return (*ci);
}

/*---------------------------------------------------------------------------*/
void _HTTP_FreeCookieData (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	)
{
	if (cookie->domain)
	{
		rtp_free(cookie->domain);
		cookie->domain = 0;
	}

	if (cookie->path)
	{
		rtp_free(cookie->path);
		cookie->path = 0;
	}

	if (cookie->name)
	{
		rtp_free(cookie->name);
		cookie->name = 0;
	}

	if (cookie->value)
	{
		rtp_free(cookie->value);
		cookie->value = 0;
	}
}

/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_GetFirstMatchingCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path, 
		HTTPCookieIterator* ci
	)
{
	HTTPCookieInfo* cookie = _HTTP_GetFirstCookie(ctx, ci);

	while (cookie && !_HTTP_CookieMatch(cookie, host, path))
	{
		cookie = _HTTP_GetNextCookie(ctx, ci);
	}
	
	return cookie;
}
	
/*---------------------------------------------------------------------------*/
HTTPCookieInfo* _HTTP_GetNextMatchingCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path, 
		HTTPCookieIterator* ci
	)
{
	HTTPCookieInfo* cookie = _HTTP_GetNextCookie(ctx, ci);

	while (cookie && !_HTTP_CookieMatch(cookie, host, path))
	{
		cookie = _HTTP_GetNextCookie(ctx, ci);
	}
	
	return cookie;
}

/*---------------------------------------------------------------------------*/
unsigned _HTTP_CookieMatch (
		HTTPCookieInfo* cookie, 
		const HTTP_CHAR* host, 
		const HTTP_CHAR* path
	)
{
	long domainLen, hostLen;
	HTTP_CHAR* domain;

	if (!cookie->domain)
	{
		return 0;
	}

	if (_HTTP_CheckCookieExpiration(cookie) == 1)
	{
		return 0;
	}

	domain = cookie->domain;
	
	domainLen = tc_strlen(domain);
	hostLen = tc_strlen(host);
	
	if (hostLen > domainLen)
	{
		host += (hostLen - domainLen);
	}
	else
	{
		domain += (domainLen - hostLen);
	}
	
	if (!tc_stricmp(host, domain))
	{
		if (!cookie->path)
		{
			return 1;
		}
		
		if (!tc_strnicmp(path, cookie->path, tc_strlen(cookie->path)))
		{
			return (1);
		}
	}

	return (0);	
}

/*---------------------------------------------------------------------------*/
int _HTTP_InitCookieContext (
		HTTPCookieContext* ctx,
		HTTP_INT32 maxCookieBytes
	)
{
	DLLIST_INIT(&ctx->cookieList);
	ctx->maxCookieBytes = maxCookieBytes;
	ctx->usedCookieBytes = 0;
	ctx->setCookieFn = HTTP_SetCookie;
	ctx->printCookiesFn = HTTP_PrintMatchingCookies;
	ctx->filterFn = 0;

	return (0);
}
