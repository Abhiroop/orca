/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the sweeper for the group.
 *
 *  Clients may register function+arg, which are called regularly.
 *  One lock protects all. A pointer to this lock is passed to this module
 *  in the init call.
 *  During the function call, this lock is held. If the function does an action
 *  for which the lock must be opened (such as communication), it must itself
 *  release the lock.
 */


#include <assert.h>
#include <stdlib.h>

#include "pan_sys.h"

#include "grp_types.h"
#include "grp_sweep.h"

#include "pan_trace.h"



/*
 * Global variables.
 */

#define N_SWEEP_INCR		10

static pan_mutex_p           sweep_lock;
static pan_cond_p            sweep_finished;
static pan_time_p            sweep_interval;
static pan_thread_p          sweep_thread;
static int                   sweep_done;

typedef struct SWEEP_T {
    int				max_ticks;
    int				ticks;
    pan_grp_sweep_func_p	f;
    void		       *arg;
} sweep_t, *sweep_p;


static int                   max_n_sweep	= 0;
static int                   n_sweep		= 0;

static sweep_p               sweep	= NULL;

/*
 * Local types
 */


void
pan_grp_sweep_register(pan_grp_sweep_func_p f, void *arg, int ticks)
{
    pan_mutex_lock(sweep_lock);

    if (n_sweep == max_n_sweep) {
	max_n_sweep += N_SWEEP_INCR;
	sweep        = pan_realloc(sweep, max_n_sweep * sizeof(sweep_t));
    }

    if (ticks <= 0) {
	pan_panic("%2d: sweep ticks should be > 0\n");
    }

    sweep[n_sweep].f         = f;
    sweep[n_sweep].arg       = arg;
    sweep[n_sweep].max_ticks = ticks;
    sweep[n_sweep].ticks     = ticks;
    ++n_sweep;

    pan_mutex_unlock(sweep_lock);
}



/* The sweeper thread.
 * Repeatedly:
 *  - call all registered functions with their registered argument.
 */
/*ARGSUSED*/
static void
pan_grp_sweeper(void *args)
{
    pan_time_p       timeout = pan_time_create();
    int              i;

    trc_new_thread(0, "sweeper");

    pan_mutex_lock(sweep_lock);

    if (! sweep_done) {
	while (TRUE) {

	    pan_time_get(timeout);
	    pan_time_add(timeout, sweep_interval);
	    pan_cond_timedwait(sweep_finished, timeout);

	    if (sweep_done) {
		break;
	    }

	    for (i = 0; i < n_sweep; i++) {
		--sweep[i].ticks;
		if (sweep[i].ticks == 0) {
		    sweep[i].ticks = sweep[i].max_ticks;
		    sweep[i].f(sweep[i].arg);
		}
	    }

	}
    }

    pan_mutex_unlock(sweep_lock);

    pan_time_clear(timeout);

    pan_thread_exit();
}




void
pan_grp_sweep_start(pan_mutex_p lock, pan_time_p interval)
{
    sweep_lock     = lock;
    sweep_done     = FALSE;
    sweep_finished = pan_cond_create(sweep_lock);
    sweep_interval = pan_time_create();
    pan_time_copy(sweep_interval, interval);

				/* Start sweeper thread */
    sweep_thread   = pan_thread_create(pan_grp_sweeper, NULL, 0, pan_thread_maxprio(), 0);
}



void
pan_grp_sweep_end(void)
{
    pan_mutex_lock(sweep_lock);
    sweep_done = TRUE;
    pan_cond_signal(sweep_finished);
    pan_mutex_unlock(sweep_lock);

				/* Await termination of the ordr daemon. */
    pan_thread_join(sweep_thread);
    pan_thread_clear(sweep_thread);

    pan_free(sweep);
    sweep       = NULL;
    max_n_sweep = 0;
    n_sweep     = 0;

    pan_cond_clear(sweep_finished);
    pan_time_clear(sweep_interval);
}
