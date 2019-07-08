/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#ifndef __PAN_MCAST_PURGE_H__
#define __PAN_MCAST_PURGE_H__


#ifndef RELIABLE_NETWORK


/*
 * This module implements the one centralized purge watchdog.
 *
 * Idea:
 * Look repeatedly whether the acknowledged seen_seqnos have arrived at the
 * next value that validates a PURGE multicast.
 * Also, watch out that members are not too far behind with notification of
 * their status: request their status if they take too long in reporting it
 * of their own initiative.
 */




#include "fm.h"				/* Fast Messages for Myrinet */

#include "pan_sys_msg.h"

#include "pan_mcast_hdr.h"

#ifndef STATIC_CI
#  define STATIC_CI static
#endif


STATIC_CI void pan_mcast_purge_upcall(pan_mcast_hdr_p cntrl_msg);

STATIC_CI void pan_mcast_purge_init(void);
STATIC_CI void pan_mcast_purge_end(void);

#endif		/* RELIABLE_NETWORK */


#endif
