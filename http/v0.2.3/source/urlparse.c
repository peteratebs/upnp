//
// URLPARSE.C -
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

#include "urlparse.h"
#include "rtpmem.h"
#include "rtpstr.h"
#include "rtpscnv.h"
#include "rtpnet.h"
#include "assert.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

void _URL_CollapsePath (char* to, const char* from);

static int  _URL_CopyHTTP        (URLDescriptor* to,
                                  URLDescriptor* from);

static int  _URL_ParseHTTP       (URLDescriptor* desc,
                                  const char* hostPos,
                                  int ipType);


static int  _URL_GetAbsoluteHTTP (URLDescriptor* outAbsolute,
                                  URLDescriptor* inRelative,
                                  URLDescriptor* inBase,
                                  int ipType);

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int  URL_Copy        (URLDescriptor* to,
                      URLDescriptor* from)
{
	to->buffer = (char*) rtp_malloc(sizeof(char) * from->bufferSize);

	if (to->buffer)
	{
		to->protocolType = from->protocolType;
		to->bufferSize = from->bufferSize;

		switch (from->protocolType)
		{
			case URL_PROTOCOL_HTTP:
				if (_URL_CopyHTTP(to, from) >= 0)
				{
					return (0);
				}
				break;
		}

		rtp_free(to->buffer);
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  URL_Parse       (URLDescriptor* desc,
                      const char* str,
                      URLProtocolType baseProtocol,
                      int ipType)
{
	const char* ptr;

	desc->protocolType = baseProtocol;

	ptr = rtp_strstr(str, "://");

	if (ptr)
	{
		/* everything before the "://" is the protocol */
		if (!rtp_strnicmp(str, "HTTP", 4))
		{
			desc->protocolType = URL_PROTOCOL_HTTP;
		}
	}

	switch (desc->protocolType)
	{
		case URL_PROTOCOL_HTTP:
			if (_URL_ParseHTTP(desc, str, ipType) >= 0)
			{
				return (0);
			}
			break;
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
int  URL_GetAbsolute (URLDescriptor* outAbsolute,
                      URLDescriptor* inRelative,
                      URLDescriptor* inBase,
                      int ipType)
{
	if (inRelative->protocolType != URL_PROTOCOL_UNKNOWN)
	{
		if (inRelative->protocolType != inBase->protocolType)
		{
			return (URL_Copy(outAbsolute, inRelative));
		}

		outAbsolute->protocolType = inRelative->protocolType;
	}
	else if (inBase->protocolType != URL_PROTOCOL_UNKNOWN)
	{
		outAbsolute->protocolType = inRelative->protocolType;
	}

	if (outAbsolute->protocolType != URL_PROTOCOL_UNKNOWN)
	{
		switch (outAbsolute->protocolType)
		{
			case URL_PROTOCOL_HTTP:
				if (_URL_GetAbsoluteHTTP(outAbsolute, inRelative, inBase, ipType) >= 0)
				{
					return (0);
				}
				break;

			default:
				break;
		}
	}

	return (-1);
}

/*---------------------------------------------------------------------------*/
void URL_Free        (URLDescriptor* desc)
{
	if (desc->buffer)
	{
		rtp_free(desc->buffer);
		desc->buffer = 0;
	}
}

/*---------------------------------------------------------------------------*/
void URL_Init        (URLDescriptor* desc)
{
	rtp_memset(desc, 0, sizeof(URLDescriptor));
}

/*---------------------------------------------------------------------------*/
void _URL_CollapsePath (char* to, const char* from)
{
	char* toPtr = to;
	const char* fromPtr = from;
	long chunkLen = 0;

	while (*fromPtr)
	{
		chunkLen = 0;
		while (fromPtr[chunkLen] && fromPtr[chunkLen] != '/' && fromPtr[chunkLen] != '\\')
		{
			chunkLen++;
		}

		if (chunkLen == 1)
		{
			if (fromPtr[0] == '.')
			{
				if (fromPtr[chunkLen] == 0)
				{
					break;
				}

				fromPtr += (chunkLen + 1);
				continue;
			}
		}
		else if (chunkLen == 2)
		{
			if (fromPtr[0] == '.' && fromPtr[1] == '.')
			{
				if (toPtr > to)
				{
					toPtr--;

					while (toPtr > to)
					{
						toPtr--;
						if (*toPtr == '/' || *toPtr == '\\')
						{
							break;
						}
					}

					if (*toPtr == '/' || *toPtr == '\\')
					{
						toPtr++;
					}
				}

				if (fromPtr[chunkLen] == 0)
				{
					break;
				}

				fromPtr += (chunkLen + 1);
				continue;
			}
		}

		rtp_memcpy(toPtr, fromPtr, (chunkLen + 1)*sizeof(char));
		toPtr += (chunkLen + 1);

		if (fromPtr[chunkLen] == 0)
		{
			break;
		}

		fromPtr += (chunkLen + 1);
	}

	*toPtr = 0;
}

/*---------------------------------------------------------------------------*/
static int  _URL_CopyHTTP        (URLDescriptor* to,
                                  URLDescriptor* from)
{
	char* ptr = to->buffer;

	to->protocolName = ptr;
	rtp_strcpy(ptr, "HTTP");
	ptr = to->buffer + 5;

	if (from->field.http.host)
	{
		to->field.http.host = ptr;
		rtp_strcpy(ptr, from->field.http.host);
		ptr += rtp_strlen(to->field.http.host)+1;
	}
	else
	{
		to->field.http.host = 0;
	}

	to->field.http.port = from->field.http.port;

	if (from->field.http.path)
	{
		to->field.http.path = ptr;
		rtp_strcpy(ptr, from->field.http.path);
		ptr += rtp_strlen(to->field.http.path);
	}
	else
	{
		to->field.http.path = 0;
	}

	if (from->field.http.anchor)
	{
		to->field.http.anchor = ptr;
		rtp_strcpy(ptr, from->field.http.anchor);
		ptr += rtp_strlen(to->field.http.anchor);
	}
	else
	{
		to->field.http.anchor = 0;
	}

	return (0);
}

/*---------------------------------------------------------------------------*/
static int  _URL_ParseHTTP       (URLDescriptor* to,
                                  const char* str,
                                  int ipType)
{
	to->bufferSize = sizeof(char) * (rtp_strlen(str) + 8);
	to->buffer = (char*) rtp_malloc(to->bufferSize);
	if (to->buffer)
	{
		char* ptr = to->buffer;
		char* host = rtp_strstr(str, "//");
		char* path = (host)? rtp_strstr(host + 2, "/") : (char*)str;
		char* anchor = (path)? rtp_strstr(path, "#") : 0;
		char *port;
		int index;

		if(ipType == RTP_NET_TYPE_IPV6)
		{
			if(host)
            {
                index = (path) ? (rtp_strlen(host) - rtp_strlen(path)) :  rtp_strlen(host);
		        while(host[index] !=':')
		        {
		            index--;
		        }
		        port = host+index;
            }
		}
		else
		{
			port = (host)? rtp_strstr(host + 2, ":") : 0;
		}

		to->protocolName = ptr;
		rtp_strcpy(ptr, "HTTP");
		ptr = to->buffer + 5;

		if (host)
		{
			long hostLen = (port)? (port - host) : ((path)? (path - host) : rtp_strlen(host));

			host += 2;
			hostLen -= 2;

			to->field.http.host = ptr;
			rtp_memcpy(ptr, host, hostLen*sizeof(char));
			ptr[hostLen] = 0;
			ptr += hostLen + 1;
		}
		else
		{
			to->field.http.host = 0;
		}

		if (port)
		{
			to->field.http.port = rtp_atoi(port + 1);
		}
		else
		{
			to->field.http.port = (host)? 80 : 0;
		}

		if (path)
		{
			long pathLen = (anchor)? (anchor - path) : rtp_strlen(path);

			to->field.http.path = ptr;
			rtp_memcpy(ptr, path, pathLen*sizeof(char));
			ptr[pathLen] = 0;
			ptr += pathLen + 1;
		}
		else
		{
			to->field.http.path = 0;
		}

		if (anchor)
		{
			to->field.http.anchor = ptr;
			rtp_strcpy(ptr, anchor);
			ptr += rtp_strlen(to->field.http.anchor);
		}
		else
		{
			to->field.http.anchor = 0;
		}
		return (0);
	}

	to->bufferSize = 0;

	return (-1);
}

/*---------------------------------------------------------------------------*/
static int  _URL_GetAbsoluteHTTP (URLDescriptor* outAbsolute,
                                  URLDescriptor* inRelative,
                                  URLDescriptor* inBase,
                                  int ipType)
{
	long sizeNeeded = 8;

	if (inRelative->field.http.host)
	{
		sizeNeeded += rtp_strlen(inRelative->field.http.host);
	}
	else if (inBase->field.http.host)
	{
		sizeNeeded += rtp_strlen(inBase->field.http.host);
	}

	if (inRelative->field.http.path)
	{
		sizeNeeded += rtp_strlen(inRelative->field.http.path);
	}

	if (inBase->field.http.path)
	{
		sizeNeeded += rtp_strlen(inBase->field.http.path);
	}

	if (inRelative->field.http.anchor)
	{
		sizeNeeded += rtp_strlen(inRelative->field.http.anchor);
	}
	else if (inBase->field.http.anchor)
	{
		sizeNeeded += rtp_strlen(inBase->field.http.anchor);
	}

	outAbsolute->buffer = (char*) rtp_malloc(sizeof(char) * sizeNeeded);

	if (outAbsolute->buffer)
	{
		char* ptr = outAbsolute->buffer;

		outAbsolute->bufferSize = sizeNeeded;

		outAbsolute->protocolName = ptr;
		rtp_strcpy(ptr, "HTTP");
		ptr = outAbsolute->buffer + 5;

		if (inRelative->field.http.port)
		{
			outAbsolute->field.http.port = inRelative->field.http.port;
		}
		else if (inBase->field.http.port)
		{
			outAbsolute->field.http.port = inBase->field.http.port;
		}
		else
		{
			outAbsolute->field.http.port = 80;
		}

		if (inRelative->field.http.host)
		{
			outAbsolute->field.http.host = ptr;
			rtp_strcpy(ptr, inRelative->field.http.host);
			ptr += rtp_strlen(outAbsolute->field.http.host) + 1;
		}
		else if (inBase->field.http.host)
		{
			outAbsolute->field.http.host = ptr;
			rtp_strcpy(ptr, inBase->field.http.host);
			ptr += rtp_strlen(outAbsolute->field.http.host) + 1;
		}
		else
		{
			outAbsolute->field.http.host = 0;
		}

		if (inRelative->field.http.path)
		{
			outAbsolute->field.http.path = ptr;

			if (inRelative->field.http.path[0] != '\\' &&
			    inRelative->field.http.path[0] != '/' &&
			    inBase->field.http.path &&
				(inBase->field.http.path[0] == '\\' ||
			     inBase->field.http.path[0] == '/'))
			{
				int baseLen = 0;
				int n = 0;

				while (inBase->field.http.path[n])
				{
					if (inBase->field.http.path[n] == '\\' ||
					    inBase->field.http.path[n] == '/')
					{
						baseLen = n + 1;
					}
					n++;
				}

				rtp_strncpy(ptr, inBase->field.http.path, baseLen);
				ptr += baseLen;

				rtp_strcpy(ptr, inRelative->field.http.path);
				ptr += rtp_strlen(inRelative->field.http.path) + 1;
			}
			else
			{
				rtp_strcpy(ptr, inRelative->field.http.path);
				ptr += rtp_strlen(outAbsolute->field.http.path) + 1;
			}
		}
		else if (inBase->field.http.path)
		{
			outAbsolute->field.http.path = ptr;
			rtp_strcpy(ptr, inBase->field.http.path);
			ptr += rtp_strlen(outAbsolute->field.http.path) + 1;
		}
		else
		{
			outAbsolute->field.http.path = 0;
		}

		if (inRelative->field.http.anchor)
		{
			outAbsolute->field.http.anchor = ptr;
			rtp_strcpy(ptr, inRelative->field.http.anchor);
			ptr += rtp_strlen(outAbsolute->field.http.anchor) + 1;
		}
		else if (inBase->field.http.anchor)
		{
			outAbsolute->field.http.anchor = ptr;
			rtp_strcpy(ptr, inBase->field.http.anchor);
			ptr += rtp_strlen(outAbsolute->field.http.anchor) + 1;
		}
		else
		{
			outAbsolute->field.http.anchor = 0;
		}

		return (0);
	}

	outAbsolute->bufferSize = 0;

	return (-1);
}
