/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#ifndef __PAN_MCAST_SEND_H__
#define __PAN_MCAST_SEND_H__


/*
 *  This module implements the send stuff of mcast communication.
 * It exports the following functions:
 *	- pan_mcast_send()
 *
 * Send msgs GSB style. The sender keeps a copy of the message for
 * reliability purposes. Members who did not receive the broadcast msgs
 * can request their retransmission, via a broadcast or via a point to point
 * retransmit request. This is all of the reliability of data messages: the
 * sender just broadcasts unreliably.
 * The history of sent messages is purged on initiative from the purge process,
 * that broadcasts MCAST_PURGE messages.
 * The sequence number is fetched with a system level call. A time-out
 * mechanism makes it reliable.
 *
 * The code does not implement a protocol to recover from processor failures.
 * However, the network is made reliable, the protocol copes with lost network
 * packets.
 */


#ifndef STATIC_CI
#  define STATIC_CI static
#endif



#ifndef STATIC_CI

void pan_mcast_do_send_init(void);
void pan_mcast_do_send_end(void);

#endif

#endif
