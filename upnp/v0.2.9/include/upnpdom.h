//
// UPNPDOM.H - UPnP DOM-related functions
//
// EBS - HTTP
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
#ifndef __UPNPDOM_H__
#define __UPNPDOM_H__

#include "upnp.h"

#ifdef __cplusplus
extern "C" {
#endif

RTPXML_Element *UPnP_DOMGetElemInDoc        (RTPXML_Document *descDoc, char *tagName, int num);
RTPXML_Element *UPnP_DOMGetElemInDocNS      (RTPXML_Document *descDoc, char *tagName, char *ns, int num);
RTPXML_Element *UPnP_DOMGetElemInElem       (RTPXML_Element *parent, char *tagName, int num);
DOMString     UPnP_DOMGetElemTextInDoc    (RTPXML_Document *descDoc, char *elemName, int num);
int           UPnP_DOMSetElemTextInDoc    (RTPXML_Document *descDoc, char *elemName, int num, char *value);
DOMString     UPnP_DOMGetElemTextInElem   (RTPXML_Element *parent, char *elemName, int num);
char         *UPnP_DOMSubstituteAddr      (char *baseURL, RTP_NET_ADDR *ipAddr);
char         *UPnP_DOMGetServerPath       (RTPXML_Document *descDoc, DOMString relUrl);

#ifdef __cplusplus
}
#endif

#endif /* __UPNPDOM_H__ */
