//
// GENATYPES.H - 
//
// EBS - 
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:07 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __GENATYPES_H__
#define __GENATYPES_H__

typedef int             GENA_BOOL;
typedef char            GENA_CHAR;
typedef char            GENA_INT8;
typedef short           GENA_INT16;
typedef long            GENA_INT32;
typedef unsigned char   GENA_UINT8;
typedef unsigned short  GENA_UINT16;
typedef unsigned long   GENA_UINT32;

#define GENA_TIMEOUT_INFINITE   -1

enum e_GENARequestType
{
	GENA_REQUEST_SUBSCRIBE = 0,
	GENA_REQUEST_RENEW,
	GENA_REQUEST_UNSUBSCRIBE,
	GENA_NUM_REQUEST_TYPES
};

typedef enum e_GENARequestType GENARequestType;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* __GENATYPES_H__ */
