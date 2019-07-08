#ifndef __PAN_SYS_MSG_COUNTER_H__
#define __PAN_SYS_MSG_COUNTER_H__


#include <sys/sem.h>

#include "pan_error.h"


/* this is not a pool of messages, but some administration for
    managing quotas of messages */


typedef struct msg_counter_struct {
    int         total_nr;	/* size of the 'quota' */
    int         max_used;	/* maximum usage */
    Semaphore_t sema;
    char       *name;		/* name of the 'quota' */
} pan_msg_counter_t, *pan_msg_counter_p;


		/* try to consume a message from the quota,
		 * fail if none available */
#define pan_msg_counter_try_hit(x,mc) \
	do { \
	    x = TestWait(&((mc)->sema)); \
	    if ((mc)->sema.Count < (mc)->max_used) \
		(mc)->max_used = (mc)->sema.Count; \
	} while (0)

		/* consume a message from the quota, error if none available */
#define pan_msg_counter_hit(mc) \
	do { \
	    if (!TestWait(&((mc)->sema))) \
		pan_sys_printf("(hit) %s: %d\n", (mc)->name, (mc)->sema.Count); \
	    if ((mc)->sema.Count < (mc)->max_used) \
		(mc)->max_used = (mc)->sema.Count; \
	} while (0)

		/* consume a message from the quota, block till one available */
#define pan_msg_counter_wait_hit(mc) \
	do { \
	    Wait(&((mc)->sema)); \
	    if ((mc)->sema.Count < (mc)->max_used) \
		(mc)->max_used = (mc)->sema.Count; \
	} while (0)

		/* give a message back to a quota */
#define pan_msg_counter_relax(mc) \
	do { \
	    Signal(&((mc)->sema)); \
	} while (0)

		/* give a message back to a quota,
		 * check for blocked consumers */
#define pan_msg_counter_signal_relax(mc) \
	do { \
	    Signal(&((mc)->sema)); \
	} while (0)


void pan_msg_counter_init(pan_msg_counter_p counter, int nr, char *name);

void pan_msg_counter_clear(pan_msg_counter_p counter);


#endif
