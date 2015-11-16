//
// HTTPP.C - HTTP Parser
//
// EBS - Common Library
//
// Copyright EBS Inc. , 2003
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "httpp.h"
#include "rtpmem.h"
#include "rtpstr.h"
#include "rtpscnv.h"
#include "rtpprint.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

#define HTTP_ENTER_HEADER_POOL
#define HTTP_EXIT_HEADER_POOL
#define HTTP_READ_BUFFER_SIZE                   1024

#define IS_WHITESPACE(C) ((C)==' ' ||  (C)=='\r' || (C)=='\n' || (C)=='\t' || (C)=='\f' || (C)=='\v')
#define IS_DIGIT(X)      ((X)>='0' && (X)<='9')

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************
  Data
 *****************************************************************************/

const HTTP_CHAR *gpHTTPHeaderTypeName[HTTP_NUM_HEADER_TYPES] =
{
	"Accept",
	"Accept-Charset",
	"Accept-Encoding",
	"Accept-Language",
	"Accept-Ranges",
	"Age",
	"Allow",
	"Authorization",
	"Cache-Control",
	"Callback",
	"Connection",
	"Content-Encoding",
	"Content-Language",
	"Content-Length",
	"Content-Location",
	"Content-MD5",
	"Content-Range",
	"Content-Type",
	"Cookie",
	"Date",
	"ETAG",
	"Expect",
	"Expires",
	"Ext",
	"From",
	"Host",
	"If-Match",
	"If-Modified-Since",
	"If-None-Match",
	"If-Range",
	"If-Unmodified-Since",
	"Keep-Alive",
	"Last-Modified",
	"Location",
	"Man",
	"Max-Forwards",
	"MX",
	"NT",
	"NTS",
	"Pragma",
	"Proxy-Authenticate",
	"Proxy-Authorization",
	"Range",
	"Referer",
	"Refresh",
	"Retry-After",
	"Server",
	"Set-Cookie",
	"SID",
	"SOAPAction",
	"ST",
	"TE",
	"Timeout",
	"Trailer",
	"Transfer-Encoding",
	"Upgrade",
	"User-Agent",
	"USN",
	"Vary",
	"Via",
	"Warning",
	"WWW-Authenticate"
};

const HTTP_CHAR *gpHTTPMethodTypeName[HTTP_NUM_METHOD_TYPES] =
{
	"CONNECT",
	"DELETE",
	"GET",
	"HEAD",
	"M-POST",
	"M-SEARCH",
	"NOTIFY",
	"OPTIONS",
	"POST",
	"PUT",
	"SUBSCRIBE",
	"TRACE",
	"UNSUBSCRIBE"
};

const HTTP_CHAR HTTP_STR_CR_LF[3] =
{
	'\r','\n',0
};

const HTTP_CHAR HTTP_STR_SPACE[2] =
{
	' ',0
};

const HTTP_CHAR HTTP_STR_COLON[2] =
{
	':',0
};

const HTTP_CHAR HTTP_STR_HTTP_SLASH[6] =
{
	'H','T','T','P','/',0
};

const HTTP_CHAR HTTP_STR_DOT[2] =
{
	'.',0
};

/*****************************************************************************
  Function Prototypes
 *****************************************************************************/

static HTTP_INT32     _HTTP_WriteHeaderValue      (HTTPSession* session,
                                                   HTTPHeaderValue* value);

static HTTP_INT32     _HTTP_WriteHeader           (HTTPSession* session,
                                                   const HTTP_CHAR* name,
                                                   HTTPHeaderValue* value);

static int            _HTTP_ReadChunkHeader       (HTTPSession *session);
static int            _HTTP_ReadChunkTrailer      (HTTPSession *session);

static HTTP_INT32     _HTTP_ReadBuffered          (HTTPSession* session,
                                                   HTTP_UINT8* buffer,
                                                   HTTP_INT32 size);

static HTTP_INT32     _HTTP_ReadLine              (HTTPSession* session,
                                                   HTTP_CHAR* buffer,
                                                   HTTP_INT32 size);

static HTTPHeaderType _HTTP_HeaderNameToType      (const HTTP_CHAR *name);

static HTTPMethodType _HTTP_MethodNameToType      (const HTTP_CHAR *name);

static int            _HTTP_BSearchString         (const HTTP_CHAR *forStr,
                                                   const HTTP_CHAR **inStrList,
                                                   int size);

static void           _HTTP_ProcessHeader         (HTTPSession *session,
                                                   HTTPHeaderType type,
                                                   HTTP_CHAR *name,
                                                   HTTP_CHAR *value);

static HTTP_INT32     _HTTP_ParseHttpVersion      (const HTTP_CHAR *lineBuffer,
                                                   HTTP_INT8 *majorVersion,
                                                   HTTP_INT8 *minorVersion);

static HTTP_INT32     _HTTP_WriteBuffered         (HTTPSession *session,
                                                   const HTTP_UINT8 *buffer,
                                                   HTTP_INT32 size);

static HTTP_INT32     _HTTP_WriteStrBuffered      (HTTPSession* session,
                                                   const HTTP_CHAR* str);

static HTTP_INT32     _HTTP_WriteStrLenBuffered   (HTTPSession* session,
                                                   const HTTP_CHAR* str,
												   HTTP_INT32 len);

static HTTP_INT32     _HTTP_WriteIntBuffered      (HTTPSession* session,
                                                   HTTP_INT32 i);

/*****************************************************************************/
// Session related functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int HTTP_InitSession (HTTPSession *session,         // uninitialized struct
                      HTTPStreamReadFn readFn,	    // stream read callback
                      HTTPStreamWriteFn writeFn,   // stream write callback
                      void *streamPtr)              // stream private pointer
{
	rtp_memset(session, 0, sizeof(HTTPSession));

	session->state = HTTP_STATE_INITIALIZED;
	session->readFn = readFn;
	session->writeFn = writeFn;
	session->streamPtr = streamPtr;

	session->readBuffer = (HTTP_UINT8 *) HTTP_MALLOC(HTTP_READ_BUFFER_SIZE);
	if (session->readBuffer)
	{
		session->readBufferSize = HTTP_READ_BUFFER_SIZE;
	}
	else
	{
		return (-1);
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_ResetSession (HTTPSession *session,
                       HTTPStreamReadFn readFn,
                       HTTPStreamWriteFn writeFn,
                       void *streamPtr)
{
	HTTP_UINT8 *readBuffer      = session->readBuffer;
	HTTP_INT32  readBufferSize  = session->readBufferSize;
	HTTP_UINT8 *writeBuffer     = session->writeBuffer;
  	HTTP_INT32  writeBufferSize = session->writeBufferSize;

	rtp_memset(session, 0, sizeof(HTTPSession));

	session->state = HTTP_STATE_INITIALIZED;
	session->readFn = readFn;
	session->writeFn = writeFn;
	session->streamPtr = streamPtr;
	session->readBuffer = readBuffer;
	session->readBufferSize = readBufferSize;
	session->writeBuffer = writeBuffer;
	session->writeBufferSize = writeBufferSize;

	return (0);
}

/*---------------------------------------------------------------------------*/
void HTTP_SessionSetState (HTTPSession* session,
                           HTTPSessionState state)
{
	session->state = state;
}

/*---------------------------------------------------------------------------*/
void HTTP_FreeSession (HTTPSession *session)
{
	HTTP_WriteFlush(session);

	if (session->readBuffer)
	{
		HTTP_FREE(session->readBuffer);
	}

	if (session->writeBuffer)
	{
		HTTP_FREE(session->writeBuffer);
	}

	rtp_memset(session, 0, sizeof(HTTPSession));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 HTTP_Read (HTTPSession *session,   // HTTP client session
                      HTTP_UINT8 *buffer,     // buffer to read data into
                      HTTP_INT32 size)        // size of buffer
{
HTTP_INT32 bytesLeft = size;
HTTP_INT32 bytesRead = 0;
HTTP_INT32 toRead;
HTTP_INT32 len;
HTTP_INT32 temp;

	if (session->fileSizeKnown)
	{
		temp = session->fileSize - session->bytesReadThisFile;
		if (temp < bytesLeft)
		{
			bytesLeft = temp;
		}
	}

	switch (session->state)
	{
		case HTTP_STATE_READING_DATA:
			if (session->chunkedEncoding)
			{
				while (bytesLeft > 0)
				{
					// if we're at the end of the current chunk, read the next chunk header
					if (session->bytesReadThisChunk >= session->chunkSize)
					{
						_HTTP_ReadChunkHeader(session);
						if (session->chunkSize == 0)
						{
							char buf[65];

							// last chunk; that's all folks...
							do
							{
								len = _HTTP_ReadLine(session, buf, 64);
								if ((len > 0) && buf[len-1] == '\n')
								{
									len--;
									if ((len > 0) && buf[len-1] == '\r')
									{
										len--;
									}
								}
							} while (len > 0);

							session->state = HTTP_STATE_DONE;

							break; // while (bytesLeft > 0)
						}
					}

					temp = session->chunkSize - session->bytesReadThisChunk;
					toRead = (bytesLeft < temp)? bytesLeft : temp;
					len = _HTTP_ReadBuffered(session, &buffer[bytesRead], toRead);

					if (len == 0)
					{
						break;
					}

					if (len < 0)
					{
						if (len == HTTP_EWOULDBLOCK)
						{
							if (bytesRead > 0)
							{
								return (bytesRead);
							}
							return (HTTP_EWOULDBLOCK);
						}
						else
						{
							return (len);
						}
					}

					bytesLeft -= len;
					bytesRead += len;
					session->bytesReadThisChunk += len;
					session->bytesReadThisFile += len;

					// if we've reached the end of the chunk, then read the CR LF at the end
					if (session->bytesReadThisChunk >= session->chunkSize)
					{
						_HTTP_ReadChunkTrailer(session);
					}
				}
			}
			else
			{
				while (bytesLeft > 0)
				{
					len = _HTTP_ReadBuffered(session, &buffer[bytesRead], bytesLeft);

					if (len == 0)
					{
						break;
					}

					if (len < 0)
					{
						if (len == HTTP_EWOULDBLOCK)
						{
							if (bytesRead > 0)
							{
								return (bytesRead);
							}
							return (HTTP_EWOULDBLOCK);
						}
						else
						{
							return (len);
						}
					}

					bytesLeft -= len;
					bytesRead += len;
					session->bytesReadThisFile += len;
				}

				if (session->fileSizeKnown &&
				    session->fileSize == session->bytesReadThisFile)
				{
					session->state = HTTP_STATE_DONE;
				}
			}
			break;

		case HTTP_STATE_DONE:
			return (0);

		default:
			return (-1);
	}

	return (bytesRead);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 HTTP_Write (HTTPSession *session,
                       const HTTP_UINT8 *buffer,
                       HTTP_INT32 size)
{
	return (_HTTP_WriteBuffered(session, buffer, size));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetWriteBufferSize (HTTPSession *session,
                             HTTP_INT32 size)
{
	if (session->writeBufferSize == size)
	{
		return (0);
	}

	if (session->writeBuffer)
	{
		HTTP_FREE(session->writeBuffer);
		session->writeBuffer = 0;
		session->writeBufferSize = 0;
		session->writeBufferContent = 0;
	}

	if (size > 0)
	{
		session->writeBuffer = (HTTP_UINT8 *) HTTP_MALLOC(size);
		if (session->writeBuffer)
		{
			session->writeBufferSize = size;
		}
		else
		{
			return (-1);
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_WriteFlush (HTTPSession *session)
{
	if (session->writeBuffer && session->writeBufferContent)
	{
		HTTP_INT32 bytesSent = session->writeFn(session->streamPtr,
		                                        session->writeBuffer,
		                                        session->writeBufferContent);

		if (bytesSent < session->writeBufferContent)
		{
			session->writeBufferContent = 0;
			return (-1);
		}

        session->writeBufferContent = 0;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_WriteChunkStart (HTTPSession *session,
                          HTTP_INT32 chunkSize)
{
	char hexDigit[] = "0123456789ABCDEF";
	char buf[14];
	char *ptr = &buf[11];

	if (chunkSize < 0)
	{
		return (-1);
	}

	buf[13] = 0;
	buf[12] = '\n';
	buf[11] = '\r';
	if (chunkSize == 0)
	{
		ptr--;
		*ptr = '0';
	}
	else
	{
		while (chunkSize != 0)
		{
			ptr--;
			*ptr = hexDigit[chunkSize % 0x10];
			chunkSize /= 0x10;
		}
	}

	_HTTP_WriteBuffered(session, (unsigned char*)ptr, rtp_strlen(ptr));

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_WriteChunkEnd (HTTPSession *session)
{
	char buf[] = "\r\n";
	_HTTP_WriteBuffered(session, (unsigned char*)buf, 2);
	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_ReadHeaders (HTTPSession *session,
                      HTTPHeaderCallback processHeader,
                      void *userData,
                      HTTP_UINT8 *buffer,
                      HTTP_INT32 size)
{
HTTP_INT32 len, n;
HTTP_CHAR *lineBuffer = (HTTP_CHAR *) buffer;
HTTP_CHAR *headerName;
HTTP_CHAR *headerValue;
HTTPHeaderType headerType;
int skipRemaining = 0;

	if (session->state == HTTP_STATE_READING_HEADERS)
	{
		// Now go through the headers one by one and parse each
		while (1)
		{
			len = _HTTP_ReadLine(session, (HTTP_INT8*)buffer, size-1);

			if ((len > 0) && lineBuffer[len-1] == '\n')
			{
				len--;
				if ((len > 0) && lineBuffer[len-1] == '\r')
				{
					len--;
				}
			}

			if (len <= 0)
			{
				break;
			}

			if (!skipRemaining)
			{
				lineBuffer[len] = '\0';

				n = 0;
				while (IS_WHITESPACE(lineBuffer[n]))
				{
					n++;
				}

				if (!lineBuffer[n])
				{
					// this line is all whitespace
					continue;
				}

				headerName = &lineBuffer[n];

				while (lineBuffer[n] && lineBuffer[n] != ':')
				{
					n++;
				}

				if (lineBuffer[n])
				{
					headerValue = &lineBuffer[n+1];
					while (n > 0 && IS_WHITESPACE(lineBuffer[n-1]))
					{
						n--;
					}
					lineBuffer[n] = 0;

					while (IS_WHITESPACE(*headerValue))
					{
						headerValue++;
					}
				}
				else
				{
					headerValue = &lineBuffer[n];
				}

				headerType = _HTTP_HeaderNameToType(headerName);

				switch (processHeader(userData, session, headerType, headerName, headerValue))
				{
					case -1:
						return (-1);

					case 0:
						_HTTP_ProcessHeader(session, headerType, headerName, headerValue);
						break;

					case 1:
						break;

					case 2:
						skipRemaining = 1;
						break;
				}
			}
		}

		session->state = HTTP_STATE_READING_DATA;

		return (0);
	}
	return (-1);
}

/*****************************************************************************/
// Request related functions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  HTTP_InitRequest (HTTPSession *session,
                       HTTPRequest *request,
                       const HTTP_CHAR *method,
                       const HTTP_CHAR *target,
                       HTTP_INT8 httpMajorVersion,
                       HTTP_INT8 httpMinorVersion)
{
	if (session->state != HTTP_STATE_INITIALIZED &&
	    session->state != HTTP_STATE_DONE)
	{
		return (-1);
	}

	session->state = HTTP_STATE_INITIALIZED;

	rtp_memset(request, 0, sizeof(HTTPRequest));

	request->session = session;
	request->httpMajorVersion = httpMajorVersion;
	request->httpMinorVersion = httpMinorVersion;
	request->method = method;
	request->target = target;

	_HTTP_WriteStrBuffered(session, request->method);
	_HTTP_WriteStrBuffered(session, HTTP_STR_SPACE);
	_HTTP_WriteStrBuffered(session, request->target);
	_HTTP_WriteStrBuffered(session, HTTP_STR_SPACE);
	_HTTP_WriteStrBuffered(session, HTTP_STR_HTTP_SLASH);
	_HTTP_WriteIntBuffered(session, request->httpMajorVersion);
	_HTTP_WriteStrBuffered(session, HTTP_STR_DOT);
	_HTTP_WriteIntBuffered(session, request->httpMinorVersion);
	_HTTP_WriteStrBuffered(session, HTTP_STR_CR_LF);

	session->state = HTTP_STATE_SENDING_REQUEST;

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeader (HTTPRequest *request,
                           const HTTP_CHAR *name,
                           HTTPHeaderValue *value)
{
	return (_HTTP_WriteHeader(request->session, name, value));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeaderStr (HTTPRequest *request,
                              const HTTP_CHAR *name,
                              const HTTP_CHAR *value)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING;
	val.data.str.ptr = value;
	val.data.str.len = rtp_strlen(value);

	return (_HTTP_WriteHeader(request->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeaderStrLen (HTTPRequest *request,
                                 const HTTP_CHAR *name,
                                 HTTP_INT32 nameLen,
                                 const HTTP_CHAR *value,
                                 HTTP_INT32 valueLen)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING;
	val.data.str.ptr = value;
	val.data.str.len = valueLen;

	return (_HTTP_WriteHeader(request->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeaderInt (HTTPRequest *request,
                              const HTTP_CHAR *name,
                              int value)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_INT;
	val.data.i = value;

	return (_HTTP_WriteHeader(request->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeaderTime (HTTPRequest *request,
                               const HTTP_CHAR *name,
                               RTP_TIMESTAMP *timeVal)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_TIME;
	val.data.time = *timeVal;

	return (_HTTP_WriteHeader(request->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetRequestHeaderFn (HTTPRequest *request,
                             const HTTP_CHAR *name,
                             WriteString writeString,
                             void *privateData)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING_OBJECT;
	val.data.strObject.writeString = writeString;
	val.data.strObject.privateData = privateData;

	return (_HTTP_WriteHeader(request->session, name, &val));
}


/*---------------------------------------------------------------------------*/
int HTTP_WriteRequest (HTTPSession *session,
                       HTTPRequest *request)
{
int result = 0;

	session->state = HTTP_STATE_REQUEST_SENT;

	if (_HTTP_WriteStrLenBuffered(session, HTTP_STR_CR_LF, 2) < 0)
	{
		session->state = HTTP_STATE_ERROR;
		result = -1;
	}

	return (result);
}

/*---------------------------------------------------------------------------*/
void HTTP_FreeRequest (HTTPRequest *request)
{
}

/* server-side */
/*---------------------------------------------------------------------------*/
HTTP_INT32 HTTP_ReadRequest (HTTPSession *session,
                             HTTPRequest *request,
                             HTTP_UINT8 *buffer,
                             HTTP_INT32 size)
{
HTTP_INT32 len;
HTTP_INT32 n;

	// read one line
	// parse out: <method> <target> HTTP/<major version>.<minor version>

	if (session->state != HTTP_STATE_INITIALIZED)
	{
		return (-1);
	}

	request->session = session;

	// Read the request line
	len = _HTTP_ReadLine(session, (HTTP_INT8*)buffer, size-1);
	if ((len > 0) && buffer[len-1] == '\n')
	{
		len--;
		if ((len > 0) && buffer[len-1] == '\r')
		{
			len--;
		}
	}
	buffer[len] = '\0';

	for (n = 0; IS_WHITESPACE(buffer[n]); n++) {;}

	request->method = (HTTP_INT8*)&buffer[n];

	if (!buffer[n])
	{
		// incomplete request !
		return (-1);
	}

	while (!IS_WHITESPACE(buffer[n]))
	{
		n++;
	}

	buffer[n++] = 0;

	while (IS_WHITESPACE(buffer[n]))
	{
		n++;
	}

	request->target = (HTTP_INT8*)&buffer[n];

	if (!buffer[n])
	{
		// incomplete request !
		return (-1);
	}

	while (!IS_WHITESPACE(buffer[n]))
	{
		n++;
	}

	buffer[n++] = 0;

	if (_HTTP_ParseHttpVersion((HTTP_INT8*)&buffer[n], &request->httpMajorVersion, &request->httpMinorVersion) < 0)
	{
		return (-1);
	}

	if (request->method)
	{
		request->methodType = _HTTP_MethodNameToType(request->method);
	}
	else
	{
		request->methodType = HTTP_UNKNOWN_METHOD_TYPE;
	}

	session->state = HTTP_STATE_READING_HEADERS;

	return (len + 1/* plus one for the null terminator */);
}


/*****************************************************************************/
// Response related functions
/*****************************************************************************/

/* server-side */

/*---------------------------------------------------------------------------*/
int HTTP_InitResponse (HTTPSession *session,
                       HTTPResponse *response,
                       HTTP_INT8 httpMajorVersion,
                       HTTP_INT8 httpMinorVersion,
                       HTTP_INT16 httpStatus,
                       const HTTP_CHAR *httpMessage)
{
	rtp_memset(response, 0, sizeof(HTTPResponse));

	response->session = session;
	response->httpStatus = httpStatus;
	response->httpMajorVersion = httpMajorVersion;
	response->httpMinorVersion = httpMinorVersion;
	response->httpMessage = httpMessage;

	/* write the response line */
	_HTTP_WriteStrBuffered(session, HTTP_STR_HTTP_SLASH);
	_HTTP_WriteIntBuffered(session, response->httpMajorVersion);
	_HTTP_WriteStrBuffered(session, HTTP_STR_DOT);
	_HTTP_WriteIntBuffered(session, response->httpMinorVersion);
	_HTTP_WriteStrBuffered(session, HTTP_STR_SPACE);
	_HTTP_WriteIntBuffered(session, response->httpStatus);
	_HTTP_WriteStrBuffered(session, HTTP_STR_SPACE);
	_HTTP_WriteStrBuffered(session, response->httpMessage);
	_HTTP_WriteStrBuffered(session, HTTP_STR_CR_LF);

	return (0);
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeader (HTTPResponse *response,
                            const HTTP_CHAR *name,
                            HTTPHeaderValue *value)
{
	return (_HTTP_WriteHeader(response->session, name, value));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeaderStr (HTTPResponse *response,
                               const HTTP_CHAR *name,
                               const HTTP_CHAR *value)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING;
	val.data.str.ptr = value;
	val.data.str.len = rtp_strlen(value);

	return (_HTTP_WriteHeader(response->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeaderStrLen (HTTPResponse *response,
                                  const HTTP_CHAR *name,
                                  HTTP_INT32 nameLen,
                                  const HTTP_CHAR *value,
                                  HTTP_INT32 valueLen)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING;
	val.data.str.ptr = value;
	val.data.str.len = valueLen;

	return (_HTTP_WriteHeader(response->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeaderInt (HTTPResponse *response,
                               const HTTP_CHAR *name,
                               int value)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_INT;
	val.data.i = value;

	return (_HTTP_WriteHeader(response->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeaderTime (HTTPResponse *response,
                                const HTTP_CHAR *name,
                                RTP_TIMESTAMP *timeVal)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_TIME;
	val.data.time = *timeVal;

	return (_HTTP_WriteHeader(response->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_SetResponseHeaderFn (HTTPResponse *response,
                              const HTTP_CHAR *name,
                             WriteString writeString,
                              void *privateData)
{
	HTTPHeaderValue val;

	val.type = HTTP_TYPE_STRING_OBJECT;
	val.data.strObject.writeString = writeString;
	val.data.strObject.privateData = privateData;

	return (_HTTP_WriteHeader(response->session, name, &val));
}

/*---------------------------------------------------------------------------*/
int HTTP_WriteResponse (HTTPSession *session,
                        HTTPResponse *response)
{
int result = 0;

	session->state = HTTP_STATE_RESPONSE_SENT;

	if (_HTTP_WriteStrLenBuffered(session, HTTP_STR_CR_LF, 2) < 0)
	{
		session->state = HTTP_STATE_ERROR;
		result = -1;
	}

	return (result);
}

/*---------------------------------------------------------------------------*/
void HTTP_FreeResponse (HTTPResponse *response)
{

}

/* client-side */

/*---------------------------------------------------------------------------*/
HTTP_INT32 HTTP_ReadResponse (HTTPSession *session,
                              HTTPResponse *response,
                              HTTP_UINT8 *buffer,
                              HTTP_INT32 size)
{
HTTP_INT32 len;

	if (session->state != HTTP_STATE_REQUEST_SENT)
	{
		return (-1);
	}

	response->session = session;
	response->httpMessage = 0;
	response->httpReply[0] = 0;

	// Read the status line
	len = _HTTP_ReadLine(session, (HTTP_INT8*)buffer, size-1);
	if ((len > 0) && buffer[len-1] == '\n')
	{
		len--;
		if ((len > 0) && buffer[len-1] == '\r')
		{
			len--;
		}
	}
	buffer[len] = '\0';

	// set response code
	if (rtp_strnicmp((HTTP_INT8*)buffer, "HTTP/", 5) == 0)
	{
		int n;

		// this is the normal response header form
		response->httpStatus = rtp_atoi((HTTP_INT8*)&(buffer[9]));
		rtp_strncpy(response->httpReply, &buffer[9], sizeof(response->httpReply)); // HEREHERE

		if (buffer[6] == '.')
		{
			response->httpMajorVersion = buffer[5] - '0';
			response->httpMinorVersion = buffer[7] - '0';
		}

		n = 9;

		while (IS_DIGIT(buffer[n]))
		{
			n++;
		}

		while (IS_WHITESPACE(buffer[n]))
		{
			n++;
		}

		response->httpMessage = (HTTP_INT8*)&buffer[n];
	}
	else if (rtp_strnicmp((HTTP_INT8*)buffer, "Status: ", 8) == 0)
	{
		// sometimes we get this type, too
		response->httpStatus = rtp_atoi((HTTP_INT8*)&(buffer[8]));
		rtp_strncpy(response->httpReply, &buffer[8], sizeof(response->httpReply)); // HEREHERE
	}
	else
	{
		int n;
		for (n=0; buffer[n] && !IS_DIGIT(buffer[n]); n++) {;}
		if (buffer[n])
		{
			response->httpStatus = rtp_atoi((HTTP_INT8*)&(buffer[n]));
			rtp_strncpy(response->httpReply, &buffer[9], sizeof(response->httpReply)); // HEREHERE
		}

		while (IS_DIGIT(buffer[n]))
		{
			n++;
		}

		while (IS_WHITESPACE(buffer[n]))
		{
			n++;
		}

		response->httpMessage = (HTTP_INT8*)&buffer[n];
	}

	session->fileSizeKnown = 0;
	session->fileSize = 0;
	session->state = HTTP_STATE_READING_HEADERS;

	return (len + 1 /* plus one for the null terminator */);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_WriteHeaderValue (HTTPSession* session,
                                   HTTPHeaderValue *value)
{
	switch (value->type)
	{
		case HTTP_TYPE_INT:
			return (_HTTP_WriteIntBuffered(session, value->data.i));

		case HTTP_TYPE_STRING:
			_HTTP_WriteStrLenBuffered(session, value->data.str.ptr, value->data.str.len);
			return (value->data.str.len);

		case HTTP_TYPE_STRING_OBJECT:
			return (value->data.strObject.writeString (
					value->data.strObject.privateData,
					(WriteFn)_HTTP_WriteStrLenBuffered,
					session
				));

		case HTTP_TYPE_TIME:
		{
			char buffer[RTP_MAX_DATE_STR_LEN];
			long len;

			rtp_date_print_time(buffer, &value->data.time, 3);
			len = rtp_strlen(buffer);
			_HTTP_WriteStrLenBuffered(session, buffer, len);

			return (rtp_strlen(buffer));
		}
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_WriteHeader (HTTPSession* session,
                              const HTTP_CHAR* name,
                              HTTPHeaderValue* value)
{
	_HTTP_WriteStrBuffered(session, name);
	_HTTP_WriteStrBuffered(session, HTTP_STR_COLON);
	_HTTP_WriteStrBuffered(session, HTTP_STR_SPACE);
	_HTTP_WriteHeaderValue(session, value);
	_HTTP_WriteStrBuffered(session, HTTP_STR_CR_LF);

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ReadChunkHeader (HTTPSession *session)
{
	HTTP_CHAR buffer[64];

	session->bytesReadThisChunk = 0;
	_HTTP_ReadLine(session, buffer, 63);
	session->chunkSize = rtp_strtol(buffer, 0, 16);

	return (0);
}

/*---------------------------------------------------------------------------*/
int _HTTP_ReadChunkTrailer (HTTPSession *session)
{
	char buf[11];
	_HTTP_ReadLine(session, buf, 10);
	return (0);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ReadBuffered (HTTPSession *session,
                               HTTP_UINT8 *buffer,
                               HTTP_INT32 size)
{
	HTTP_INT32 bytesRead = 0;
	HTTP_INT32 bytesReceived;

	// if there is something in our buffer already, use that first
	if (session->readBufferBegin < session->readBufferEnd)
	{
		HTTP_INT32 temp = session->readBufferEnd - session->readBufferBegin;
		HTTP_INT32 toCopy = (temp < size)? temp : size;
		rtp_memcpy(buffer, session->readBuffer + session->readBufferBegin, toCopy);
		session->readBufferBegin += toCopy;
		buffer += toCopy;
		size -= toCopy;
		bytesRead += toCopy;
	}

	if (size > 0)
	{
		session->readBufferBegin = 0;
		session->readBufferEnd = 0;

		// read whatever is left directly off the stream
		bytesReceived = session->readFn(
				session->streamPtr,
				buffer,
				0,
				size);

		if (bytesReceived < 0)
		{
			return (bytesReceived);
		}

		bytesRead += bytesReceived;
	}

	return (bytesRead);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ReadLine (HTTPSession *session,
                           HTTP_CHAR *buffer,
                           HTTP_INT32 size)
{
HTTP_INT32 n, toCopy, bytesCopied = 0, bytesReceived, unreadData, limit;
HTTP_UINT8 done = 0;
HTTP_UINT8 *inBuffer;

	while (1)
	{
		inBuffer = session->readBuffer + session->readBufferBegin;
		unreadData = session->readBufferEnd - session->readBufferBegin;
		limit = (unreadData < size)? unreadData : size;

		// search what's in the buffer to see if we have a complete line
		for (n = 0; n < limit; n++)
		{
			if (inBuffer[n] == (HTTP_UINT8) '\n')
			{
				n++;
				done = 1;
				break;
			}
		}

		toCopy = n;

		if (toCopy > 0)
		{
			rtp_memcpy(buffer, inBuffer, toCopy);

			size -= toCopy;
			inBuffer += toCopy;
			bytesCopied += toCopy;
			session->readBufferBegin += toCopy;

			if (done || (size <= 0))
			{
				break;
			}
		}

		session->readBufferBegin = 0;
		session->readBufferEnd = 0;

		// now receive more data

		bytesReceived = session->readFn(
				session->streamPtr,
				session->readBuffer, 1,
				session->readBufferSize);

		if (bytesReceived < 1)
		{
			break;
		}

		session->readBufferEnd += bytesReceived;
	}

	return (bytesCopied);
}

/*---------------------------------------------------------------------------*/
HTTPHeaderType _HTTP_HeaderNameToType (const HTTP_CHAR *name)
{
	return ((HTTPHeaderType) _HTTP_BSearchString(name, gpHTTPHeaderTypeName, HTTP_NUM_HEADER_TYPES));
}

/*---------------------------------------------------------------------------*/
HTTPMethodType _HTTP_MethodNameToType (const HTTP_CHAR *name)
{
	return ((HTTPMethodType) _HTTP_BSearchString(name, gpHTTPMethodTypeName, HTTP_NUM_METHOD_TYPES));
}

/*---------------------------------------------------------------------------*/
int _HTTP_BSearchString (const HTTP_CHAR *forStr,
                         const HTTP_CHAR **inStrList,
                         int size)
{
	int order;
	int low = 0;
	int high = size-1;
	int middle;

	while (low <= high)
	{
		middle = (low + high) >> 1;
		order = rtp_stricmp(forStr, inStrList[middle]);

		if (order == 0)
		{
			return (middle);
		}

		if (order < 0)
		{
			high = middle - 1;
		}
		else
		{
			low = middle + 1;
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
void _HTTP_ProcessHeader (HTTPSession *session,
                          HTTPHeaderType type,
                          HTTP_CHAR *name,
                          HTTP_CHAR *value)
{
	switch (type)
	{
		case HTTP_HEADER_CONTENT_LENGTH:
			session->fileSize = rtp_atoi(value);
			session->fileSizeKnown = 1;
			break;

		case HTTP_HEADER_TRANSFER_ENCODING:
			if (rtp_stristr(value, "chunked"))
			{
				session->chunkedEncoding = 1;
				session->fileSizeKnown = 0;
			}
			break;

		default:
			// unimportant header
			break;
	}
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_ParseHttpVersion (const HTTP_CHAR *lineBuffer,
                                   HTTP_INT8 *majorVersion,
                                   HTTP_INT8 *minorVersion)
{
	if ((lineBuffer[0] == 'H' || lineBuffer[0] == 'h') &&
	    (lineBuffer[1] == 'T' || lineBuffer[1] == 't') &&
	    (lineBuffer[2] == 'T' || lineBuffer[2] == 't') &&
	    (lineBuffer[3] == 'P' || lineBuffer[3] == 'p') &&
	    (lineBuffer[4] == '/' || lineBuffer[4] == '/'))
	{
		int n=5;

		if (!IS_DIGIT(lineBuffer[n]))
		{
			return (-1);
		}

		*majorVersion = 0;
		while (IS_DIGIT(lineBuffer[n]))
		{
			*majorVersion = (*majorVersion * 10) + (lineBuffer[n] - '0');
			n++;
		}

		if (lineBuffer[n] != '.')
		{
			return (-1);
		}

		n++;

		*minorVersion = 0;
		while (IS_DIGIT(lineBuffer[n]))
		{
			*minorVersion = (*minorVersion * 10) + (lineBuffer[n] - '0');
			n++;
		}

		return (n);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 _HTTP_WriteBuffered (HTTPSession *session,
                                const HTTP_UINT8 *buffer,
                                HTTP_INT32 size)
{
	HTTP_INT32 bytesWritten = 0;
	HTTP_INT32 result;

	if (!session->writeBuffer)
	{
		/* just write the data directly */
		return (session->writeFn(session->streamPtr, buffer, size));
	}

	if (session->writeBufferContent + size >= session->writeBufferSize)
	{
		HTTP_INT32 toCopy;

		/* first fill the current buffer and send it */
		toCopy = session->writeBufferSize - session->writeBufferContent;
		rtp_memcpy (session->writeBuffer + session->writeBufferContent, buffer, toCopy);

		result = session->writeFn(session->streamPtr, session->writeBuffer, session->writeBufferSize);

		if (result < 0)
		{
			return (result);
		}

		buffer += toCopy;
		size -= toCopy;
		bytesWritten += toCopy;

		/* we've cleared out the write buffer */
		session->writeBufferContent = 0;

		/* send some input data directly if it is larger than the buffer size */
		if (size > session->writeBufferSize)
		{
			HTTP_INT32 toSend = size - (size % session->writeBufferSize);

			result = session->writeFn(session->streamPtr, buffer, toSend);

			if (result < 0)
			{
				return (result);
			}

			buffer += toSend;
			size -= toSend;
			bytesWritten += toSend;
		}

	}

	/* buffer any remaining data at the end of the write buffer */
	rtp_memcpy (session->writeBuffer + session->writeBufferContent, buffer, size);
	session->writeBufferContent += size;
	bytesWritten += size;

	return (bytesWritten);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 HTTP_ParseNameValuePair (
		const HTTP_CHAR* str,
		HTTP_CHAR* name,
		HTTP_CHAR* value,
		HTTP_INT32 nameSize,
		HTTP_INT32 valueSize
	)
{
	const HTTP_CHAR* strBegin = str;

	while (IS_WHITESPACE(str[0]))
	{
		str++;
	}

	nameSize--;
	while (str[0] && str[0] != '=' && nameSize > 0)
	{
		name[0] = str[0];
		nameSize--;
		name++;
		str++;
	}
	name[0] = 0;

	while (str[0] && str[0] != '=')
	{
		str++;
	}

	if (str[0] != '=')
	{
		return (-1);
	}

	str++;

	while (IS_WHITESPACE(str[0]))
	{
		str++;
	}

	if (str[0] == '\'' || str[0] == '"')
	{
		str++;
	}

	valueSize--;
	while (str[0] && str[0] != '\'' && str[0] != '"' && valueSize > 0)
	{
		value[0] = str[0];
		valueSize--;
		value++;
		str++;
	}
	value[0] = 0;

	if (str[0] == '\'' || str[0] == '"')
	{
		str++;
	}

	return (str - strBegin);
}

/*---------------------------------------------------------------------------*/
const HTTP_CHAR* HTTP_MethodTypeToString (
		HTTPMethodType methodType
	)
{
	if (methodType < 0 || methodType >= HTTP_NUM_METHOD_TYPES)
	{
		methodType = 0;
	}

	return (gpHTTPMethodTypeName[methodType]);
}

/*---------------------------------------------------------------------------*/
const HTTP_CHAR* HTTP_HeaderTypeToString (
		HTTPHeaderType headerType
	)
{
	if (headerType < 0 || headerType >= HTTP_NUM_HEADER_TYPES)
	{
		headerType = 0;
	}

	return (gpHTTPHeaderTypeName[headerType]);
}

/*---------------------------------------------------------------------------*/
HTTP_INT32     _HTTP_WriteStrBuffered      (HTTPSession* session,
                                            const HTTP_CHAR* str)
{
	return (_HTTP_WriteBuffered(session, (HTTP_UINT8*) str, rtp_strlen(str)));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32     _HTTP_WriteStrLenBuffered   (HTTPSession* session,
                                            const HTTP_CHAR* str,
											HTTP_INT32 len)
{
	return (_HTTP_WriteBuffered(session, (HTTP_UINT8*) str, len));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32     _HTTP_WriteIntBuffered      (HTTPSession* session,
                                            HTTP_INT32 i)
{
	char buffer[16];
	long len;

	rtp_itoa(i, buffer, 10);
	len = rtp_strlen(buffer);
	_HTTP_WriteStrLenBuffered(session, buffer, len);

	return (len);
}

/*---------------------------------------------------------------------------*/
void HTTP_SessionRequireSizeKnown(HTTPSession* session)
{
	if (!session->fileSizeKnown && !session->chunkedEncoding)
	{
		session->fileSizeKnown = 1;
		session->fileSize = 0;
	}
}
