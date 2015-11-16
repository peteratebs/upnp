//
// EVENT.H - UPnP Event-related functions
//
// EBS - HTTP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __EVENT_H__
#define __EVENT_H__

#include "upnp.h"

#ifdef __cplusplus
extern "C" {
#endif

int  UPnP_DeviceEventBindService   (UPnPDeviceRuntime *deviceRuntime,
                                    UPnPService *service);

int  UPnP_DeviceEventUnBindService (UPnPDeviceRuntime *deviceRuntime,
                                    UPnPService *service);                                          


#ifdef __cplusplus
}
#endif

#endif /* __EVENT_H__ */
