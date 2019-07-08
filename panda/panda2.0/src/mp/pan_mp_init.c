/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_mp.h"		/* Provides a module interface */
#include "pan_mp_init.h"
#include "pan_mp_state.h"
#include "pan_mp_conn.h"
#include "pan_mp_policy.h"
#include "pan_mp_ticks.h"

static int inits = 0;

/*
 * pan_mp_init:
 *                 Initializes the message passing module.
 */
void 
pan_mp_init(void)
{
    /* Don't start these modules in another order !! */

    if (inits++) {
	return;
    }

#ifndef RELIABLE_UNICAST
    pan_mp_ticks_start();
    pan_mp_conn_start();                              /* connection table */
#endif

    pan_mp_state_start();
}

/*
 * pan_mp_end:
 *                 Terminates the message passing module.     
 */
void 
pan_mp_end(void)
{
    if (--inits) {
	return;
    }

    pan_mp_state_end();

#ifndef RELIABLE_UNICAST
    pan_mp_conn_end();
    pan_mp_ticks_end();
#endif
}




