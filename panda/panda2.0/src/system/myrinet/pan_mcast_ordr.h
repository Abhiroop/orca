/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */



/*
 *  This module implements the orderer functionality: data msgs
 *  are received and delivered upwards in total order.
 *
 *    Mcast messages are received here; the network performs
 *    an upcall to deliver mcast msgs in the ordered buffer. The ordering is
 *    enforced with help of the sequence numbers. Out-of-order messages are
 *    buffered, retransmit of missing messages is broadcast.
 *
 *    Reliability:
 *    The sender stores its messages to serve possible retransmit requests.
 *    The msg copy is made via the network; this might be done more
 *    cheaply, BUT sender and receiver must synchronise anyway in order to
 *    send the next Panda-level msg. The synchronisation is combined
 *    with the copy action now.
 *
 *  This module exports the following user level functions:
 *      - <none>
 *
 *  It exports communication with the sender module:
 *	- some types
 */


#ifndef __PAN_MCAST_ORDR_H__
#define __PAN_MCAST_ORDR_H__


#include "pan_sys_msg.h"

#include "pan_mcast_header.h"


#ifndef STATIC_CI
#  define STATIC_CI static
#endif


STATIC_CI void pan_mcast_ordr_data_upcall(pan_msg_p msg, pan_mcast_hdr_p hdr);

#ifndef RELIABLE_NETWORK
STATIC_CI void pan_mcast_ordr_cntrl_upcall(pan_mcast_hdr_p hdr);
#endif		/* RELIABLE_NETWORK */

STATIC_CI void pan_mcast_ordr_init(void);
STATIC_CI void pan_mcast_ordr_end(void);


#endif
