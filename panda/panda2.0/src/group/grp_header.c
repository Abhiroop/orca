/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Utility functions to push/pop/look a group msg header
 */

#include <stdio.h>

#include "pan_sys.h"

#include "grp_header.h"


#define UPMOD(x,y) (((x) + (y) - 1) % (y))





char     *pan_grp_msg_t2ascii(grp_msg_t tp)
{
    switch (tp) {
    case GRP_JOINREQ      : return "GRP_JOINREQ";
    case GRP_JOIN         : return "GRP_JOIN";
    case GRP_LEAVEREQ     : return "GRP_LEAVEREQ";
    case GRP_LEAVE        : return "GRP_LEAVE";
    case GRP_PB_REQ       : return "GRP_PB_REQ";
    case GRP_PB_ACPT      : return "GRP_PB_ACPT";
    case GRP_BB_REQ       : return "GRP_BB_REQ";
    case GRP_BB_ACPT      : return "GRP_BB_ACPT";
    case GRP_RETRANS      : return "GRP_RETRANS";
    case GRP_SYNC         : return "GRP_SYNC";
    case GRP_STATUS       : return "GRP_STATUS";
    case GRP_SUICIDE      : return "GRP_SUICIDE";
    }
    return NULL;		/*NOTREACHED*/
}


void            pan_grp_hdr_print(grp_hdr_p hdr)
{
    printf("%s", pan_grp_msg_t2ascii(hdr->type));
    printf("[seqno %d,sender %d,messid %d]", hdr->seqno,
	   hdr->sender, hdr->messid);
}
