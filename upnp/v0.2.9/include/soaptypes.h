//
// SOAPTYPES.H -
//
// EBS -
//
//  $Author: vmalaiya $
//  $Date: 2006/11/29 18:53:08 $
//  $Name:  $
//  $Revision: 1.1 $
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __SOAPTYPES_H__
#define __SOAPTYPES_H__

typedef int             SOAP_BOOL;
typedef char            SOAP_CHAR;
typedef char            SOAP_INT8;
typedef short           SOAP_INT16;
typedef long            SOAP_INT32;
typedef unsigned char   SOAP_UINT8;
typedef unsigned short  SOAP_UINT16;
typedef unsigned long   SOAP_UINT32;

typedef struct s_SOAPEnvelope SOAPEnvelope;

#include "rtpxml.h"

#define SOAP_ERR_STR_LEN   32
#define SOAP_MALLOC        rtp_malloc
#define SOAP_FREE          rtp_free


struct s_SOAPEnvelope
{
	RTPXML_Document*      doc;
	RTPXML_Element*       headerElem;
	RTPXML_Element*       bodyElem;
};

#ifdef __cplusplus
extern "C" {
#endif

int SOAP_ParseEnvelope (
		SOAPEnvelope* envelope,
		SOAP_CHAR* xmlStr,
		SOAP_INT32 xmlLen
	);

#ifdef __cplusplus
}
#endif

#endif /* __SOAPTYPES_H__ */
