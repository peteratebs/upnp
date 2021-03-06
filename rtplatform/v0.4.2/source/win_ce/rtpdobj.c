/*
|  RTPDOBJ.C -
|
|  EBS -
|
|   $Author: vmalaiya $
|   $Date: 2006/07/17 15:29:02 $
|   $Name:  $
|   $Revision: 1.3 $
|
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/
#include "rtpdobj.h"
#include "rtpdebug.h"
#include <time.h>
#include <malloc.h>
#include <string.h>
#include <windows.h>
#include <Winbase.h>

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/*****************************************************************************/
/* Types
 *****************************************************************************/

typedef struct NativeFileSystemObj
{
	HANDLE                   handle;
	WIN32_FIND_DATA          fsObject;
} FSOBJ;

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

static int _rtp_windate_to_date (const FILETIME * windate, RTP_DATE * rtpdate);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/*----------------------------------------------------------------------*
                             rtp_file_gfirst
 *----------------------------------------------------------------------*/
int rtp_file_gfirst (void ** dirobj, char * name)
{
FSOBJ* winDirObj;

#ifdef RTP_DEBUG
    int result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    winDirObj = (FSOBJ*) malloc(sizeof(FSOBJ));
    memset(winDirObj, 0, sizeof(FSOBJ));

    winDirObj->handle = FindFirstFile((unsigned short *) name, &(winDirObj->fsObject));
    if (winDirObj->handle == INVALID_HANDLE_VALUE)
    {
        free (winDirObj);
#ifdef RTP_DEBUG
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_gfirst: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

    *dirobj = (void*) winDirObj;
    return (0);
}



/*----------------------------------------------------------------------*
                             rtp_file_gnext
 *----------------------------------------------------------------------*/
int rtp_file_gnext (void * dirobj)
{
#ifdef RTP_DEBUG
    int result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    if (FindNextFile(((FSOBJ *)dirobj)->handle, &(((FSOBJ *)dirobj)->fsObject)) == 0)
    {
#ifdef RTP_DEBUG
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_gnext: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

    return (0);
}



/*----------------------------------------------------------------------*
                             rtp_file_gdone
 *----------------------------------------------------------------------*/
void rtp_file_gdone (void * dirobj)
{
    FindClose(((FSOBJ *)dirobj)->handle);
	free(dirobj);
}


/*----------------------------------------------------------------------*
                            rtp_file_get_size
 *----------------------------------------------------------------------*/
int rtp_file_get_size (void * dirobj, unsigned long * size)
{
#ifdef RTP_DEBUG
    int  result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    if (!dirobj)
    {
#ifdef RTP_DEBUG
        SetLastError (ERROR_INVALID_HANDLE);
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_get_size: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

	if (size)
	{
		*size = (unsigned long) (
			(((FSOBJ *)dirobj)->fsObject.nFileSizeHigh * MAXDWORD)
			 + ((FSOBJ *)dirobj)->fsObject.nFileSizeLow );

	}
#ifdef RTP_DEBUG
	else
	{
        RTP_DEBUG_OUTPUT_STR("rtp_file_get_size: error no storage location.\n");
	}
#endif
    return (0);
}


/*----------------------------------------------------------------------*
                           rtp_file_get_attrib
 *----------------------------------------------------------------------*/
int rtp_file_get_attrib (void * dirobj, unsigned char * attributes)
{
#ifdef RTP_DEBUG
    int  result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    if (!dirobj)
    {
#ifdef RTP_DEBUG
        SetLastError (ERROR_INVALID_HANDLE);
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_get_attrib: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

	*attributes  = (((FSOBJ *)dirobj)->fsObject.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? RTP_FILE_ATTRIB_ISDIR   : 0;
    *attributes |= (((FSOBJ *)dirobj)->fsObject.dwFileAttributes & FILE_ATTRIBUTE_READONLY)  ? RTP_FILE_ATTRIB_RDONLY  : RTP_FILE_ATTRIB_RDWR;
    *attributes |= (((FSOBJ *)dirobj)->fsObject.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)   ? RTP_FILE_ATTRIB_ARCHIVE : 0;
    *attributes |= (((FSOBJ *)dirobj)->fsObject.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    ? RTP_FILE_ATTRIB_HIDDEN  : 0;
    *attributes |= (((FSOBJ *)dirobj)->fsObject.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)    ? RTP_FILE_ATTRIB_SYSTEM  : 0;

    return (0);
}


/*----------------------------------------------------------------------*
                           rtp_file_get_name
 *----------------------------------------------------------------------*/
int rtp_file_get_name (void * dirobj, char * name, int size)
{
unsigned int sizelimit;

#ifdef RTP_DEBUG
    int  result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    if (!dirobj)
    {
#ifdef RTP_DEBUG
        SetLastError(ERROR_INVALID_HANDLE);
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_get_name: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

    sizelimit = strlen((const char *)((FSOBJ *)dirobj)->fsObject.cFileName );
    if (size > 0)
    {
        if (sizelimit > (unsigned int)(size - 1))
        {
            sizelimit = size - 1;
        }
    }
    strncpy(name, (const char *) (((FSOBJ *)dirobj)->fsObject.cFileName ), sizelimit);
	name[sizelimit] = '\0';
    return (0);
}



/*----------------------------------------------------------------------*
                           rtp_file_get_time
 *----------------------------------------------------------------------*/
int rtp_file_get_time (void * dirobj, RTP_DATE * adate, RTP_DATE * wdate, RTP_DATE * cdate, RTP_DATE * hdate)
{

#ifdef RTP_DEBUG
    int  result;

    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

    if (!dirobj)
    {
#ifdef RTP_DEBUG
        SetLastError (ERROR_INVALID_HANDLE);
        result = GetLastError();
        RTP_DEBUG_OUTPUT_STR("rtp_file_get_time: error returned ");
        RTP_DEBUG_OUTPUT_INT(result);
        RTP_DEBUG_OUTPUT_STR(".\n");
#endif
        return (-1);
    }

    if (adate)
    {
        /* ----------------------------------- */
        /*     Not supported by the FAT fs.    */
        /*     rtp_not_yet_implemented ();     */
        /* ----------------------------------- */
        if (((FSOBJ *)dirobj)->fsObject.ftLastAccessTime.dwLowDateTime)
        {
            if (_rtp_windate_to_date(&((FSOBJ *)dirobj)->fsObject.ftLastAccessTime, adate) != 0)
            {
                return (-1);
            }
        }
    }
    if (wdate)
    {
        if (_rtp_windate_to_date(&((FSOBJ *)dirobj)->fsObject.ftLastWriteTime , wdate) != 0)
        {
            return (-1);
        }
    }
    if (cdate)
    {
        /* ----------------------------------- */
        /*     Not supported by the FAT fs.    */
        /*     rtp_not_yet_implemented ();     */
        /* ----------------------------------- */
        if (((FSOBJ *)dirobj)->fsObject.ftCreationTime.dwLowDateTime)
        {
            if (_rtp_windate_to_date(&((FSOBJ *)dirobj)->fsObject.ftCreationTime , cdate) != 0)
            {
                return (-1);
            }
        }
    }
    if (hdate)
    {
        /* ----------------------------------- */
        /*  Not supported by the windows fs.   */
        /*    rtp_not_yet_implemented ();      */
        /* ----------------------------------- */
    }
    return (0);
}

/************************************************************************
* Utility Function Bodies
************************************************************************/

/*----------------------------------------------------------------------
----------------------------------------------------------------------*/
static int _rtp_windate_to_date (const FILETIME * windate, RTP_DATE * rtpdate)
{
int result;
SYSTEMTIME * systemTime = 0;

#ifdef RTP_DEBUG
    /* ----------------------------------- */
    /*  Clear the error state by setting   */
    /*  to 0.                              */
    /* ----------------------------------- */
    SetLastError (0);
#endif

	result = FileTimeToSystemTime(windate, systemTime);
    if (!result)
    {
#ifdef RTP_DEBUG
        RTP_DEBUG_OUTPUT_STR("_rtp_windate_to_date: error returned.\n");
#endif
        return (-1);
    }

    (*rtpdate).year   = systemTime->wYear;
    (*rtpdate).month  = systemTime->wMonth;
    (*rtpdate).day    = systemTime->wDay;
    (*rtpdate).hour   = systemTime->wHour;
    (*rtpdate).minute = systemTime->wMinute;
    (*rtpdate).second = systemTime->wSecond;
	(*rtpdate).msec   = systemTime->wMilliseconds ;

    (*rtpdate).dlsTime  = 0;    /* always 0 for gmtime */
    (*rtpdate).tzOffset = 0;

    return (0);
}