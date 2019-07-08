/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the send functions of group communication.
 */

#ifndef _GROUP_GRP_SEND_H_
#define _GROUP_GRP_SEND_H_


#include "pan_sys.h"

#include "pan_group.h"

#include "grp_global.h"
#include "grp_header.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif



#ifndef NDEBUG
STATIC_CI boolean   pan_grp_frag_acked(grp_hdr_p hdr);
#endif
STATIC_CI boolean   pan_grp_trylock_frag(grp_hdr_p hdr);
STATIC_CI pan_msg_p pan_grp_frag_arrived(pan_group_p g, grp_hdr_p hdr,
					 int is_last);

#ifndef STATIC_CI

			/* Communication between sender and receiver modules */
pan_fragment_p pan_grp_get_home_bb_data(pan_group_p g, grp_hdr_p acpt);
void           pan_grp_flag_discard(pan_group_p g, short int sender_id);

void           pan_grp_init_send(pan_group_p g);
void           pan_grp_clear_send(pan_group_p g);

void           pan_grp_send_start(void);
void           pan_grp_send_end(void);

#endif

#endif
