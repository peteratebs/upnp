//
// HTTPP.H - Structs, prototypes, and defines for HTTPP.C
//
// EBS - Common Library
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

#ifndef __HTTPP_H__
#define __HTTPP_H__

#include "dllist.h"
#include "rtpdutil.h"
#include "rtpmem.h"

#define HTTP_VERSION                       0023  /* 0.2.3 */
#define HTTP_MALLOC(X)                     rtp_malloc(X)
#define HTTP_FREE(X)                       rtp_free(X)

/* HTTP basic types */
typedef int             HTTP_BOOL;
typedef char            HTTP_CHAR;
typedef char            HTTP_INT8;
typedef short           HTTP_INT16;
typedef long            HTTP_INT32;
typedef unsigned char   HTTP_UINT8;
typedef unsigned short  HTTP_UINT16;
typedef unsigned long   HTTP_UINT32;

typedef HTTP_INT32 (*HTTPStreamReadFn) (void* streamPtr, HTTP_UINT8* buffer, HTTP_INT32 min, HTTP_INT32 max);
typedef HTTP_INT32 (*HTTPStreamWriteFn) (void* streamPtr, const HTTP_UINT8* buffer, HTTP_INT32 size);

typedef HTTP_INT32 (*WriteFn) (void* requestStream, const HTTP_CHAR* data, HTTP_INT32 len);
typedef HTTP_INT32 (*WriteString) (void* privateData, WriteFn writeFn, void* requestStream);

#define HTTP_TRUE      1
#define HTTP_FALSE     0
#define HTTP_NULL      0

/* possible return codes returned from the stream functions and
   the session read/write functions */
#define HTTP_ENONE        0
#define HTTP_EFATAL      -1
#define HTTP_EWOULDBLOCK -2
#define HTTP_EIOFAILURE  -3

typedef enum e_HTTPSessionState
{
	HTTP_STATE_ERROR = -1,
	HTTP_STATE_INITIALIZED = 0,
	HTTP_STATE_DONE,
	HTTP_STATE_READING_DATA,
	HTTP_STATE_READING_HEADERS,
	HTTP_STATE_REQUEST_SENT,
	HTTP_STATE_RESPONSE_SENT,
	HTTP_STATE_SENDING_REQUEST,
	HTTP_STATE_SENDING_RESPONSE
}
HTTPSessionState;

typedef enum e_HTTPMethodType
{
	HTTP_UNKNOWN_METHOD_TYPE = -1,
	HTTP_METHOD_CONNECT = 0,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_M_POST,
	HTTP_METHOD_M_SEARCH,
	HTTP_METHOD_NOTIFY,
	HTTP_METHOD_OPTIONS,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_SUBSCRIBE,
	HTTP_METHOD_TRACE,
	HTTP_METHOD_UNSUBSCRIBE,
	HTTP_NUM_METHOD_TYPES
}
HTTPMethodType;

typedef enum e_HTTPHeaderValueType
{
	HTTP_TYPE_INT,
	HTTP_TYPE_REQUEST_OPTION,
	HTTP_TYPE_STRING,
	HTTP_TYPE_STRING_OBJECT,
	HTTP_TYPE_TIME
}
HTTPHeaderValueType;

typedef enum e_HTTPHeaderType
{
	HTTP_UNKNOWN_HEADER_TYPE = -1,
	HTTP_HEADER_ACCEPT = 0,
	HTTP_HEADER_ACCEPT_CHARSET,
	HTTP_HEADER_ACCEPT_ENCODING,
	HTTP_HEADER_ACCEPT_LANGUAGE,
	HTTP_HEADER_ACCEPT_RANGES,
	HTTP_HEADER_AGE,
	HTTP_HEADER_ALLOW,
	HTTP_HEADER_AUTHORIZATION,
	HTTP_HEADER_CACHE_CONTROL,
	HTTP_HEADER_CALLBACK,
	HTTP_HEADER_CONNECTION,
	HTTP_HEADER_CONTENT_ENCODING,
	HTTP_HEADER_CONTENT_LANGUAGE,
	HTTP_HEADER_CONTENT_LENGTH,
	HTTP_HEADER_CONTENT_LOCATION,
	HTTP_HEADER_CONTENT_MD5,
	HTTP_HEADER_CONTENT_RANGE,
	HTTP_HEADER_CONTENT_TYPE,
	HTTP_HEADER_COOKIE,
	HTTP_HEADER_DATE,
	HTTP_HEADER_ETAG,
	HTTP_HEADER_EXPECT,
	HTTP_HEADER_EXPIRES,
	HTTP_HEADER_EXT,
	HTTP_HEADER_FROM,
	HTTP_HEADER_HOST,
	HTTP_HEADER_IF_MATCH,
	HTTP_HEADER_IF_MODIFIED_SINCE,
	HTTP_HEADER_IF_NONE_MATCH,
	HTTP_HEADER_IF_RANGE,
	HTTP_HEADER_IF_UNMODIFIED_SINCE,
	HTTP_HEADER_KEEP_ALIVE,
	HTTP_HEADER_LAST_MODIFIED,
	HTTP_HEADER_LOCATION,
	HTTP_HEADER_MAN,
	HTTP_HEADER_MAX_FORWARDS,
	HTTP_HEADER_MX,
	HTTP_HEADER_NT,
	HTTP_HEADER_NTS,
	HTTP_HEADER_PRAGMA,
	HTTP_HEADER_PROXY_AUTHENTICATE,
	HTTP_HEADER_PROXY_AUTHORIZATION,
	HTTP_HEADER_RANGE,
	HTTP_HEADER_REFERER,
	HTTP_HEADER_REFRESH,
	HTTP_HEADER_RETRY_AFTER,
	HTTP_HEADER_SERVER,
	HTTP_HEADER_SET_COOKIE,
	HTTP_HEADER_SID,
	HTTP_HEADER_SOAP_ACTION,
	HTTP_HEADER_ST,
	HTTP_HEADER_TE,
	HTTP_HEADER_TIMEOUT,
	HTTP_HEADER_TRAILER,
	HTTP_HEADER_TRANSFER_ENCODING,
	HTTP_HEADER_UPGRADE,
	HTTP_HEADER_USER_AGENT,
	HTTP_HEADER_USN,
	HTTP_HEADER_VARY,
	HTTP_HEADER_VIA,
	HTTP_HEADER_WARNING,
	HTTP_HEADER_WWW_AUTHENTICATE,
	HTTP_NUM_HEADER_TYPES
}
HTTPHeaderType;

#define HTTP_STATUS_100_CONTINUE                          100
#define HTTP_STATUS_101_SWITCHING_PROTOCOLS               101
#define HTTP_STATUS_200_OK                                200
#define HTTP_STATUS_201_CREATED                           201
#define HTTP_STATUS_202_ACCEPTED                          202
#define HTTP_STATUS_203_NON_AUTHORITATIVE_INFORMATION     203
#define HTTP_STATUS_204_NO_CONTENT                        204
#define HTTP_STATUS_205_RESET_CONTENT                     205
#define HTTP_STATUS_206_PARTIAL_CONTENT                   206
#define HTTP_STATUS_300_MULTIPLE_CHOICES                  300
#define HTTP_STATUS_301_MOVED_PERMANENTLY                 301
#define HTTP_STATUS_302_FOUND                             302
#define HTTP_STATUS_303_SEE_OTHER                         303
#define HTTP_STATUS_304_NOT_MODIFIED                      304
#define HTTP_STATUS_305_USE_PROXY                         305
#define HTTP_STATUS_307_TEMPORARY_REDIRECT                307
#define HTTP_STATUS_400_BAD_REQUEST                       400
#define HTTP_STATUS_401_UNAUTHORIZED                      401
#define HTTP_STATUS_402_PAYMENT_REQUIRED                  402
#define HTTP_STATUS_403_FORBIDDEN                         403
#define HTTP_STATUS_404_NOT_FOUND                         404
#define HTTP_STATUS_405_METHOD_NOT_ALLOWED                405
#define HTTP_STATUS_406_NOT_ACCEPTABLE                    406
#define HTTP_STATUS_407_PROXY_AUTHENTICATION_REQUIRED     407
#define HTTP_STATUS_408_REQUEST_TIME_OUT                  408
#define HTTP_STATUS_409_CONFLICT                          409
#define HTTP_STATUS_410_GONE                              410
#define HTTP_STATUS_411_LENGTH_REQUIRED                   411
#define HTTP_STATUS_412_PRECONDITION_FAILED               412
#define HTTP_STATUS_413_REQUEST_ENTITY_TOO_LARGE          413
#define HTTP_STATUS_414_REQUEST_URI_TOO_LARGE             414
#define HTTP_STATUS_415_UNSUPPORTED_MEDIA_TYPE            415
#define HTTP_STATUS_416_REQUESTED_RANGE_NOT_SATISFIABLE   416
#define HTTP_STATUS_417_EXPECTATION_FAILED                417
#define HTTP_STATUS_500_INTERNAL_SERVER_ERROR             500
#define HTTP_STATUS_501_NOT_IMPLEMENTED                   501
#define HTTP_STATUS_502_BAD_GATEWAY                       502
#define HTTP_STATUS_503_SERVICE_UNAVAILABLE               503
#define HTTP_STATUS_504_GATEWAY_TIME_OUT                  504
#define HTTP_STATUS_505_HTTP_VERSION_NOT_SUPPORTED        505

typedef struct s_HTTPSession
{
	HTTPSessionState   state;

	HTTPStreamReadFn   readFn;
	HTTPStreamWriteFn  writeFn;
	void*              streamPtr;

	HTTP_UINT8*        readBuffer;
	HTTP_INT32         readBufferSize;
	HTTP_INT32         readBufferBegin;
	HTTP_INT32         readBufferEnd;

	HTTP_UINT8*        writeBuffer;
  	HTTP_INT32         writeBufferSize;
  	HTTP_INT32         writeBufferContent;

	// required for HTTP 1.1
  	HTTP_INT32         chunkSize;
  	HTTP_INT32         bytesReadThisChunk;

  	HTTP_INT32         bytesReadThisFile;
  	HTTP_BOOL          chunkedEncoding;
  	HTTP_BOOL          fileSizeKnown;
  	HTTP_INT32         fileSize;
}
HTTPSession;

typedef struct s_HTTPResponse
{
	HTTPSession*       session;
	HTTP_INT16         httpStatus;
	HTTP_INT8          httpMajorVersion;
	HTTP_INT8          httpMinorVersion;
	const HTTP_CHAR*   httpMessage;
}
HTTPResponse;

typedef struct s_HTTPRequest
{
	HTTPSession*       session;
	HTTPMethodType     methodType;
	HTTP_INT8          httpMajorVersion;
	HTTP_INT8          httpMinorVersion;
	const HTTP_CHAR*   method;
	const HTTP_CHAR*   target;
}
HTTPRequest;

typedef struct s_HTTPStringObject
{
	WriteString writeString;
	void* privateData;
}
HTTPStringObject;

typedef struct s_HTTPHeaderValue
{
	HTTPHeaderValueType type;
 	union
 	{
 		int               i;
 		struct {
	 		const HTTP_CHAR* ptr;
 			HTTP_INT32       len;
 		} str;
 		HTTPStringObject  strObject;
 		RTP_TIMESTAMP     time;
	} data;
}
HTTPHeaderValue;

/* call-back to process header fields; return 1 to tell HTTP to
   ignore this header, -1 to indicate an error and stop processing
   immediately, and 0 to continue normally */
typedef int (*HTTPHeaderCallback)(
		void* userData,
		HTTPSession* session,
		HTTPHeaderType type,
		const HTTP_CHAR* name,
		const HTTP_CHAR* value);

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------ */
/* Session related */

int  HTTP_InitSession             (HTTPSession* session,
                                   HTTPStreamReadFn readFn,
                                   HTTPStreamWriteFn writeFn,
                                   void* streamPtr);

int  HTTP_ResetSession            (HTTPSession* session,
                                   HTTPStreamReadFn readFn,
                                   HTTPStreamWriteFn writeFn,
                                   void* streamPtr);

void HTTP_SessionSetState         (HTTPSession* session,
                                   HTTPSessionState state);

void HTTP_SessionRequireSizeKnown (HTTPSession* session);

void HTTP_FreeSession             (HTTPSession* session);

HTTP_INT32 HTTP_Read              (HTTPSession* session,
                                   HTTP_UINT8* buffer,
                                   HTTP_INT32 size);

HTTP_INT32 HTTP_Write             (HTTPSession* session,
                                   const HTTP_UINT8* buffer,
                                   HTTP_INT32 size);

int  HTTP_SetWriteBufferSize      (HTTPSession* session,
                                   HTTP_INT32 size);

int  HTTP_WriteFlush              (HTTPSession* session);

int  HTTP_WriteChunkStart         (HTTPSession* session,
                                   HTTP_INT32 chunkSize);

int  HTTP_WriteChunkEnd           (HTTPSession* session);

int  HTTP_ReadHeaders             (HTTPSession* session,
                                   HTTPHeaderCallback processHeader,
                                   void* userData,
                                   HTTP_UINT8* buffer,
                                   HTTP_INT32 size);

/* ------------------------------------------------------ */
/* Request related */

/* client-side */
int  HTTP_InitRequest             (HTTPSession* session,
                                   HTTPRequest* request,
                                   const HTTP_CHAR* method,
                                   const HTTP_CHAR* target,
                                   HTTP_INT8 httpMajorVersion,
                                   HTTP_INT8 httpMinorVersion);

int  HTTP_SetRequestHeader        (HTTPRequest* request,
                                   const HTTP_CHAR* name,
                                   HTTPHeaderValue* value);

int  HTTP_SetRequestHeaderStr     (HTTPRequest* request,
                                   const HTTP_CHAR* name,
                                   const HTTP_CHAR* value);

int  HTTP_SetRequestHeaderStrLen  (HTTPRequest* request,
                                   const HTTP_CHAR* name,
                                   HTTP_INT32 nameLen,
                                   const HTTP_CHAR* value,
                                   HTTP_INT32 valueLen);

int  HTTP_SetRequestHeaderInt     (HTTPRequest* request,
                                   const HTTP_CHAR* name,
                                   int value);

int  HTTP_SetRequestHeaderTime    (HTTPRequest* request,
                                   const HTTP_CHAR* name,
                                   RTP_TIMESTAMP* timeVal);

int  HTTP_SetRequestHeaderFn      (HTTPRequest* request,
                                   const HTTP_CHAR* name,
								   WriteString writeString,
                                   void* privateData);

int  HTTP_WriteRequest            (HTTPSession* session,
                                   HTTPRequest* request);

void HTTP_FreeRequest             (HTTPRequest* request);

/* server-side */
HTTP_INT32 HTTP_ReadRequest       (HTTPSession* session,
                                   HTTPRequest* request,
                                   HTTP_UINT8* buffer,
                                   HTTP_INT32 size);

/* ------------------------------------------------------ */
/* Response related */

/* server-side */
int  HTTP_InitResponse            (HTTPSession* session,
                                   HTTPResponse* response,
                                   HTTP_INT8 httpMajorVersion,
                                   HTTP_INT8 httpMinorVersion,
                                   HTTP_INT16 httpStatus,
                                   const HTTP_CHAR* httpMessage);

int  HTTP_SetResponseHeader       (HTTPResponse* response,
                                   const HTTP_CHAR* name,
                                   HTTPHeaderValue* value);

int  HTTP_SetResponseHeaderStr    (HTTPResponse* response,
                                   const HTTP_CHAR* name,
                                   const HTTP_CHAR* value);

int HTTP_SetResponseHeaderStrLen  (HTTPResponse* response,
                                   const HTTP_CHAR* name,
                                   HTTP_INT32 nameLen,
                                   const HTTP_CHAR* value,
                                   HTTP_INT32 valueLen);

int  HTTP_SetResponseHeaderInt    (HTTPResponse* response,
                                   const HTTP_CHAR* name,
                                   int value);

int  HTTP_SetResponseHeaderTime   (HTTPResponse* response,
                                   const HTTP_CHAR* name,
                                   RTP_TIMESTAMP* timeVal);

int  HTTP_SetResponseHeaderFn     (HTTPResponse* response,
                                   const HTTP_CHAR* name,
								   WriteString writeString,
                                   void* privateData);

int  HTTP_WriteResponse           (HTTPSession* session,
                                   HTTPResponse* response);

void HTTP_FreeResponse            (HTTPResponse* response);

/* client-side */
HTTP_INT32  HTTP_ReadResponse     (HTTPSession* session,
                                   HTTPResponse* response,
                                   HTTP_UINT8* buffer,
                                   HTTP_INT32 size);

/* ------------------------------------------------------ */
/* Misc utilities                                         */

HTTP_INT32 HTTP_ParseNameValuePair (
		const HTTP_CHAR* str,
		HTTP_CHAR* name,
		HTTP_CHAR* value,
		HTTP_INT32 nameSize,
		HTTP_INT32 valueSize
	);

const HTTP_CHAR* HTTP_MethodTypeToString (
		HTTPMethodType methodType
	);

const HTTP_CHAR* HTTP_HeaderTypeToString (
		HTTPHeaderType headerType
	);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPP_H__ */
