/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Parameters and data structures shared between all mcast groups.
 *
 * Exports:
 *
 * Types:
 *
 * Functions public for upper layer:
 *
 *	void pan_mcast_va_set_params(void *dummy, ...);
 *	void pan_mcast_va_get_params(void *dummy, ...);
 *
 * Functions public for this layer:
 *
 *	void pan_mcast_init(void);
 *	void pan_mcast_end(void);
 */

#ifndef _SYS_MCAST_GLOBAL_H_
#define _SYS_MCAST_GLOBAL_H_


#include "fm.h"			/* For type FM_mc_t */


#include "pan_sys_msg.h"

#include "pan_system.h"
#include "pan_comm.h"
#include "pan_nsap.h"

			/* Group structure: */
typedef struct PAN_SYS_MCAST_STATE_T pan_mcast_t, *pan_mcast_p;

#include "pan_mcast_stats.h"

#include "pan_mcast_buf.h"
#include "pan_mcast_tick_buf.h"

#include "pan_mcast_header.h"
#include "pan_mcast_memlist.h"


			/* Flags for the mcast state */

typedef enum MCAST_FLAGS_T {
    MCAST_send_sync	= (0x1) << 0,		/* sender */
    MCAST_rexmitting	= (0x1) << 1,		/* receiver */
    MCAST_syncing	= (0x1) << 3,		/* purge watchdog */
    MCAST_cleared	= (0x1) << 4,		/* sender, upcall */
    MCAST_done		= (0x1) << 5,		/* sender, upcall */
    MCAST_in_ordr	= (0x1) << 6		/* thread in ordr upcall */
} pan_mcast_flags_t, *pan_mcast_flags_p;


/* The multicast state.
 * All of the state is put into one struct, to allow easy upgrade to multiple
 * mcast groups if ever need be.
 *
 * The locking scheme is simple.
 * If a thread wants to access the mcast data structures, it must grab
 * pan_mcast_upcall_lock.
 *
 * This allows no concurrency in sending when an upcall is going on.
 */
struct PAN_SYS_MCAST_STATE_T {

    FM_mc_t		mcast_tag;		/* Tag of this mcast group ?? */
#ifndef RELIABLE_NETWORK
    int			purge_pe;		/* purge watchdog process */
#endif		/* RELIABLE_NETWORK */

				/* I. Shared between all threads */
#ifndef RELIABLE_NETWORK
    int			last_status;		/* last ack-ed seqno */
						/* RW sender receiver rexmit */
#endif		/* RELIABLE_NETWORK */

				/* II. shared between ordr and sender */
#ifndef RELIABLE_NETWORK
    pan_buf_p		hist;			/* history of "my" msgs */
						/* RW sender receiver */
    int			next_accept;		/* cache ord_buf->next */
						/* RW receiver R sender */
#endif		/* RELIABLE_NETWORK */

				/* III. shared between ordr and purge */
#ifndef RELIABLE_NETWORK
    int			global_next_accept;	/* worldwide next-to-be accepted
						 * seqno */
#endif		/* RELIABLE_NETWORK */

				/* IV. For ordr:
				 *     shared RW between upcall and daemon */
    pan_mcast_flags_t	ordr_flags;
#ifndef RELIABLE_NETWORK
    int			last_rexmit_request;	/* last rexmit request seqno */
#endif		/* RELIABLE_NETWORK */
					/* Message buffers */
    pan_tick_buf_p	ord_buf;		/* ordered msgs for this grp */
    pan_msg_p		ordr_queue;		/* continuation list */

				/* V. For purge pe:
				 *    shared RW between upcall and watchdog */
#ifndef RELIABLE_NETWORK
    pan_mcast_flags_t	purge_flags;
    int		       *seen_seqno;		/* Seq. records member status */
    int			global_min_seen;	/* global minimum seqno */
    int                 purge_sent;		/* remember last purged seqno */
    int                 watch_counter;		/* Check asynchronously every
						 * # purge_timeout ticks */
#endif		/* RELIABLE_NETWORK */

				/* Statistics: */
    pan_mcast_stat_p	stat;
    int		       *discard;
    int		       *discard_cleanup;
};




				/* All sys_mcast upcalls are protected by one
				 * lock.
				 * This is necessary to avoid a race at
				 * shut-down between multiple communication
				 * daemons. */
extern pan_mutex_p     pan_mcast_upcall_lock;

extern pan_pset_p      pan_mcast_the_world;	/* for sanity checks */

extern pan_mcast_t     pan_mcast_state;		/* the mcast state */

#ifndef RELIABLE_NETWORK
extern int             pan_mcast_retrans_ticks;	/* rexmit req after
							 * # ticks */

extern int             pan_mcast_watch_ticks;	/* sync after #watches silence
						 * from members */

extern int             pan_mcast_hist_size;	/* size of history */

extern int             pan_mcast_sweep_stack;	/* sweeper daemon stack size */
extern int             pan_mcast_sweep_prio;	/* sweeper daemon priority */

extern pan_time_p      pan_mcast_sweep_interval;
#endif		/* RELIABLE_NETWORK */

extern int             pan_mcast_ord_buf_size;	/* size of ord. msg buf */



/* Public prototypes */

void    pan_mcast_init(void);
void    pan_mcast_end(void);

int     pan_mcast_va_set_params(void *dummy, ...);
int     pan_mcast_va_get_params(void *dummy, ...);

void    pan_mcast_create(int purge_pe);

void    pan_mcast_clear(void);

void    pan_mcast_clear_data(void);

#endif
