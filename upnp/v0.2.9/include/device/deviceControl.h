//
// CONTROL.H - UPnP Control-related functions
//
// EBS - UPnP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __CONTROL_H__
#define __CONTROL_H__

#include "upnpdev.h"

#ifdef __cplusplus
extern "C" {
#endif

int              UPnP_DeviceControlBindService   (UPnPDeviceRuntime *deviceRuntime, 
                                                  UPnPService *service);

int              UPnP_DeviceControlUnBindService (UPnPDeviceRuntime *deviceRuntime, 
                                                  UPnPService *service);
                                          
#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_H__ */
