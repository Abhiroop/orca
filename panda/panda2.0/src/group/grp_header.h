/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Definition of group message header layout. */

#ifndef _GROUP_PAN_GRP_HEADER_H
#define _GROUP_PAN_GRP_HEADER_H


#include "pan_sys.h"

#include "grp_types.h"


/* Print of group message */

char            *pan_grp_msg_t2ascii(grp_msg_t tp);

void             pan_grp_hdr_print(grp_hdr_p hdr);

#endif
