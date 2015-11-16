//
// DESCRIBE.H -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __DESCRIBE_H__
#define __DESCRIBE_H__

#include "upnpdev.h"
#include "rtpxml.h"

#ifdef __cplusplus
extern "C" {
#endif

UPnPRootDevice * UPnP_DeviceDescribeRootDevice (RTPXML_Document *doc,
                                                int maxDepth);

UPnPDevice     * UPnP_DeviceDescribeDevice     (RTPXML_Element *deviceElement,
                                                UPnPRootDevice *rootDevice,
                                                int maxDepth);

UPnPService    * UPnP_DeviceDescribeService    (RTPXML_Element *serviceElement,
                                                UPnPDevice *device);

void             UPnP_DeviceFreeRootDevice     (UPnPRootDevice *rootDevice);

void             UPnP_DeviceFreeDevice         (UPnPDevice *device);

void             UPnP_DeviceFreeService        (UPnPService *service);

#ifdef __cplusplus
}
#endif

#endif		/* __DESCRIBE_H__ */
