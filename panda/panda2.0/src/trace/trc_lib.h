/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Public declarations of the panda trace package.
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */


#ifndef _TRACE_TRC_LIB_H
#define _TRACE_TRC_LIB_H

#include "pan_sys.h"


#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"		/* For exported prototypes */

#include "trc_types.h"		/* Contains typedefs */


/* extern const trc_thread_id_t         trc_no_such_thread; */

#define trc_no_such_thread  {-1, -1}

#define trc_thread_id_eq(t1, t2) \
	(((t1).my_pid == (t2).my_pid)&& ((t1).my_thread == (t2).my_thread))

#define is_no_thread(t) \
	(((t).my_pid == -1)&& ((t).my_thread == -1))


#define IS_IMPLICIT_SRC(flag) \
			(! ((flag) & EXPLICIT_SRC))
#define IS_EXPLICIT_SRC(flag) \
			(((flag) & EXPLICIT_SRC)     == EXPLICIT_SRC)
#define IS_THREADS_MERGED(flag) \
			(((flag) & THREADS_MERGED)   == THREADS_MERGED)
#define IS_PLATFORMS_MERGED(flag) \
			(((flag) & PLATFORMS_MERGED) == PLATFORMS_MERGED)
#define STATE_COMES_FIRST(flag) \
			(((flag) & STATE_FIRST)      == STATE_FIRST)
#define IS_COMPACT_FORMAT(flag) \
			(((flag) & COMPACT_FORMAT)   == COMPACT_FORMAT)
#define IS_LITTLE_ENDIAN(flag) \
			(((flag) & LITTLE_ENDIAN)    == LITTLE_ENDIAN)



extern trc_event_t	EMB_CLOCK_SHIFT;
extern trc_event_t	CLOCK_SHIFT;
extern trc_event_t	FLUSH_BLOCK;


void              trc_init_state(trc_p trc);

trc_thread_info_p trc_thread_locate(trc_p trc, trc_thread_id_t thread_id);

int               trc_upshot_thread_id(trc_p trc, trc_thread_id_t thread_id);


#endif
