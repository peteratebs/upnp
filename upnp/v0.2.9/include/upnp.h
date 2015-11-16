//
// UPNP.H -
//
// EBS -
//
// Copyright EBS Inc. , 2005
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __UPNP_H__
#define __UPNP_H__

#define UPNP_VERSION         0029 /* 0.2.9 */

#define                      UPNP_MULTITHREAD

typedef char                 UPNP_CHAR;
typedef char                 UPNP_INT8;
typedef short                UPNP_INT16;
typedef long                 UPNP_INT32;
typedef unsigned char        UPNP_UINT8;
typedef unsigned short       UPNP_UINT16;
typedef unsigned long        UPNP_UINT32;
typedef int                  UPNP_BOOL;

typedef struct s_UPnPRuntime UPnPRuntime;

#define UPNP_TRUE      1
#define UPNP_FALSE     0

#define UPNP_MAX_ERROR_DESC_LEN        32
#define UPNP_MAX_STATE_VAR_VALUE_LEN   128
#define UPNP_EVENT_SEND_TIMEOUT_SEC    30

#ifdef UPNP_MULTITHREAD
#define UPNP_RUNTIME_ENTER_CRITICAL(X)  rtp_sig_mutex_claim((X)->mutex)
#define UPNP_RUNTIME_EXIT_CRITICAL(X)   rtp_sig_mutex_release((X)->mutex)
#else
#define UPNP_RUNTIME_ENTER_CRITICAL(X)
#define UPNP_RUNTIME_EXIT_CRITICAL(X)
#endif

#include "rtp.h"
#include "httpp.h"
#include "ssdpsrv.h"

#include "httpsrv.h"
#include "rtpxml.h"
#include "upnpcli.h"
#include "upnpdev.h"
#include "assert.h"

#ifdef UPNP_MULTITHREAD
 #include "rtpsignl.h"
#endif

/* Check for following required verions of http and rtplatform
   HTTP_VERSION 0023 ----> 0.2.3
   RTPLATFORM_VERSION 0041 --->0.4.1
*/
#if (HTTP_VERSION < 0023)
#error USE HTTP VERSION 0.2.3 or greater
#endif

#if (RTPLATFORM_VERSION < 0041)
#error USE RTPLATFORM VERSION 0.4.1 or greater
#endif

#ifdef RTP_DEBUG
#define UPNP_ASSERT(X, Y)  { \
                               if (!X) \
                               { \
                                   RTP_DEBUG_OUTPUT_STR(Y); \
                                   RTP_DEBUG_OUTPUT_STR(".\n"); \
                               } \
                           }
#else
#define UPNP_ASSERT(X, Y)
#endif



#define UPNP_TIMEOUT_INFINITE          GENA_TIMEOUT_INFINITE

typedef enum e_UPnPDaemonState
{
	UPNP_DAEMON_STOPPED = 0,
	UPNP_DAEMON_RUNNING,
	UPNP_DAEMON_STOPPING,
	UPNP_NUM_DAEMON_STATES
}
UPnPDaemonState;

struct s_UPnPRuntime
{
	SSDPServerContext        ssdpContext;
	HTTPServerContext        httpServer;
	UPnPDeviceRuntime*       deviceRuntime;
	UPnPControlPoint*        controlPoint;
	UPNP_INT16               ipType;
  #ifdef UPNP_MULTITHREAD
  	UPnPDaemonState          daemonState;
	RTP_MUTEX                mutex;
  #endif
};

#ifdef __cplusplus
extern "C" {
#endif

int UPnP_RuntimeInit (
		UPnPRuntime* rt,
		UPNP_UINT8* serverAddr,
		UPNP_UINT16 serverPort,
		UPNP_INT16 ipType,
		UPNP_CHAR* wwwRootDir,
		UPNP_INT16 maxConnections,
		UPNP_INT16 maxHelperThreads
	);

void UPnP_RuntimeDestroy (
		UPnPRuntime* rt
	);

int UPnP_AddVirtualFile (
		UPnPRuntime* rt,
		const UPNP_CHAR* serverPath,
		const UPNP_UINT8* data,
		UPNP_INT32 size,
		const UPNP_CHAR* contentType
	);

int UPnP_RemoveVirtualFile (
		UPnPRuntime* rt,
		const UPNP_CHAR* serverPath
	);

int UPnP_ProcessState (
		UPnPRuntime* rt,
		UPNP_INT32 msecTimeout
	);

int UPnP_StartDaemon (
		UPnPRuntime* rt
	);

int UPnP_StopDaemon (
		UPnPRuntime* rt,
		UPNP_INT32 secTimeout
	);

/*****************************************************************************/

int UPnP_AddToPropertySet (
		RTPXML_Document** doc,
		const UPNP_CHAR* name,
		const UPNP_CHAR* value
	);

const UPNP_CHAR* UPnP_GetPropertyValueByName (
		RTPXML_Document* propertySet,
		const UPNP_CHAR* name
	);

#define UPnP_GetPropertyValue(S,N) UPnP_GetPropertyValueByName(S,N)

const UPNP_CHAR* UPnP_GetPropertyNameByIndex (
		RTPXML_Document* propertySet,
		int index
	);

const UPNP_CHAR* UPnP_GetPropertyValueByIndex (
		RTPXML_Document* propertySet,
		int index
	);

/*****************************************************************************/

RTPXML_Document* UPnP_CreateAction (
		const UPNP_CHAR* serviceTypeURI,
		const UPNP_CHAR* actionName
	);

int UPnP_SetActionArg (
		RTPXML_Document* actionDoc,
		const UPNP_CHAR* name,
		const UPNP_CHAR* value
	);

#ifdef __cplusplus
}
#endif

#endif /* __UPNP_H__ */
