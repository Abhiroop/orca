/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "fm.h"				/* Fast Messages for Myrinet */

#include "pan_sys_msg.h"

#include "pan_system.h"
#include "pan_sync.h"			/* Use macros */

#include "pan_global.h"

#include "pan_comm.h"

#include "pan_msg_hdr.h"

#include "pan_mcast_bitset.h"
#include "pan_mcast_memlist.h"
#include "pan_mcast_global.h"
#include "pan_mcast_rcve.h"
#include "pan_mcast_ordr.h"
#include "pan_mcast_send.h"
#include "pan_mcast_header.h"

#include "pan_mcast_send_rcve.h"

#include "pan_timer.h"

#include "pan_trace.h"



/*
 * Global variables
 */


static pan_timer_p fm_latency_timer;
static pan_timer_p panda_timer;
static pan_timer_p poll_timer;





/*
 * Include the .c files: allows inline substitution of functions
 */


#include "pan_mcast_ordr.ci"
#include "pan_mcast_purge.ci"
#include "pan_msg_hdr.ci"
#include "pan_mcast_rcve.ci"
#include "pan_mcast_send.ci"



void
pan_mcast_send_init(void)
{
    fm_latency_timer = pan_timer_create();
    panda_timer = pan_timer_create();
    poll_timer = pan_timer_create();

#ifndef RELIABLE_NETWORK
    if (pan_mcast_state.purge_pe == pan_my_pid()) {
	pan_mcast_purge_init();
    }
#endif		/* RELIABLE_NETWORK */
    pan_mcast_ordr_init();
    pan_mcast_rcve_init();
    pan_mcast_do_send_init();
}


void
pan_mcast_send_end(void)
{
    pan_mcast_do_send_end();
    pan_mcast_rcve_end();
    pan_mcast_ordr_end();
#ifndef RELIABLE_NETWORK
    if (pan_mcast_state.purge_pe == pan_my_pid()) {
	pan_mcast_purge_end();
    }
#endif		/* RELIABLE_NETWORK */

    pan_timer_print(fm_latency_timer, "FM network");
    pan_timer_print(panda_timer, "Panda");
    pan_timer_print(poll_timer, "poll");

    pan_timer_clear(fm_latency_timer);
    pan_timer_clear(panda_timer);
    pan_timer_clear(poll_timer);
}
