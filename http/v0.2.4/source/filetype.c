//
// FILETYPE.C - Runtime Platform Services
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
// Module description:
//  [tbd]
//

/************************************************************************
* Headers
************************************************************************/
#include "filetype.h"
#include "rtpstr.h"

/************************************************************************
* Types
************************************************************************/


/************************************************************************
* Data
************************************************************************/

const char *gpFileContentTypeNames [NUM_FILE_TYPES] =
{
	"application/oda",                  /* RFC 1494 */
	"application/octet-stream",         /* RFC 1494 */
	"application/pdf",
	"application/postscript",           /* RFC 1494 */
	"application/x-compress",
	"application/x-gtar",
	"application/x-gzip",
	"application/x-javascript",
	"application/x-tar",
	"application/zip",
	"audio/basic",                      /* RFC 1494 */
	"audio/mid",
	"audio/mpeg",
	"audio/x-aiff",
	"audio/x-mpegurl",
	"audio/x-pn-realaudio",
	"audio/x-wav",
	"image/bmp",
	"image/cis-cod",
	"image/gif",                        /* RFC 1494 */
	"image/ief",
	"image/jpeg",                       /* RFC 1494 */
	"image/pipeg",
	"image/png",
	"image/tiff",
	"image/x-cmu-raster",
	"image/x-icon",
	"image/x-portable-anymap",
	"image/x-portable-bitmap",
	"image/x-portable-graymap",
	"image/x-portable-pixmap",
	"image/x-rgb",
	"image/x-xbitmap",
	"image/x-xpixmap",
	"image/x-xwindowdump",
	"message/rfc822",
	"text/css",
	"text/h323",
	"text/html",
	"text/iuls",
	"text/plain",                       /* RFC 1494 */
	"text/richtext",                    /* RFC 1494 */
	"text/scriptlet",
	"text/tab-separated-values",
	"text/vcard",
	"text/webviewhtml",
	"text/x-component",
	"text/x-setext",
	"text/xml",
	"video/mpeg",                       /* RFC 1494 */
	"video/quicktime",
	"video/x-la-asf",
	"video/x-ms-asf",
	"video/x-msvideo",
	"video/x-sgi-movie",
	"x-world/x-vrml"
};


/************************************************************************
* Functions
************************************************************************/

/*---------------------------------------------------------------------------*/
FileContentType StrToFileContentType (const char *str)
{
	int order;
	int low = 0;
	int high = NUM_FILE_TYPES-1;
	int middle;

	while (low <= high)
	{
		middle = (low + high) >> 1;
		order = rtp_stricmp(str, gpFileContentTypeNames[middle]);

		if (order == 0)
		{
			return ((FileContentType) middle);
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

	return (FILE_TYPE_UNKNOWN);
}

/*---------------------------------------------------------------------------*/
const char     *FileContentTypeToStr (FileContentType type)
{
	if (type >= 0 && type < NUM_FILE_TYPES)
	{
		return (gpFileContentTypeNames[type]);
	}

	return (0);
}
