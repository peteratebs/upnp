//
// MSGQUEUE.H - Thread message queue
//
// EBS - 
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

#ifndef __MSGQUEUE_H__
#define __MSGQUEUE_H__

#include "rtpsignl.h"

typedef struct s_RTPMessageQueue
{
	RTP_SEMAPHORE  sem;
	RTP_MUTEX      lock;
	int            queueSize;
	long           msgSize;
	int            first;
	int            last;
	char          *messages;
}
RTPMessageQueue;

#ifdef __cplusplus
extern "C" {
#endif

int rtp_queue_init      (RTPMessageQueue *queue, int queueSize, long msgSize, void *messages);
int rtp_queue_put       (RTPMessageQueue *queue, void *message);
int rtp_queue_get       (RTPMessageQueue *queue, void *message);
int rtp_queue_get_timed (RTPMessageQueue *queue, void *message, long msecTimeout);
int rtp_queue_peek      (RTPMessageQueue *queue);
int rtp_queue_free      (RTPMessageQueue *queue);

#ifdef __cplusplus
}
#endif

#endif /* __MSGQUEUE_H__ */
