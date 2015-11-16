//
// URLPARSE.H - URL parsing routines
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __URLPARSE_H__
#define __URLPARSE_H__

typedef struct s_URLDescriptor URLDescriptor;

enum e_URLProtocolType
{
	URL_PROTOCOL_UNKNOWN = -1,
	URL_PROTOCOL_FILE = 0,
	URL_PROTOCOL_FTP,
	URL_PROTOCOL_HTTP,
	URL_PROTOCOL_HTTPS,
	URL_PROTOCOL_JAVASCRIPT,
	URL_PROTOCOL_MAILTO,
	URL_NUM_PROTOCOLS
};
typedef enum e_URLProtocolType URLProtocolType;

struct s_URLDescriptor
{
	char*           protocolName;
	URLProtocolType protocolType;

	union
	{
		struct
		{
			const char*    host;
			unsigned short port;
			const char*    path;
			const char*    anchor;
		}
		http;

		/*
		struct
		{
		}
		file;

		struct
		{
		}
		ftp;

		struct
		{
		}
		javascript;
		*/
	}
	field;

	char*           buffer;
	long            bufferSize;
};

/*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void URL_Init        (URLDescriptor* desc);

int  URL_Copy        (URLDescriptor* to,
                      URLDescriptor* from);

int  URL_Parse       (URLDescriptor* desc,
                      const char* str,
                      URLProtocolType baseProtocol,
                      int ipType);

int  URL_GetAbsolute (URLDescriptor* outAbsolute,
                      URLDescriptor* inRelative,
                      URLDescriptor* inBase,
                      int ipType);

void URL_Free        (URLDescriptor* desc);

#ifdef __cplusplus
}
#endif

#endif /* __URLPARSE_H__ */
