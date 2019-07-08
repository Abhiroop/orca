/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Definition of sys_mcast message header layout. */

#ifndef _SYS_MCAST_HEADER_H
#define _SYS_MCAST_HEADER_H


#include "pan_sys_msg.h"


#ifndef RELIABLE_NETWORK

typedef enum MCAST_MSG_T {
    MCAST_GSB,			/* member mcasts GSD data msg */
    MCAST_REXMIT,		/* member mcasts GSB rexmit reply */
    MCAST_REXMITREQ,		/* member mcasts GSB rexmit request */
    MCAST_STATUS,		/* member sends status to purge_pe */
    MCAST_PURGE,		/* purge_pe mcasts purge request */
    MCAST_SYNC			/* purge_pe mcasts purge + status request */
}   pan_mcast_msg_t, *pan_mcast_msg_p;

#endif		/* RELIABLE_NETWORK */


typedef struct MCAST_HDR_T pan_mcast_hdr_t, *pan_mcast_hdr_p;

struct MCAST_HDR_T {
#ifndef RELIABLE_NETWORK
    unsigned char	type;		/* Mcast type */
    short int		sender;		/* Sender */
    int			seqno;		/* This data msg's seqno */
    int			seen_seqno;	/* Piggy-back seen seqno */
#elif (defined PANDA_MULTICASTS)
    int			seqno;		/* This data msg's seqno */
    short int		sender;		/* Sender */
#else
    int			seqno;		/* This data msg's seqno */
#endif
    short int		nsap;		/* The nsap id encoded here for size
					 * efficiency */
};



/* Print of sys_mcast message */

#ifndef RELIABLE_NETWORK
char            *pan_mcast_msg_t2ascii(pan_mcast_msg_t tp);
#endif		/* RELIABLE_NETWORK */

void             pan_mcast_hdr_print(pan_mcast_hdr_p hdr);

#endif
