#include "pan_sys_msg.h"

#include "pan_group.h"

#include "global.h"
#include "group_tab.h"
#include "header.h"
#include "join_leave.h"
#include "name_server.h"
#include "grp_stats.h"

/* Author: Raoul Bhoedjang, October 1993
 *
 * Data structures:
 * o group_tab: a table indexed by group id.
 *
 * Notes:
 * o Each node maintains information about all groups,
 *   even if it is not part of that group. This may
 *   seem stupid, but it simplifies the implementation and
 *   currently the number of groups tends to be _very_ small (1).
 *
 * o Join protocol:
 *   - a group can be created (locally)in two ways:
 *     1. local joiner creates it.
 *     2. incoming JOIN message handler creates it.
 *
 *   - the local joiner always registers the receive function;
 *     without it, no messages can be received.
 *
 *   - when a platform receives one of its own JOIN messages,
 *     it starts the corresponding group, i.e. it registers a
 *     receive function.
 *
 *   - this module is a monitor; the routines operate both on
 *     the group table and individual groups. It is necessary that
 *     that the routines as a _whole_ operate atomically, so locking
 *     individual groups and the group table is _not_ good enough.
 *
 *   - leave_id, leaves_completed, and leave_done form an event counter
 *     that is integrated with the monitor.
 *
 *   - I am not sure if there is any dangerous interference between
 *     module "send" and this module.
 */



static pan_pset_p  all;

static int         leave_id = 1;	 /* shared; protected by pan_grp_lock */
static int         leaves_completed = 0; /* shared; protected by pan_grp_lock */
static pan_cond_p  leave_done;



void
pan_grp_jl_start(void)
{
    all = pan_pset_create();
    pan_pset_fill(all);

    leave_done = pan_cond_create(pan_grp_lock);
    pan_gtab_start(pan_grp_lock);
}


void
pan_grp_jl_end(void)
{
    pan_gtab_await_empty();		/* wait until all groups have gone */

    pan_cond_clear(leave_done);
    pan_pset_clear(all);
}


void
pan_grp_handle_join(grp_hdr_p hdr, pan_msg_p msg)
{
    pan_group_p     grp;

    pan_mutex_lock(pan_grp_lock);	/* lock shared between send/jl etc */

    if (!(grp = pan_gtab_find(hdr->gid))) {
	grp = pan_grp_create(hdr->gid);		/* create group struct */
	pan_gtab_enter(hdr->gid, grp);		/* register it from outside */
    }

    STATINC(grp, GRP_ST_RCVE_JOIN);

    pan_grp_add_member(grp, hdr->sender);	/* register new member */
    if (hdr->sender == pan_grp_me) {
	grp->flags |= GRP_JOINED;
    }

    pan_mutex_unlock(pan_grp_lock);
}


pan_group_p
pan_group_join(char *name, void (*receive)(pan_msg_p))
{
    group_id_t     gid;
    pan_group_p    grp;
    grp_hdr_p      join_hdr;
    pan_msg_p      msg;

    gid = pan_ns_register(name);	/* get id from name server */

    /* If the group has already been registered from outside, get its
     * pointer from the group tab. Otherwise, create a new group and
     * register it from inside in the group tab. */

    pan_mutex_lock(pan_grp_lock);

    if (!(grp = pan_gtab_find(gid))) {	/* is there a temp. entry? */
	grp = pan_grp_create(gid);	/* No, create a group.     */
	pan_gtab_enter(gid, grp);	/* and enter it */
    }

					/* register upcall function */
    pan_grp_register(grp, receive);	/* upcall function */

    pan_mutex_unlock(pan_grp_lock);

    STATINC(grp, GRP_ST_SEND_JOIN);

    msg  = pan_msg_create();
    join_hdr = pan_msg_push(msg, sizeof(grp_hdr_t), alignof(grp_hdr_t));

    join_hdr->type   = G_JOIN;
    join_hdr->gid    = gid;
    join_hdr->sender = pan_grp_me;

    pan_comm_multicast_msg(all, msg, pan_grp_nsap);

    return grp;
}


void
pan_grp_handle_leave(grp_hdr_p hdr, pan_msg_p msg)
{
    pan_group_p     grp;

    pan_mutex_lock(pan_grp_lock);	/* lock shared between send/jl etc */

    if ((grp = pan_gtab_find(hdr->gid))) {
	int  sz;

	sz = pan_grp_del_member(grp, hdr->sender);	/* update member info */

	if (sz == 0) {		/* all members left */
	    pan_gtab_delete(pan_grp_gid(grp));	/* remove group */
	}
    }

    STATINC(grp, GRP_ST_RCVE_LEAVE);

    if (hdr->sender == pan_grp_me) {	/* leaving member is local */
	grp->flags &= ~GRP_JOINED;
	leaves_completed++;
	pan_cond_broadcast(leave_done);	/* tell leaving thread that  */
    }					/* its leave msg has arrived */

    pan_mutex_unlock(pan_grp_lock);
}


void
pan_group_leave(pan_group_p grp)
{
    int            my_leave_id;
    grp_hdr_p      leave_hdr;
    pan_msg_p      msg;

    STATINC(grp, GRP_ST_SEND_LEAVE);

    msg  = pan_msg_create();
    leave_hdr = pan_msg_push(msg, sizeof(grp_hdr_t), alignof(grp_hdr_t));

    pan_mutex_lock(pan_grp_lock);
    my_leave_id = leave_id++;	/* draw a ticket */
    pan_mutex_unlock(pan_grp_lock);	/* HACK by HPH */

    /* Note that we don't unregister with the name server.
     * It's easy to add on. */

    leave_hdr->type   = G_LEAVE;
    leave_hdr->gid    = grp->gid;
    leave_hdr->sender = pan_grp_me;

    pan_comm_multicast_msg(all, msg, pan_grp_nsap);

    pan_mutex_lock(pan_grp_lock);	/* HACK by HPH */
    while (leaves_completed < my_leave_id) { /* wait for leave msg to arrive */
	pan_cond_wait(leave_done);
    }
    pan_mutex_unlock(pan_grp_lock);
}
