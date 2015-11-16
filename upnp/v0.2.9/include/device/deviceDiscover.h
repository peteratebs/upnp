//
// DISCOVER.H - 
//
// EBS - 
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __DISCOVER_H__
#define __DISCOVER_H__

#include "ssdpsrv.h"
#include "upnpdev.h"

#ifdef __cplusplus
extern "C" {
#endif

int UPnP_DeviceSendAllAlive         (UPnPDeviceRuntime *runtime);
int UPnP_DeviceSendAllByeBye        (UPnPDeviceRuntime *runtime);
int UPnP_DeviceSendRootDeviceAlive  (UPnPRootDevice *rootDevice, int deep);
int UPnP_DeviceSendDeviceAlive      (UPnPDevice *device);
int UPnP_DeviceSendServiceAlive     (UPnPService *service);
int UPnP_DeviceSendRootDeviceByeBye (UPnPRootDevice *rootDevice, int deep);
int UPnP_DeviceSendDeviceByeBye     (UPnPDevice *device);
int UPnP_DeviceSendServiceByeBye    (UPnPService *service);

int UPnP_DeviceSSDPCallback         (SSDPServerContext* ctx, SSDPServerRequest* request, void *cookie);

#ifdef __cplusplus
}
#endif

#endif /*__DISCOVER_H__*/
