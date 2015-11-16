//
// FILEEXT.H - File extension identifier
//
// EBS - HTTP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __FILEEXT_H__
#define __FILEEXT_H__

#include "filetype.h"

#ifdef __cplusplus
extern "C" {
#endif

FileContentType GetFileTypeByExtension (const char *fileName);

#ifdef __cplusplus
}
#endif

#endif /*__FILEEXT_H__*/
