/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group struct initialisation/clearing */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pan_sys.h"

#include "grp_global.h"
#include "grp_group.h"
#include "grp_header.h"
#include "grp_tab.h"
#include "grp_stats.h"
#include "grp_send_rcve.h"


pan_group_p
pan_group_create(char *name, void (*rcve)(pan_msg_p msg), int gid,
		 int sequencer)
{
    int         len;
    pan_group_p g = pan_malloc(sizeof(pan_group_t));

    g->gid              = gid;
    g->seq              = sequencer;
    g->n_members        = 0;
    g->is_rexmitting    = FALSE;
    g->is_joining       = TRUE;
    g->is_in_suicide    = FALSE;
    g->is_seq_alive     = TRUE;
    g->is_syncing       = FALSE;
    g->is_done          = FALSE;
    g->is_cleared       = FALSE;
    g->is_want_to_leave = FALSE;
    g->is_ordr_done     = FALSE;
    g->is_ordr_busy     = FALSE;

#ifdef STATISTICS
    g->stat            = init_grp_stats();
    g->discard         = init_grp_discards();
    g->discard_cleanup = init_grp_discards();
#endif

    g->group_cleared = pan_cond_create(pan_grp_upcall_lock);
    g->group_size_changed = pan_cond_create(pan_grp_upcall_lock);

    len = strlen(name);
    g->name = pan_malloc(len + 1);
    strcpy(g->name, name);

    pan_grp_init_send_rcve(g, rcve);

    pan_gtab_add(g, gid);

    return g;
}



void
pan_group_clear_data(pan_group_p g)
{
    pan_grp_clear_send_rcve(g);

    pan_free(g->name);

#ifdef STATISTICS
    clear_grp_stats(g->stat);
    clear_grp_discards(g->discard);
    clear_grp_discards(g->discard_cleanup);
#endif
}


void
pan_group_clear(pan_group_p g)
{
    grp_hdr_t suicide;
    pan_time_p timeout = pan_time_create();

    suicide.type   = GRP_SUICIDE;
    suicide.gid    = g->gid;
    suicide.sender = pan_grp_me;

    do {
	pan_comm_unicast_small(pan_grp_me, pan_grp_cntrl_nsap, &suicide);
	pan_time_get(timeout);
	pan_time_add(timeout, pan_grp_sweep_interval);
	pan_mutex_lock(pan_grp_upcall_lock);
	if (g->is_cleared) {
	    break;
	}
	pan_cond_timedwait(g->group_cleared, timeout);
	pan_mutex_unlock(pan_grp_upcall_lock);
    } while (TRUE);

    pan_mutex_unlock(pan_grp_upcall_lock);

    pan_group_clear_data(g);

    pan_cond_clear(g->group_size_changed);
    pan_cond_clear(g->group_cleared);
    pan_free(g);
    pan_time_clear(timeout);
}


void
pan_group_await_size(pan_group_p g, int n)
{
    pan_mutex_lock(pan_grp_upcall_lock);
    assert(g == pan_gtab_locate(g->gid));
    while (g->n_members < n) {
	pan_cond_wait(g->group_size_changed);
    }
    pan_mutex_unlock(pan_grp_upcall_lock);
}


void
pan_group_va_set_g_params(pan_group_p g, ...)
{
    va_list args;
    char    *cmd;

    va_start(args, g);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (FALSE) {
	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    (void)va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}


void
pan_group_va_get_g_params(pan_group_p g, ...)
{
    va_list args;
    char    *cmd;
    int     *i;
#ifdef STATISTICS
    char    *stat_buf;
    char    *discard_buf;
    char    *buf;
    char   **buf_p;
#endif

    va_start(args, g);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if        (strcmp("PAN_GRP_sequencer",           cmd) == 0) {
	    i = (int*)va_arg(args, int*);
	    *i = g->seq;
	} else if (strcmp("PAN_GRP_n_members",           cmd) == 0) {
	    pan_mutex_lock(pan_grp_upcall_lock);
	    *(int*)va_arg(args, int*) = g->n_members;
	    pan_mutex_unlock(pan_grp_upcall_lock);
#ifdef STATISTICS
	} else if (strcmp("PAN_GRP_statistics",          cmd) == 0) {
	    stat_buf = pan_group_sprint_stats(g->stat);
	    discard_buf = pan_group_sprint_discards(g->discard,
						    g->discard_cleanup);
	    buf = pan_malloc(strlen(stat_buf) + strlen(discard_buf) + 1);
	    strcpy(buf, stat_buf);
	    strcat(buf, discard_buf);
	    pan_free(stat_buf);
	    pan_free(discard_buf);
	    if (g->seq == pan_grp_me) {
		stat_buf = pan_group_sprint_seq_stats(g->stat);
		discard_buf = pan_group_sprint_seq_discards(g->discard,
							    g->discard_cleanup);
		buf = pan_realloc(buf, strlen(buf) + strlen(stat_buf) +
				       strlen(discard_buf) + 1);
		strcat(buf, stat_buf);
		strcat(buf, discard_buf);
		pan_free(stat_buf);
		pan_free(discard_buf);
	    }
	    buf_p = va_arg(args, char**);
	    *buf_p = buf;
#endif
	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    (void)va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}
