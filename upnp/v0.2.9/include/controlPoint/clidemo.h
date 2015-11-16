#ifndef __CLIDEMO_H__	
#define __CLIDEMO_H__

#include "ssdpcli.h"

//#define NULL 0

int UPnP_ClientSSDPCallback(
     SSDPClientContext *ctx,     /** the SSDP context */
     SSDPAnnounce *announce,     /** address of buffer holding ssdp request 
                                     information */
     void *cookie            	 /** cookie holds pointer to device run time 
                                     information */);
#endif 