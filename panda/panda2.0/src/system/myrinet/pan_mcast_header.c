/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Utility functions to push/pop/look a sys_mcast msg header
 */

#include <stdio.h>

#include "pan_sys_msg.h"

#include "pan_mcast_header.h"


#ifndef RELIABLE_NETWORK

char *
pan_mcast_msg_t2ascii(pan_mcast_msg_t tp)
{
    switch (tp) {
    case MCAST_GSB         : return "MCAST_GSB";
    case MCAST_REXMIT      : return "MCAST_REXMIT";
    case MCAST_REXMITREQ   : return "MCAST_REXMITREQ";
    case MCAST_STATUS      : return "MCAST_STATUS";
    case MCAST_PURGE       : return "MCAST_PURGE";
    case MCAST_SYNC        : return "MCAST_SYNC";
    }
    return NULL;		/*NOTREACHED*/
}

#endif		/* RELIABLE_NETWORK */

void
pan_mcast_hdr_print(pan_mcast_hdr_p hdr)
{
#ifndef RELIABLE_NETWORK
    printf("%s", pan_mcast_msg_t2ascii(hdr->type));
    printf("[seqno %d,sender %d]", hdr->seqno, hdr->sender);
#elif defined PANDA_MULTICASTS
    printf("[seqno %d,sender %d]", hdr->seqno, hdr->sender);
#else
    printf("[seqno %d]", hdr->seqno);
#endif
}
