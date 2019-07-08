/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Bcast initialisation and termination for stuff shared among all mcast
 * groups */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include "pan_sys_msg.h"

#include "pan_system.h"

#include "pan_global.h"

#include "pan_mcast_buf.h"
#include "pan_mcast_global.h"
#include "pan_mcast_sweep.h"
#include "pan_mcast_send_rcve.h"
#include "pan_mcast_stats.h"



pan_mutex_p         pan_mcast_upcall_lock;	/* global lock */

pan_pset_p          pan_mcast_the_world;	/* for sanity checks */

pan_mcast_t         pan_mcast_state;


#ifdef BUFFER_SIZE
#ifndef RELIABLE_NETWORK
int                 pan_mcast_hist_size     = BUFFER_SIZE;
#endif		/* RELIABLE_NETWORK */
int                 pan_mcast_ord_buf_size  = BUFFER_SIZE;
#else
#ifndef RELIABLE_NETWORK
int                 pan_mcast_hist_size     = 256;
#endif		/* RELIABLE_NETWORK */
int                 pan_mcast_ord_buf_size  = -1;
#endif

#ifndef RELIABLE_NETWORK
int                 pan_mcast_retrans_ticks = 3;
int                 pan_mcast_watch_ticks   = 8;

int                 pan_mcast_sweep_stack;
int                 pan_mcast_sweep_prio;

pan_time_p          pan_mcast_sweep_interval;
#endif		/* RELIABLE_NETWORK */



int
pan_mcast_va_set_params(void *dummy, ...)
{
    va_list  args;
    char    *cmd;
    int      ok = TRUE;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (strcmp("PAN_SYS_mcast_ord_buf_size",        cmd) == 0) {
	    pan_mcast_ord_buf_size        = va_arg(args, int);

#ifndef RELIABLE_NETWORK
	} else if (strcmp("PAN_SYS_mcast_hist_size",           cmd) == 0) {
	    pan_mcast_hist_size           = va_arg(args, int);

	} else if (strcmp("PAN_SYS_mcast_retrans_ticks",     cmd) == 0) {
	    pan_mcast_retrans_ticks       = va_arg(args, int);

	} else if (strcmp("PAN_SYS_mcast_sweep_interval",    cmd) == 0) {
	    pan_time_d2t(pan_mcast_sweep_interval, va_arg(args, double));
#endif		/* RELIABLE_NETWORK */

	} else {
	    printf("No such mcast value tag: \"%s\" -- ignored\n", cmd);
	    ok = FALSE;
	    va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);

    return ok;
}


int
pan_mcast_va_get_params(void *dummy, ...)
{
    va_list  args;
    char    *cmd;
    int      ok = TRUE;
#ifdef STATISTICS
    char    *stat_buf;
    char    *discard_buf;
    char    *buf;
    char   **buf_p;
    int      len;
#endif

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (strcmp("PAN_SYS_mcast_ord_buf_size",           cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_mcast_ord_buf_size;

#ifndef RELIABLE_NETWORK
	} else if (strcmp("PAN_SYS_mcast_hist_size",       cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_mcast_hist_size;

	} else if (strcmp("PAN_SYS_mcast_retrans_ticks",   cmd) == 0) {
	    *(int*)va_arg(args, int*) = pan_mcast_retrans_ticks;

	} else if (strcmp("PAN_SYS_mcast_sweep_interval", cmd) == 0) {
	    *(double*)va_arg(args, double*) =
		pan_time_t2d(pan_mcast_sweep_interval);
#endif		/* RELIABLE_NETWORK */

#ifdef STATISTICS
	} else if (strcmp("PAN_SYS_mcast_statistics",      cmd) == 0) {
	    stat_buf = pan_mcast_sprint_stats(pan_mcast_state.stat);
	    discard_buf = pan_mcast_sprint_discards(pan_mcast_state.discard,
					    pan_mcast_state.discard_cleanup);
	    len = strlen(stat_buf) + strlen(discard_buf) + 1;
	    buf = pan_malloc(len);
	    strcpy(buf, stat_buf);
	    strcat(buf, discard_buf);
	    pan_free(stat_buf);
	    pan_free(discard_buf);
#ifndef RELIABLE_NETWORK
	    if (pan_mcast_state.purge_pe == pan_my_pid()) {
		stat_buf = pan_mcast_sprint_purge_stats(pan_mcast_state.stat);
		len += strlen(stat_buf);
		buf = pan_realloc(buf, len);
		strcat(buf, stat_buf);
		pan_free(stat_buf);
	    }
#endif		/* RELIABLE_NETWORK */
	    buf_p = va_arg(args, char **);
	    *buf_p = buf;
#endif
	} else {
	    printf("No such mcast value tag: \"%s\" -- ignored\n", cmd);
	    ok = FALSE;
	    va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);

    return ok;
}


static void
pan_mcast_state_create(void)
{
#ifndef RELIABLE_NETWORK
    pan_mcast_state.next_accept          = 0;
    pan_mcast_state.purge_pe             = 0;
    pan_mcast_state.last_status          = -1;
    pan_mcast_state.global_next_accept   = 0;

    pan_mcast_state.hist                 = pan_buf_create(pan_mcast_hist_size,
							 pan_mcast_upcall_lock);
#endif		/* RELIABLE_NETWORK */

#ifdef STATISTICS
    pan_mcast_state.stat                 = pan_mcast_stats_create();
    pan_mcast_state.discard              = pan_mcast_discards_create();
    pan_mcast_state.discard_cleanup      = pan_mcast_discards_create();
#endif
}


static void
pan_mcast_state_clear(void)
{
#ifndef RELIABLE_NETWORK
    int n;

    n = pan_buf_clear(pan_mcast_state.hist, pan_msg_clear);
				STATDISCARD_CLEANUP(DX_HIST_B, n);
#endif		/* RELIABLE_NETWORK */

#if (defined STATISTICS) && (defined SAFE_TO_CLEAR)
    pan_mcast_stats_clear(pan_mcast_state.stat);
    pan_mcast_discards_clear(pan_mcast_state.discard);
    pan_mcast_discards_clear(pan_mcast_state.discard_cleanup);
#endif
}



void
pan_mcast_init(void)
{
/* Group module initialisation.
 * Initialise things that are global to all mcast groups:
 *    Set mcast thread priorities.
 *    Initialise the name server.
 *    Start the global mcast receive stuff.
 */

#ifndef RELIABLE_NETWORK
    if (pan_mcast_ord_buf_size == -1) {
	pan_mcast_ord_buf_size = MAX(512, pan_mcast_hist_size *
					  pan_nr_platforms() / 4);
    }
#else		/* RELIABLE_NETWORK */
    if (pan_mcast_ord_buf_size == -1) {
	pan_mcast_ord_buf_size = MAX(512, 32 * pan_nr_platforms());
    }
#endif		/* RELIABLE_NETWORK */

#ifdef STATISTICS
    pan_mcast_stats_init();
#endif

    pan_mcast_the_world = pan_pset_create();
    pan_pset_fill(pan_mcast_the_world);
    pan_mcast_upcall_lock = pan_mutex_create();

    pan_mcast_state_create();

#ifndef RELIABLE_NETWORK
    pan_mcast_sweep_interval  = pan_time_create();
    pan_time_d2t(pan_mcast_sweep_interval, 0.1);

    pan_mcast_sweep_stack = 0;
    if (pan_thread_maxprio() <= pan_thread_minprio() + 2) {
	pan_mcast_sweep_prio = pan_thread_maxprio();
    } else {
	pan_mcast_sweep_prio = pan_thread_maxprio() - 1;
    }

    pan_mcast_sweep_init(pan_mcast_upcall_lock, pan_mcast_sweep_interval,
			 pan_mcast_sweep_stack, pan_mcast_sweep_prio);
#endif		/* RELIABLE_NETWORK */

    pan_mcast_send_init();

#ifdef RANDOM_DISCARD
    srandom(pan_my_pid());
#endif
}



void
pan_mcast_end(void)
{
#ifndef RELIABLE_NETWORK
    pan_mcast_sweep_end();

    pan_time_clear(pan_mcast_sweep_interval);
#endif		/* RELIABLE_NETWORK */

    pan_mcast_send_end();

    pan_mcast_state_clear();

#if (defined STATISTICS) && (defined SAFE_TO_CLEAR)
    pan_mcast_stats_end();
#endif

#ifdef SAFE_TO_CLEAR
    pan_mutex_clear(pan_mcast_upcall_lock);
#endif
}
