/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_MP_QUEUE_H__
#define __PAN_MP_QUEUE_H__

#include "pan_mp_policy.h"

#ifdef SEND_DAEMON

extern void pan_mp_queue_start(void);
extern void pan_mp_queue_end(void);

extern void pan_mp_queue_lock(pan_mutex_p lock);

extern void  pan_mp_queue_enqueue(void *p);
extern void *pan_mp_queue_dequeue(void);

#endif /* SEND_DAEMON */


#endif /* __PAN_MP_QUEUE_H__ */
