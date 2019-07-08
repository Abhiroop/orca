/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __POLICY_H__
#define __POLICY_H__

#define CONN_NR_SEQNO     512 /* must be a multiple of 8 */
#define RETRY_TIMEOUT     10
#define ACK_TIMEOUT       5

#ifdef RELIABLE_UNICAST
#define SEND_DAEMON
#endif

#endif /* __POLICY_H__ */
