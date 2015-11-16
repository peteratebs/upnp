//
// FILEEXT.C -
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
#include "fileext.h"
#include "rtpstr.h"

/************************************************************************
* Macros
************************************************************************/

#define UPPERCASE(X) (((X) >= 'a' && (X) <= 'z')? ((X) + 'A' - 'a') : (X))


/************************************************************************
* Types
************************************************************************/

typedef struct s_FileExtensionEntry
{
	const char         *extension;
	FileContentType     contentType;
}
FileExtensionEntry;


/************************************************************************
* Data
************************************************************************/
/* NOTE: ENTRIES IN THIS TABLE ARE ALWAYS ARRANGED IN ASCENDING ORDER */
FileExtensionEntry gpFileExtensionTable [] =
{
	{"323",   FILE_TYPE_TEXT_H323},
	{"AI",    FILE_TYPE_APPLICATION_POSTSCRIPT},
	{"AIF",   FILE_TYPE_AUDIO_X_AIFF},
	{"AIFC",  FILE_TYPE_AUDIO_X_AIFF},
	{"AIFF",  FILE_TYPE_AUDIO_X_AIFF},
	{"ASF",   FILE_TYPE_VIDEO_X_MS_ASF},
	{"ASP",   FILE_TYPE_TEXT_HTML},
	{"ASR",   FILE_TYPE_VIDEO_X_MS_ASF},
	{"ASX",   FILE_TYPE_VIDEO_X_MS_ASF},
	{"AU",    FILE_TYPE_AUDIO_BASIC},
	{"AVI",   FILE_TYPE_VIDEO_X_MSVIDEO},
	{"BAS",   FILE_TYPE_TEXT_PLAIN},
	{"BIN",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"BMP",   FILE_TYPE_IMAGE_BMP},
	{"C",     FILE_TYPE_TEXT_PLAIN},
	{"CPP",   FILE_TYPE_TEXT_PLAIN},
	{"CHM",   FILE_TYPE_TEXT_HTML},
	{"CLA",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"CLASS", FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"COD",   FILE_TYPE_IMAGE_CIS_COD},
	{"CSS",   FILE_TYPE_TEXT_CSS},
	{"DMS",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"EPS",   FILE_TYPE_APPLICATION_POSTSCRIPT},
	{"ETX",   FILE_TYPE_TEXT_X_SETEXT},
	{"EXE",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"FLR",   FILE_TYPE_X_WORLD_X_VRML},
	{"GIF",   FILE_TYPE_IMAGE_GIF},
	{"GTAR",  FILE_TYPE_APPLICATION_X_GTAR},
	{"GZ",    FILE_TYPE_APPLICATION_X_GZIP},
	{"GZIP",  FILE_TYPE_APPLICATION_X_GZIP},
	{"H",     FILE_TYPE_TEXT_PLAIN},
	{"HPP",   FILE_TYPE_TEXT_PLAIN},
	{"HTM",   FILE_TYPE_TEXT_HTML},
	{"HTML",  FILE_TYPE_TEXT_HTML},
	{"HTC",   FILE_TYPE_TEXT_X_COMPONENT},
	{"HTT",   FILE_TYPE_TEXT_WEBVIEWHTML},
	{"ICO",   FILE_TYPE_IMAGE_X_ICON},
	{"IEF",   FILE_TYPE_IMAGE_IEF},
	{"JFIF",  FILE_TYPE_IMAGE_PIPEG},
	{"JPE",   FILE_TYPE_IMAGE_JPEG},
	{"JPEG",  FILE_TYPE_IMAGE_JPEG},
	{"JPG",   FILE_TYPE_IMAGE_JPEG},
	{"JS",    FILE_TYPE_APPLICATION_X_JAVASCRIPT},
	{"JSP",   FILE_TYPE_TEXT_HTML},
	{"LHA",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"LSF",   FILE_TYPE_VIDEO_X_LA_ASF},
	{"LSX",   FILE_TYPE_VIDEO_X_LA_ASF},
	{"LZH",   FILE_TYPE_APPLICATION_OCTET_STREAM},
	{"M3U",   FILE_TYPE_AUDIO_X_MPEGURL},
	{"MHT",   FILE_TYPE_MESSAGE_RFC822},
	{"MHTML", FILE_TYPE_MESSAGE_RFC822},
	{"MID",   FILE_TYPE_AUDIO_MID},
	{"MOV",   FILE_TYPE_VIDEO_QUICKTIME},
	{"MOVIE", FILE_TYPE_VIDEO_X_SGI_MOVIE},
	{"MP2",   FILE_TYPE_VIDEO_MPEG},
	{"MP3",   FILE_TYPE_AUDIO_MPEG},
	{"MPA",   FILE_TYPE_VIDEO_MPEG},
	{"MPE",   FILE_TYPE_VIDEO_MPEG},
	{"MPEG",  FILE_TYPE_VIDEO_MPEG},
	{"MPG",   FILE_TYPE_VIDEO_MPEG},
	{"MPV2",  FILE_TYPE_VIDEO_MPEG},
	{"NWS",   FILE_TYPE_MESSAGE_RFC822},
	{"ODA",   FILE_TYPE_APPLICATION_ODA},
	{"PBM",   FILE_TYPE_IMAGE_X_PORTABLE_BITMAP},
	{"PDF",   FILE_TYPE_APPLICATION_PDF},
	{"PGM",   FILE_TYPE_IMAGE_X_PORTABLE_GRAYMAP},
	{"PHP",   FILE_TYPE_TEXT_HTML},
	{"PNG",   FILE_TYPE_IMAGE_PNG},
	{"PPM",   FILE_TYPE_IMAGE_X_PORTABLE_PIXMAP},
	{"PNM",   FILE_TYPE_IMAGE_X_PORTABLE_ANYMAP},
	{"PS",    FILE_TYPE_APPLICATION_POSTSCRIPT},
	{"QT",    FILE_TYPE_VIDEO_QUICKTIME},
	{"RA",    FILE_TYPE_AUDIO_X_PN_REALAUDIO},
	{"RAM",   FILE_TYPE_AUDIO_X_PN_REALAUDIO},
	{"RAS",   FILE_TYPE_IMAGE_X_CMU_RASTER},
	{"RGB",   FILE_TYPE_IMAGE_X_RGB},
	{"RMI",   FILE_TYPE_AUDIO_MID},
	{"RTF",   FILE_TYPE_TEXT_RICHTEXT},
	{"RTX",   FILE_TYPE_TEXT_RICHTEXT},
	{"SCT",   FILE_TYPE_TEXT_SCRIPTLET},
	{"SND",   FILE_TYPE_AUDIO_BASIC},
	{"STM",   FILE_TYPE_TEXT_HTML},
	{"TAR",   FILE_TYPE_APPLICATION_X_TAR},
	{"TEXT",  FILE_TYPE_TEXT_PLAIN},
	{"TIF",   FILE_TYPE_IMAGE_TIFF},
	{"TIFF",  FILE_TYPE_IMAGE_TIFF},
	{"TSV",   FILE_TYPE_TEXT_TAB_SEPARATED_VALUES},
	{"TXT",   FILE_TYPE_TEXT_PLAIN},
	{"ULS",   FILE_TYPE_TEXT_IULS},
	{"VCF",   FILE_TYPE_TEXT_VCARD},
	{"VRML",  FILE_TYPE_X_WORLD_X_VRML},
	{"WAV",   FILE_TYPE_AUDIO_X_WAV},
	{"WMV",   FILE_TYPE_VIDEO_X_MSVIDEO},
	{"WRL",   FILE_TYPE_X_WORLD_X_VRML},
	{"WRZ",   FILE_TYPE_X_WORLD_X_VRML},
	{"XAF",   FILE_TYPE_X_WORLD_X_VRML},
	{"XBM",   FILE_TYPE_IMAGE_X_XBITMAP},
	{"XOF",   FILE_TYPE_X_WORLD_X_VRML},
	{"XPM",   FILE_TYPE_IMAGE_X_XPIXMAP},
	{"XWD",   FILE_TYPE_IMAGE_X_XWINDOWDUMP},
	{"XML",   FILE_TYPE_TEXT_XML},
	{"Z",     FILE_TYPE_APPLICATION_X_COMPRESS},
	{"ZIP",   FILE_TYPE_APPLICATION_ZIP}
};

#define NUM_FILE_EXTENSIONS (sizeof(gpFileExtensionTable) / sizeof(FileExtensionEntry))


/************************************************************************
* Functions
************************************************************************/

/*---------------------------------------------------------------------------*/
FileContentType GetFileTypeByExtension (const char *fileName)
{
const char *extPos = 0;

	while (fileName[0] && fileName[0] != '?')
	{
		if (fileName[0] == '.')
		{
			extPos = fileName + 1;
		}
		else if (fileName[0] == '/' || fileName[0] == '\\')
		{
			extPos = 0;
		}
		fileName++;
	}

	if (extPos)
	{
		char extStr[8];
		int n;
		int order;
		int low = 0;
		int high = NUM_FILE_EXTENSIONS-1;
		int middle;

		for (n = 0; n < 7 && extPos[0] && extPos[0] != '?'; n++, extPos++)
		{
			extStr[n] = UPPERCASE(extPos[0]);
		}

		extStr[n] = 0;

		while (low <= high)
		{
			middle = (low + high) >> 1;
			order = rtp_strcmp(extStr, gpFileExtensionTable[middle].extension);

			if (order == 0)
			{
				return (gpFileExtensionTable[middle].contentType);
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
	}

	return (FILE_TYPE_UNKNOWN);
}
