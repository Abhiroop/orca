/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/******************************************************************************
 *
 * The layer for reassembly of fragments.
 * Completely reassembled msgs are delivered to the upper layer via the
 * registered group upcall.
 * Upcall hook to the group fragment layer:
 *
 *    - boolean assemble_and_deliver(pan_group_p g, pan_fragment_p frag);
 *
 ******************************************************************************/


#ifndef __PAN_GRP_ASSEMBLE_H__
#define __PAN_GRP_ASSEMBLE_H__

#include "pan_sys.h"

#include "grp_types.h"
#include "grp_group.h"
#include "grp_header.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

void pan_grp_assemble_and_deliver(pan_group_p g, pan_fragment_p frag,
				  grp_hdr_p hdr);

void pan_grp_assemble_init(pan_group_p g);
void pan_grp_assemble_clear(pan_group_p g);

void pan_grp_assemble_start(void);
void pan_grp_assemble_end(void);

#endif


#endif
