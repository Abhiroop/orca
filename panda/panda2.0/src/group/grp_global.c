/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group initialisation and termination for stuff shared among all groups */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>

#include "pan_sys.h"

#include "pan_mp.h"

#include "grp_global.h"
#include "grp_sweep.h"
#include "grp_hist.h"
#include "grp_tab.h"
#include "grp_ttab.h"
#include "grp_stats.h"
#include "grp_send_rcve.h"

#include "grp_ns.h"

#define MIN_HIST_SIZE	(1 << 7)		/* Must be a power of 2 */



pan_nsap_p	pan_grp_data_nsap;
pan_nsap_p	pan_grp_cntrl_nsap;

pan_mutex_p	pan_grp_upcall_lock;

pan_pset_p	pan_grp_others;			/* Broadcast iso multicast */

int		pan_grp_me;			/* cache pan_my_pid() */
int		pan_grp_nr;			/* cache pan_nr_platforms() */


/* User settable parameters */
#ifdef BB_SIZE
int		pan_grp_bb_large		= BB_SIZE;
#else
int		pan_grp_bb_large		= 1250;
#endif

/* int		pan_grp_Leave_attempts		= 10; */
int		pan_grp_Leave_attempts		= 10;

int		pan_grp_seq_watch		= 4;
int		pan_grp_seq_suicide		= 16;


#ifdef BUFFER_SIZE

int		pan_grp_hist_size		= BUFFER_SIZE;
int		pan_grp_ord_buf_size		= BUFFER_SIZE;
int		pan_grp_home_ord_buf_size	= BUFFER_SIZE;

#else

int		pan_grp_hist_size		= MIN_HIST_SIZE;
int		pan_grp_ord_buf_size		= 128;
int		pan_grp_home_ord_buf_size	= 128;

#endif

int		pan_grp_fb_size;

pan_time_p	pan_grp_sweep_interval;

int		pan_grp_send_ticks;
int		pan_grp_sync_ticks;
int		pan_grp_retrans_ticks;


int
pan_grp_upround_twopower(int n)
{
    int n2;

    n2 = 1;
    while (n2 < n) {
	n2 *= 2;
    }

    return n2;
}


void
pan_group_va_set_params(void *dummy, ...)
{
    va_list args;
    char    *cmd;
    double   t_sweep;
    double   t;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if        (strcmp("PAN_GRP_bb_boundary",         cmd) == 0) {
	    pan_grp_bb_large            = va_arg(args, int);

	} else if (strcmp("PAN_GRP_leave_attempts",      cmd) == 0) {
	    pan_grp_Leave_attempts      = va_arg(args, int);

	} else if (strcmp("PAN_GRP_frag_buf_size",       cmd) == 0) {
	    pan_grp_fb_size       = va_arg(args, int);

	} else if (strcmp("PAN_GRP_ord_buf_size",        cmd) == 0) {
	    pan_grp_ord_buf_size        = va_arg(args, int);

	} else if (strcmp("PAN_GRP_hist_size",           cmd) == 0) {
	    pan_grp_hist_size           = va_arg(args, int);

	} else if (strcmp("PAN_GRP_hist_status",          cmd) == 0) {
	    pan_hist_send_status_perc = 100 * va_arg(args, double);

	} else if (strcmp("PAN_GRP_hist_watch",          cmd) == 0) {
	    pan_hist_nearly_full_perc = 100 * va_arg(args, double);

	} else if (strcmp("PAN_GRP_retrans_timeout",     cmd) == 0) {
	    t = va_arg(args, double);
	    t_sweep = pan_time_t2d(pan_grp_sweep_interval);
	    pan_grp_retrans_ticks = ceil(t / t_sweep);

	} else if (strcmp("PAN_GRP_send_timeout",        cmd) == 0) {
	    t = va_arg(args, double);
	    t_sweep = pan_time_t2d(pan_grp_sweep_interval);
	    pan_grp_send_ticks = ceil(t / t_sweep);

	} else if (strcmp("PAN_GRP_sync_timeout",        cmd) == 0) {
	    t = va_arg(args, double);
	    t_sweep = pan_time_t2d(pan_grp_sweep_interval);
	    pan_grp_sync_ticks = ceil(t / t_sweep);

	} else if (strcmp("PAN_GRP_watch_sync",          cmd) == 0) {
	    pan_grp_seq_watch           = va_arg(args, int);

	} else if (strcmp("PAN_GRP_watch_suicide",       cmd) == 0) {
	    pan_grp_seq_suicide         = va_arg(args, int);

	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    (void)va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}


void
pan_group_va_get_params(void *dummy, ...)
{
    va_list args;
    char    *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if        (strcmp("PAN_GRP_bb_boundary",         cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_grp_bb_large;

	} else if (strcmp("PAN_GRP_Leave_attempts",      cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_grp_Leave_attempts;

	} else if (strcmp("PAN_GRP_frag_buf_size",       cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_grp_fb_size;
	    pan_grp_fb_size = pan_grp_upround_twopower(pan_grp_fb_size);

	} else if (strcmp("PAN_GRP_ord_buf_size",        cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_grp_ord_buf_size;
	    pan_grp_ord_buf_size =
		pan_grp_upround_twopower(pan_grp_ord_buf_size);

	} else if (strcmp("PAN_GRP_hist_size",           cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_grp_hist_size;
	    pan_grp_hist_size = pan_grp_upround_twopower(pan_grp_hist_size);

	} else if (strcmp("PAN_GRP_hist_status",          cmd) == 0) {
	    *(double*)va_arg(args, double*) =
		pan_hist_send_status_perc / 100;

	} else if (strcmp("PAN_GRP_hist_watch",          cmd) == 0) {
	    *(double*)va_arg(args, double*) =
		pan_hist_nearly_full_perc / 100;

	} else if (strcmp("PAN_GRP_retrans_timeout",     cmd) == 0) {
	    *(double*)va_arg(args, double*) =
		pan_time_t2d(pan_grp_sweep_interval) * pan_grp_retrans_ticks;

	} else if (strcmp("PAN_GRP_send_timeout",        cmd) == 0) {
	    *(double*)va_arg(args, double*) =
		pan_time_t2d(pan_grp_sweep_interval) * pan_grp_send_ticks;

	} else if (strcmp("PAN_GRP_sync_timeout",        cmd) == 0) {
	    *(double*)va_arg(args, double*) =
	    	pan_time_t2d(pan_grp_sweep_interval) * pan_grp_sync_ticks;

	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    (void)va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}



void
pan_group_init(void)
{
/* Group module initialisation.
 * Initialise things that are global to all groups:
 *    Set group thread priorities.
 *    Initialise the name server.
 *    Start the global group receive stuff.
 */
    pan_grp_me = pan_my_pid();
    pan_grp_nr = pan_nr_platforms();

    pan_grp_sweep_interval = pan_time_create();
    pan_time_d2t(pan_grp_sweep_interval, 0.1);
#ifdef RANDOM_DISCARD
    pan_time_div(pan_grp_sweep_interval, 20);
#endif

			/* Interval between retransmits */
    pan_grp_send_ticks    = 10;
			/* Interval at which sequencer watchdog awakes */
    pan_grp_sync_ticks    = 10;
			/* time to request lost messages from sequencer */
    pan_grp_retrans_ticks = 10;

    pan_grp_hist_size = pan_grp_upround_twopower(8 * pan_nr_platforms());
    if (pan_grp_hist_size < MIN_HIST_SIZE) {
	pan_grp_hist_size = MIN_HIST_SIZE;
    }

    pan_grp_home_ord_buf_size =
	    pan_grp_upround_twopower((pan_grp_hist_size +
				      pan_grp_ord_buf_size) / 2);

    pan_grp_fb_size = pan_grp_upround_twopower(MAX_TTABLE_SLOTS);

    pan_grp_others = pan_pset_create();
    pan_pset_fill(pan_grp_others);
    pan_pset_del(pan_grp_others, pan_grp_me);

    pan_grp_upcall_lock = pan_mutex_create();

    pan_mp_init();
    pan_ns_init();

    pan_gtab_init(pan_grp_upcall_lock);
    init_group_module_stats();

    pan_grp_sweep_start(pan_grp_upcall_lock, pan_grp_sweep_interval);

    pan_grp_send_rcve_start();
}



void
pan_group_end(void)
{
    pan_grp_sweep_end();
    pan_time_clear(pan_grp_sweep_interval);

    pan_grp_send_rcve_end();

    pan_gtab_await_size(0);
    clear_group_module_stats();

    pan_gtab_clear();

    pan_ns_clear(TRUE);
    pan_mp_end();

    pan_pset_clear(pan_grp_others);

#ifndef UNSAFE_TO_CLEAR
    pan_mutex_clear(pan_grp_upcall_lock);
#endif
}
