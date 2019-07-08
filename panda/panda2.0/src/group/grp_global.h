/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Parameters and data structures shared between all groups.
 */

#ifndef _GROUP_GRP_GLOBAL_H_
#define _GROUP_GRP_GLOBAL_H_

#include "pan_sys.h"

#include "pan_group.h"		/* Consistency with the public prototypes */



				/* All group upcalls are protected by one
				 * lock, shared between all groups.
				 */
extern pan_mutex_p pan_grp_upcall_lock;


extern pan_nsap_p pan_grp_data_nsap;		/* Group data channel */
extern pan_nsap_p pan_grp_cntrl_nsap;		/* Group control channel */


extern pan_pset_p pan_grp_others;		/* Broadcast iso multicast */

extern int	pan_grp_me;			/* cache pan_my_pid() */
extern int	pan_grp_nr;			/* cache pan_nr_platforms() */

extern int	pan_grp_bb_large;		/* choice between PB/BB */

extern int	pan_grp_hist_size;		/* size of history */
extern int	pan_grp_ord_buf_size;		/* size of ord. frag buf */
extern int	pan_grp_home_ord_buf_size;	/* size of home ord. frag buf */
extern int	pan_grp_fb_size;		/* size of fragment buf */

extern pan_time_p	pan_grp_sweep_interval;	/* time between sweeps */

extern int	pan_grp_send_ticks;		/* timeouts in sweep ticks */
extern int	pan_grp_sync_ticks;
extern int	pan_grp_retrans_ticks;
extern int	pan_grp_watch_ticks;

extern int	pan_grp_Leave_attempts;		/* suicide after # leave att. */

extern int	pan_grp_seq_watch;		/* sync after # timeouts
						 * silence */
extern int	pan_grp_seq_suicide;		/* suicide after # timeouts
						 * silence */


int		pan_grp_upround_twopower(int n);

void		pan_group_init(void);
void		pan_group_end(void);

void            pan_group_va_set_params(void *dummy, ...);
void            pan_group_va_get_params(void *dummy, ...);

#endif
