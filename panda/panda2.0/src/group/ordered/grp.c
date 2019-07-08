#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "global.h"
#include "grp.h"
#include "grp_stats.h"

#include <stdio.h>

pan_group_p
pan_grp_create(group_id_t gid)
{
    pan_group_p g;

    g = pan_malloc(sizeof(pan_group_t));

    g->gid = gid;		/* READ-ONLY */
    g->size_change = pan_cond_create(pan_grp_lock);
    g->nmembers = 0;
    g->members = pan_pset_create();
    g->receiver = NULL;
    g->clients = 1;
    g->flags   = 0;

    pan_grp_stats_init(g);

    return g;
}



void
pan_group_clear(pan_group_p grp)
{
    int clients;

    clients = --grp->clients;
    if (clients == 0) {
	pan_cond_clear(grp->size_change);
	pan_pset_clear(grp->members);
	pan_grp_stats_print(grp);
	pan_grp_stats_clear(grp);
	pan_free(grp);
    }
}


			/* Caller needs to hold pan_grp_lock */
void
pan_grp_register(pan_group_p grp, grp_upcall_f receiver)
{
    assert(!grp->receiver);

    grp->receiver = receiver;
    ++grp->clients;
}


			/* Caller needs to hold pan_grp_lock */
void
pan_grp_add_member(pan_group_p grp, int pid)
{

    pan_pset_add(grp->members, pid);
    grp->nmembers++;
    pan_cond_broadcast(grp->size_change);
}


			/* Caller needs to hold pan_grp_lock */
int
pan_grp_del_member(pan_group_p grp, int pid)
{
    int         sz;

    pan_pset_del(grp->members, pid);
    sz = --grp->nmembers;
    pan_cond_broadcast(grp->size_change);
    return sz;
}


void
pan_group_await_size(pan_group_p grp, int size)
{
    pan_mutex_lock(pan_grp_lock);
    while (grp->nmembers < size) {
	pan_cond_wait(grp->size_change);
    }
    pan_mutex_unlock(pan_grp_lock);
}


void
pan_group_va_set_params(void *dummy,...)
{
    va_list     args;
    char       *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (FALSE) {
	} else if (pan_my_pid() == 0) {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}


void
pan_group_va_get_params(void *dummy,...)
{
    va_list     args;
    char       *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (FALSE) {
	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}




void
pan_group_va_set_g_params(pan_group_p g,...)
{
    va_list     args;
    char       *cmd;

    va_start(args, g);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (FALSE) {
	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }
    va_end(args);
}


void
pan_group_va_get_g_params(pan_group_p g,...)
{
    va_list     args;
    char       *cmd;
    int        *i;
    char      **stats;

    va_start(args, g);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (strcmp("PAN_GRP_sequencer", cmd) == 0) {
	    i = va_arg(args, int*);
	    pan_sys_va_get_params(NULL, "PAN_SYS_mcast_sequencer", i, NULL);
	} else if (strcmp("PAN_GRP_statistics", cmd) == 0) {
	    stats = va_arg(args, char**);
	    pan_sys_va_get_params(NULL, "PAN_SYS_mcast_statistics", stats,
					NULL);
	} else if (strcmp("n_members", cmd) == 0) {
	    *(int *)va_arg(args, int *) = g->nmembers;
	} else {
	    printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }
    va_end(args);
}
