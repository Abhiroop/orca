#ifndef __GROUP_GRP_H__
#define __GROUP_GRP_H__

#include "pan_sys_msg.h"
#include "pan_group.h"

#include "global.h"

#include "grp_stats.h"


#ifdef __GNUC__
#  define INLINE __inline__
#else
#  define INLINE
#endif

typedef enum GRP_FLAGS_T {
    GRP_JOINED		= 0x1 << 0
} grp_flags_t, *grp_flags_p;

typedef void (*grp_upcall_f)(pan_msg_p);


struct PAN_GROUP_T {
    group_id_t   gid;		/* globally unique group id */
    pan_cond_p   size_change;	/* signals changes in membership */
    int          nmembers;	/* number of members */
    pan_pset_p   members;	/* current group members */
    pan_thread_p rcv_id;	/* receiver thread id */
    grp_upcall_f receiver;	/* upcall function */
    int          clients;	/* #clients: ref count for clean up */
    grp_flags_t  flags;		/* e.g. GRP_JOINED */
#ifdef STATISTICS
    int          ticks[N_MSG_TICKS];
    int          discards[N_DSCRDS];
#endif
};


/* These two fields are read-only and can be freely read
 * WITHOUT locking.
 */
#define pan_grp_gid(grp)	  ((grp)->gid)   	/* debugging ONLY */
#define pan_grp_receiver(grp)   ((grp)->receiver)


pan_group_p pan_grp_create(group_id_t gid);

			/* Caller needs to hold pan_grp_lock */
void        pan_grp_register(pan_group_p grp, grp_upcall_f receiver);

			/* Caller needs to hold pan_grp_lock */
void        pan_grp_add_member(pan_group_p grp, int pid);

			/* Caller needs to hold pan_grp_lock */
int         pan_grp_del_member(pan_group_p grp, int pid);

#endif
