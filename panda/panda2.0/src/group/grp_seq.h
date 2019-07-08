/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the sequencer that runs as part of the orderer
 *  daemon of group communication.
 */


#ifndef _GROUP_PAN_GRP_SEQ_H
#define _GROUP_PAN_GRP_SEQ_H


#include "pan_sys.h"

#include "grp_group.h"
#include "grp_header.h"

#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


void pan_grp_sequence_data_upcall(pan_group_p g, pan_fragment_p frag,
				  grp_hdr_p hdr);
void pan_grp_sequence_cntrl_upcall(pan_group_p g, grp_hdr_p hdr);
void pan_grp_seq_await(pan_group_p g);

#ifndef STATIC_CI

void pan_grp_init_sequencer(pan_group_p g);
void pan_grp_clear_sequencer(pan_group_p g);

void pan_grp_seq_start(void);
void pan_grp_seq_end(void);

#endif

#endif
