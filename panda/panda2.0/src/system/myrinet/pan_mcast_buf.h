/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Circular message buffer implementation.
 *
 * The implementation increases the size to a power of two for cheap MOD
 * operations.
 *
 * Interface:
 *	get()/put() to enter a new entry/to delete the oldest entry
 *	get() waits if the buffer is full until a put() signals it.
 * Semantics:
 *	last indicates the oldest living entry
 *	next indicates the next free entry
 */


#ifndef _GROUP_PAN_NUM_BUF_H
#define _GROUP_PAN_NUM_BUF_H

#include <assert.h>


#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif



typedef struct PAN_NUM_BUF_T pan_buf_t, *pan_buf_p;

struct PAN_NUM_BUF_T {
    pan_msg_p      *buf;		/* the buffer */
    int		    last;		/* next to be read */
    int		    next;		/* next to be written */
    int		    size;		/* size */
    pan_mutex_p	    lock;		/* lock the buffer */
    pan_cond_p	    non_full;		/* signal if non-full */
    int		    waiters;		/* # waiting threads */
};


#define BUFMOD(the_buf, idx) \
	((idx) & ((the_buf)->size - 1))

#define BUF(the_buf, idx) \
	((the_buf)->buf[BUFMOD(the_buf, idx)])


#ifdef NO_INLINE_WARNINGS


INLINE int
pan_buf_in_range(pan_buf_p buf, int idx)
{
    return ((idx) >= (buf)->last && (idx) < (buf)->last + (buf)->size);
}


INLINE int
pan_buf_last(pan_buf_p buf)				/* used */
{
    return buf->last;
}


INLINE int
pan_buf_next(pan_buf_p buf)				/* used */
{
    return buf->next;
}



INLINE pan_msg_p
pan_buf_look(pan_buf_p buf, int seqno)			/* used */
{
/* Return the message contained in message buffer slot "seqno".
 */
    if (! pan_buf_in_range(buf, seqno)) {
	return NULL;
    }
    return BUF(buf, seqno);
}


#else		/* NO_INLINE_WARNINGS */

#define pan_buf_in_range(buf, idx) \
	(((idx) >= (buf)->last) && ((idx) < (buf)->last + (buf)->size))

#define pan_buf_last(buf) \
	((buf)->last)

#define pan_buf_next(buf) \
	((buf)->next)


/* Return the message contained in message buffer slot "seqno".
 */
#define pan_buf_look(buf, seqno) \
	(pan_buf_in_range(buf, seqno) ? BUF(buf, seqno) : NULL)


#endif		/* NO_INLINE_WARNINGS */


#ifndef STATIC_CI

pan_msg_p      pan_buf_get(pan_buf_p buf);	/* used */

void           pan_buf_put(pan_buf_p buf, pan_msg_p msg);

#endif


/* Initialisation, termination */

pan_buf_p      pan_buf_create(int size, pan_mutex_p lock);

int            pan_buf_clear(pan_buf_p buf,
			     void (*msg_clear)(pan_msg_p msg));

#endif
