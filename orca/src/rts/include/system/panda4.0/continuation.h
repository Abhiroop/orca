/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __continuation_h__
#define __continuation_h__

#include "orca_types.h"
#include "pan_sys.h"

#define CONT_KEEP    0  /* This continuation failed. Try again next time */
#define CONT_NEXT    1  /* This one succeeded. Remove it from the list   */
#define CONT_RESTART 2  /* This one succeeded, rerun all other remaining
			 * continuations on the list.
			 */

#define cont_pending(q)  ((q)->cq_contlist != (struct continuation *)0)

extern cont_queue_t cont_immediate_queue;

extern void cont_start(int inline_immediate_conts);

extern void cont_end(void);

extern void cont_init(cont_queue_t *q);

extern void cont_clear(cont_queue_t *q);

extern void *cont_alloc(cont_queue_t *q, int size, cont_func_p cont);

extern void cont_save(void *buf, int block_me);

extern void cont_resume(cont_queue_t *q);

#endif
