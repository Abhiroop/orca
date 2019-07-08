#include "pan_sys.h"

#include "global.h"
#include "grp.h"
#include "send.ci"		/* include .c file for inlining */
#include "dispatch.ci"		/* include .c file for inlining */
#include "group_tab.h"
#include "header.h"
#include "join_leave.h"
#include "name_server.h"



/* Author: Raoul Bhoedjang, October 1993
 *
 * Group initialisation:
 *   initialises all modules in the group layer.
 *
 * Global:
 *    The nsaps for fragment and small group msgs are created/cleared.
 */

int		pan_grp_me;			/* Cache pan_my_pid() */
int		pan_grp_n_platforms;		/* Cache pan_nr_platforms */

pan_nsap_p	pan_grp_nsap;
pan_mutex_p	pan_grp_lock;


void
pan_group_init(void)
{
    pan_grp_me = pan_my_pid();
    pan_grp_n_platforms = pan_nr_platforms();

    pan_grp_lock = pan_mutex_create();

    pan_grp_nsap = pan_nsap_create();
    pan_nsap_fragment(pan_grp_nsap, frag_switch, sizeof(grp_hdr_t),
			PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);

    pan_hdr_start();
    pan_ns_start();
    pan_grp_snd_start();
    pan_grp_jl_start();
    pan_grp_disp_start();
}


void
pan_group_end(void)
{
    pan_grp_jl_end();
    pan_grp_snd_end();
    pan_ns_end();
    pan_grp_disp_end();
    pan_hdr_end();

    pan_nsap_clear(pan_grp_nsap);

    pan_mutex_clear(pan_grp_lock );
}
