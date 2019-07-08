/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */



#ifndef _PAN_MCAST_MSG_BUF_H
#define _PAN_MCAST_MSG_BUF_H


#include <assert.h>

#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif



typedef struct PAN_TICK_BUF_T pan_tick_buf_t, *pan_tick_buf_p;



#ifdef RELIABLE_NETWORK



/* Linked list message buffer implementation: the buffer cannot be fixed size,
 * since there is no mechanism to cope with overflow.
 *
 * pan_tick_buf_last()	returns the lowest-numbered msg that has not yet
 *			been retrieved. This message need not have arrived,
 *			in which case NULL is returned.
 * Interface:
 * accept() at numbered entry, get() at buf_last().
 *
 * Shortcut for the common case that the buffer is empty, and the next-to-
 * be-expected msg arrives: advance().
 */


struct PAN_TICK_BUF_T {
    int             last;			/* next to be read */
    pan_msg_p       list;			/* linked list of msgs */
};




#else		/* RELIABLE_NETWORK */



/* Circular message buffer implementation.
 *
 * pan_tick_buf_last()	returns the lowest-numbered msg that has not yet
 *			been retrieved. This message need not have arrived,
 *			in which case NULL is returned.
 * Interface:
 * accept() at numbered entry, get() at buf_last().
 *
 * Shortcut for the common case that the buffer is empty, and the next-to-
 * be-expected msg arrives: advance().
 *
 * Extra field for msgs that are out of order: the ticks field. The
 * intermediate tick fields are set to a certain value when an out-of-order
 * msg arrives.
 * The sweeper decreases this tick field, and issues a retransmit request
 * when a tick field has become zero.
 *
 * The implementation increases the size to a power of two for cheap MOD
 * operations.
 */


typedef struct PAN_TICK_FRGM_T {
    pan_msg_p       msg;			/* the msg buffer */
    int             ticks;			/* the tick buffer */
} pan_tick_msg_t, *pan_tick_msg_p;


struct PAN_TICK_BUF_T {
    pan_tick_msg_p  buf;			/* the buffer */
    int             last;			/* next to be read */
    int             size;			/* size */
};


#endif		/* RELIABLE_NETWORK */


#ifndef STATIC_CI

int            pan_tick_buf_last_accept(pan_tick_buf_p buf)

pan_msg_p      pan_tick_buf_get(pan_tick_buf_p buf);

void           pan_tick_buf_accept(pan_tick_buf_p buf, pan_msg_p msg, int idx);

int 	       pan_tick_buf_advance(pan_tick_buf_p buf, int seqno);

/* Initialisation, termination */

pan_tick_buf_p pan_tick_buf_create(int size);

int            pan_tick_buf_clear(pan_tick_buf_p buf,
				  void (*msg_clear)(pan_msg_p msg));

#endif



#endif
