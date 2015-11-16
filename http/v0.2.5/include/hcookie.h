//
// HCOOKIE.H - 
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

#ifndef __HCOOKIE_H__
#define __HCOOKIE_H__

/* define this symbol to support thread-safe operation */
/*#define HTTP_CLIENT_COOKIE_THREADSAFE*/

typedef struct s_HTTPCookieContext HTTPCookieContext;
typedef struct s_HTTPCookieInfo    HTTPCookieInfo;

#include "httpp.h"
#include "dllist.h"

#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
#include "rtpsignl.h"
#endif

struct s_HTTPCookieContext
{
	void (*setCookieFn) (
			HTTPCookieContext* ctx,
			const HTTP_CHAR *cookie, 
			const HTTP_CHAR *defaultDomain, 
			const HTTP_CHAR *defaultPath
		);			
	
	long (*printCookiesFn) (
			HTTPCookieContext* ctx,
			const HTTP_CHAR *host, 
			const HTTP_CHAR *path,
			HTTP_INT32 (*writeFn) (
					void* requestStream,
					const HTTP_CHAR* data,
					HTTP_INT32 len
				),
			void* requestStream
		);
	
#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
	RTP_MUTEX     lock;       /* a mutex for making this context thread-safe */
#endif	
	HTTP_INT32    maxCookieBytes;
	HTTP_INT32    usedCookieBytes;
	DLListNode    cookieList;
	
	unsigned (*filterFn) (
			void* filterData,
			HTTPCookieContext* ctx,
			HTTPCookieInfo* cookie
		);
		
	void* filterData;
};

struct s_HTTPCookieInfo
{
	DLListNode    node;
	HTTP_CHAR*    domain;
	HTTP_CHAR*    path;
	HTTP_CHAR*    name;
	HTTP_CHAR*    value;
	RTP_TIMESTAMP expires;
};

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------*/
/* atomic operations                                */

int HTTP_InitCookieContext (
		HTTPCookieContext* ctx,
		HTTP_INT32 maxCookieBytes
	);

#ifdef HTTP_CLIENT_COOKIE_THREADSAFE
int HTTP_InitCookieContextMT (
		HTTPCookieContext* ctx,
		HTTP_INT32 maxCookieBytes,
		RTP_MUTEX lock
	);
#endif
	
void HTTP_DestroyCookieContext (
		HTTPCookieContext* ctx
	);

void HTTP_SetCookieFilter (
		HTTPCookieContext* ctx,
		unsigned (*filterFn) (
				void* filterData,
				HTTPCookieContext* ctx,
				HTTPCookieInfo* cookie
			),
		void* filterData
	);

int HTTP_SetCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR *cookie, 
		const HTTP_CHAR *defaultDomain, 
		const HTTP_CHAR *defaultPath
	);
	
int HTTP_SetCookieUC (
		HTTPCookieContext* ctx,
		const HTTP_UINT16 *cookie, 
		const HTTP_UINT16 *defaultDomain, 
		const HTTP_UINT16 *defaultPath
	);
	
void HTTP_DeleteAllCookies (
		HTTPCookieContext* ctx
	);
	
long HTTP_PrintMatchingCookies (
		HTTPCookieContext* ctx,
		const HTTP_CHAR *host, 
		const HTTP_CHAR *path,
		HTTP_INT32 (*writeFn) (
				void* requestStream,
				const HTTP_CHAR* data,
				HTTP_INT32 len
			),
		void* requestStream
	);

int HTTP_StoreCookies (
		HTTPCookieContext* ctx,
		long (*writeFn)(void*,HTTP_UINT8*,long), 
		void *handle
	);
	
int HTTP_RetrieveCookies (
		HTTPCookieContext* ctx,
		long (*readFn)(void*,HTTP_UINT8*,long), 
		void *handle
	);

/*--------------------------------------------------*/
/* functions that return cookies, having "locked" 
   them  */

HTTPCookieInfo* HTTP_CreateCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR* name, 
		const HTTP_CHAR* value, 
		const HTTP_CHAR* domain, 
		const HTTP_CHAR* path,
		RTP_TIMESTAMP* expires
	);
	
HTTPCookieInfo* HTTP_FindMatchingCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR *name, 
		const HTTP_CHAR *host, 
		const HTTP_CHAR *path, 
		int num
	);
	
HTTPCookieInfo* HTTP_FindExactCookie (
		HTTPCookieContext* ctx,
		const HTTP_CHAR *name, 
		const HTTP_CHAR *domain, 
		const HTTP_CHAR *path
	);

/*--------------------------------------------------*/
/* things you can do with a cookie once its been
   "locked" */
	
const HTTP_CHAR *HTTP_CookieGetName (
		HTTPCookieInfo *cookie
	);
	
const HTTP_CHAR *HTTP_CookieGetValue (
		HTTPCookieInfo *cookie
	);
	
const HTTP_CHAR *HTTP_CookieGetDomain (
		HTTPCookieInfo *cookie
	);
	
const HTTP_CHAR *HTTP_CookieGetPath (
		HTTPCookieInfo *cookie
	);
	
void HTTP_CookieSetName (
		HTTPCookieInfo *cookie, 
		const HTTP_CHAR *name
	);
	
void HTTP_CookieSetValue (
		HTTPCookieInfo *cookie, 
		const HTTP_CHAR *value
	);
	
void HTTP_CookieSetDomain (
		HTTPCookieInfo *cookie, 
		const HTTP_CHAR *domain
	);
	
void HTTP_CookieSetPath (
		HTTPCookieInfo *cookie, 
		const HTTP_CHAR *path
	);
	
int HTTP_StoreSingleCookie (
		HTTPCookieInfo *pCookie,
		long (*writeFn)(void*,HTTP_UINT8*,long), 
		void *handle
	);
	
int HTTP_RetrieveSingleCookie (
		HTTPCookieInfo *pCookie,
		long (*readFn)(void*,HTTP_UINT8*,long), 
		void *handle
	);

/*--------------------------------------------------*/
/* when you are done playing with a cookie, make 
   sure to either release it back to the context or
   delete it (either will "unlock" the cookie) */	
	
int HTTP_ReleaseCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

int HTTP_DeleteCookie (
		HTTPCookieContext* ctx,
		HTTPCookieInfo* cookie
	);

#ifdef __cplusplus
}
#endif

#endif /* __HCOOKIE_H__ */
