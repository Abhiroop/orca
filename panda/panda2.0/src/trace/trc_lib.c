/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <stddef.h>

#include "pan_util.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"
#include "trc_lib.h"


/* const trc_thread_id_t trc_no_such_thread = {-1, -1}; */



const int       TRC_NO_SUCH_THREAD = -1;
trc_event_t	CLOCK_SHIFT;
trc_event_t	EMB_CLOCK_SHIFT;		/* clock time is embedded
						 * in the explicit/compact
						 * event struct */
trc_event_t	FLUSH_BLOCK;



void
trc_init_state(trc_p trc)
{
    trc->in_buf.buf = NULL;
    trc->in_buf.current = NULL;
    trc->in_buf.size = 0;
    trc->in_buf.next = NULL;
    trc->in_buf.stream = NULL;

    trc->out_buf.buf = NULL;
    trc->out_buf.current = NULL;
    trc->out_buf.size = 0;
    trc->out_buf.next = NULL;
    trc->out_buf.stream = NULL;

    trc->n_threads = 0;
    trc->thread = NULL;

    trc->out_buf.format = 0;
    if (! pan_is_bigendian()) {
	trc->out_buf.format |= LITTLE_ENDIAN;
    }
}



int
trc_upshot_thread_id(trc_p state, trc_thread_id_t thread_id)
{
    trc_thread_info_p thr;

    thr = state->thread[thread_id.my_pid].threads;
    while (thr != NULL) {
	if (thr->thread_id.my_thread == thread_id.my_thread) {
	    return thr->upshot_id;
	}
	thr = thr->next_thread;
    }
    return TRC_NO_SUCH_THREAD;
}


trc_thread_info_p
trc_thread_locate(trc_p trc, trc_thread_id_t thread_id)
{
    trc_thread_info_p p;

    p = trc->thread[thread_id.my_pid].threads;
    while (p != NULL) {
	if (p->thread_id.my_thread == thread_id.my_thread) {
	    return p;
	}
	p = p->next_thread;
    }
    return NULL;
}
