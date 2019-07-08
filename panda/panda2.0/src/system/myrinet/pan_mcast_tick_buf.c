/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Circular message buffer implementation. Besides messages, also ticks
 * that may service, for instance, a time-out mechanism.
 *
 * The implementation increases the size to a power of two for cheap MOD
 * operations.
 *
 * Interface:
 *	accept() to put a message at a certain number
 *	get() to delete the oldest message from the buffer
 *	ticks() to read/write the ticks field
 */

#include <stdlib.h>
#include <assert.h>

#include "pan_sys_msg.h"

#include "pan_mcast_tick_buf.h"




	/* Percentage at which buffer is considered nearly full */
int pan_tick_buf_nearly_full_perc = 60; /* 75; */


pan_msg_p
pan_tick_buf_do_get(pan_tick_buf_p buf)
{
    void *msg;
    pan_tick_msg_p elt;

    assert(buf->next != buf->last);

    elt = TICK_BUF(buf, buf->last);

    msg = elt->msg;
    elt->msg = NULL;

    ++buf->last;

    return msg;
}



pan_tick_buf_p
pan_tick_buf_create(int size)
{
    pan_tick_buf_p buf;
    int            i;
    unsigned int   two_power;

    buf = pan_malloc(sizeof(pan_tick_buf_t));

    two_power = 1;
    while (two_power < size) {
	two_power *= 2;
    }
    buf->buf   = pan_malloc(two_power * sizeof(pan_tick_msg_t));

    buf->size = two_power;
    for (i = 0; i < two_power; i++) {
	buf->buf[i].msg  = NULL;
	buf->buf[i].ticks = -1;
    }

    buf->last = 0;
    buf->next = 0;

    return buf;
}




int
pan_tick_buf_clear(pan_tick_buf_p buf, void (*msg_clear)(pan_msg_p msg))
{
    int        i;
    int        n;

    n = 0;
    for (i = 0; i < buf->size; i++) {
	if (buf->buf[i].msg != NULL) {
	    if (msg_clear != NULL) {
		msg_clear(buf->buf[i].msg);
	    }
	    ++n;
	}
    }

    pan_free(buf->buf);
    pan_free(buf->ticks);

    pan_free(buf);

    return n;
}


void
pan_tick_buf_accept(pan_tick_buf_p buf, pan_msg_p msg, int idx)
{
    register int next;
    register int upb;
    register int size_1;
    register pan_tick_msg_p msg_buf;
    pan_tick_msg_p elt;

    elt = TICK_BUF(buf, idx);

    assert(pan_tick_buf_in_range(buf, idx));
    assert(elt->msg == NULL);

    elt->msg  = msg;
    elt->ticks = -1;

					/* make buf->next consistent */
					/* Fold all buf fields into registers.
					 * For performance. Bleuh. */
    next = buf->next;
    upb  = buf->last + buf->size;
    msg_buf = buf->buf;
    size_1 = buf->size - 1;
    while (msg_buf[next & size_1].msg != NULL && next < upb) {
	next++;
    }
    buf->next = next;

    assert(buf->last <= buf->next);
}


void
pan_tick_buf_advance(pan_tick_buf_p buf)
{
    assert(buf->next == buf->last);

    buf->next++;
    buf->last++;
}
