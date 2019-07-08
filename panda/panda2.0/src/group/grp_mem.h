/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the orderer daemon of ordinary members of group
 *  communication.
 */


#ifndef _GROUP_PAN_GRP_MEM_H
#define _GROUP_PAN_GRP_MEM_H

#include "pan_sys.h"

#include "pan_group.h"

#include "grp_types.h"
#include "grp_header.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif



void  pan_grp_orderer_data_upcall(pan_group_p g, pan_fragment_p frag,
				  grp_hdr_p hdr);
void  pan_grp_handle_ordr_cont(pan_group_p g);

#ifndef STATIC_CI


void	pan_grp_orderer_cntrl_upcall(pan_group_p g, grp_hdr_p cntrl_msg);

void	pan_grp_do_sync_at_home(pan_group_p g, grp_hdr_p sync_msg);
boolean	pan_grp_deliver_home_frag(pan_group_p g, pan_fragment_p frag,
				  int seqno);

void	pan_grp_init_orderer(pan_group_p g, void (*rcve)(pan_msg_p msg));
void	pan_grp_clear_orderer(pan_group_p g);

void    pan_grp_ordr_start(void);
void    pan_grp_ordr_end(void);

#endif


#endif
