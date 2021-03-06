/*
|  RTPQSORT.H - 
| 
|  EBS - 
| 
|   $Author: vmalaiya $
|   $Date: 2006/07/17 15:29:00 $
|   $Name:  $
|   $Revision: 1.3 $
| 
|  Copyright EBS Inc. , 2006
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
*/ 

#ifndef __RTPQSORT_H__
#define __RTPQSORT_H__

/*#define rtp_qsort */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef rtp_qsort
typedef int (*RTP_QSORT_COMPARISON_FN)(const void *node1, const void *node2);
void rtp_qsort (void *head, unsigned long num, unsigned long size, int (*compfunc)(const void *node1, const void *node2));
#endif

#ifdef __cplusplus
}
#endif

#endif /* __RTPQSORT_H__ */
