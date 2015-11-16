//
// HTTPAUTH.H -
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

#ifndef __HTTPAUTH_H__
#define __HTTPAUTH_H__

/* define this symbol to support thread-safe operation */
/*#define HTTP_CLIENT_AUTH_THREADSAFE*/

typedef struct s_HTTPAuthContext    HTTPAuthContext;

#include "dllist.h"
#include "httpp.h"

#ifdef HTTP_CLIENT_AUTH_THREADSAFE
#include "rtpsignl.h"
#endif

enum e_HTTPAuthStatus
{
	HTTP_AUTH_STATUS_ERROR = -1,
	HTTP_AUTH_STATUS_OK = 0,
	HTTP_AUTH_STATUS_NONE_EXISTS,
	HTTP_AUTH_STATUS_EXISTING_REJECTED,
	HTTP_AUTH_STATUS_RETRY_REQUEST
};
typedef enum e_HTTPAuthStatus       HTTPAuthStatus;

struct s_HTTPAuthContext
{
	DLListNode hostList;

	HTTP_UINT32 currentTimestamp;

	HTTP_INT32 (*printAuthorization) (
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

	HTTPAuthStatus (*processChallenge) (
			HTTPAuthContext* ctx,
			const HTTP_CHAR* host,
			HTTP_UINT16 port,
			const HTTP_CHAR* path,
			HTTP_UINT32 authTimestamp,
			const HTTP_CHAR* challenge,
			HTTP_CHAR** realm
		);

#ifdef HTTP_CLIENT_AUTH_THREADSAFE
	RTP_MUTEX                    lock;
#endif
};

typedef enum e_HTTPAuthenticationScheme
{
	HTTP_AUTHENTICATION_UNSPECIFIED = -1,
	HTTP_AUTHENTICATION_UNKNOWN = 0,
	HTTP_AUTHENTICATION_BASIC,
	HTTP_AUTHENTICATION_DIGEST

} HTTPAuthenticationScheme;

#ifdef __cplusplus
extern "C" {
#endif

int HTTP_AuthContextInit (
		HTTPAuthContext* ctx
	);

#ifdef HTTP_CLIENT_AUTH_THREADSAFE
int HTTP_AuthContextInitMT (
		HTTPAuthContext* ctx,
		RTP_MUTEX lock
	);
#endif

void HTTP_AuthContextDestroy (
		HTTPAuthContext* ctx
	);

int HTTP_SetAuthorization (
		HTTPAuthContext* ctx,
		HTTP_CHAR* host,
		HTTP_UINT16 port,
		HTTP_CHAR* path,
		HTTP_CHAR* userName,
		HTTP_CHAR* password,
		HTTP_INT32 ttlSec
	);

void HTTP_ClearAllAuthorizations (
		HTTPAuthContext* ctx
	);

void HTTP_ClearAuthorization (
		HTTPAuthContext* ctx,
		HTTP_CHAR *host,
		HTTP_UINT16 port,
		HTTP_CHAR *path
	);

const HTTP_CHAR *HTTP_AuthSchemeToStr (
		HTTPAuthenticationScheme scheme
	);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPAUTH_H__ */
