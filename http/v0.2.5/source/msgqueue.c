//
// MSGQUEUE.C - Thread message queue
//
// EBS - 
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "rtputil.h"
#include "msgqueue.h"


/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
int rtp_queue_init (RTPMessageQueue *queue, int queueSize, long msgSize, void *messages)
{
	tc_memset(queue, 0, sizeof(RTPMessageQueue));
	
	if (rtp_sig_mutex_alloc(&queue->lock, 0) < 0)
	{
		return (-1);
	}

	if (rtp_sig_semaphore_alloc(&queue->sem, 0) < 0)
	{
		rtp_sig_mutex_free(queue->lock);
		return (-1);
	}
	
	queue->queueSize = queueSize;
	queue->msgSize = msgSize;
	queue->first = 0;
	queue->last = 0;
	queue->messages = messages;
	
	return (0);
}

/*---------------------------------------------------------------------------*/
int rtp_queue_put (RTPMessageQueue *queue, void *message)
{
	if (rtp_sig_mutex_claim(queue->lock) < 0)
	{
		return (-1);
	}
	
	/* make sure the queue is not full */
	if ((queue->last + 1) % queue->queueSize == queue->first)
	{
		rtp_sig_mutex_release(queue->lock);	
		return (-1);
	}
		
	tc_memcpy(&queue->messages[queue->last * queue->msgSize], (char *) message, queue->msgSize);

	queue->last = (queue->last + 1) % queue->queueSize;
		
	rtp_sig_semaphore_signal(queue->sem);
	rtp_sig_mutex_release(queue->lock);
	
	return (0);
}

/*---------------------------------------------------------------------------*/
int rtp_queue_get (RTPMessageQueue *queue, void *message)
{
	if (rtp_sig_semaphore_wait(queue->sem) < 0)
	{
		return (-1);
	}
	
	if (rtp_sig_mutex_claim(queue->lock) < 0)
	{
		return (-1);
	}
	
	if (queue->first != queue->last)
	{
		tc_memcpy((char *) message, &queue->messages[queue->first * queue->msgSize], queue->msgSize);
		
		queue->first = (queue->first + 1) % queue->queueSize;
	}
	
	rtp_sig_mutex_release(queue->lock);

	return (0);
}

/*---------------------------------------------------------------------------*/
int rtp_queue_get_timed (RTPMessageQueue *queue, void *message, long msecTimeout)
{
	if (rtp_sig_semaphore_wait_timed(queue->sem, msecTimeout) < 0)
	{
		return (-1);
	}
	
	if (rtp_sig_mutex_claim(queue->lock) < 0)
	{
		return (-1);
	}
	
	if (queue->first != queue->last)
	{
		tc_memcpy((char *) message, &queue->messages[queue->first * queue->msgSize], queue->msgSize);
		
		queue->first = (queue->first + 1) % queue->queueSize;
	}
	
	rtp_sig_mutex_release(queue->lock);

	return (0);
}

/*---------------------------------------------------------------------------*/
int rtp_queue_peek (RTPMessageQueue *queue)
{
int first;
int last;

	if (rtp_sig_mutex_claim(queue->lock) < 0)
	{
		return (-1);
	}
	
	first = queue->first;
	last = queue->last;
	
	rtp_sig_mutex_release(queue->lock);
		
	if (last < first)
	{
		last += queue->queueSize;
	}
	
	return (last - first);
}

/*---------------------------------------------------------------------------*/
int rtp_queue_free (RTPMessageQueue *queue)
{
	rtp_sig_mutex_free(queue->lock);
	rtp_sig_semaphore_free(queue->sem);	
	
	return (0);
}
