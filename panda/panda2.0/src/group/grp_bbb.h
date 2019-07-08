/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#ifndef __PAN_GRP_BBB_H__
#define __PAN_GRP_BBB_H__

#include "pan_sys.h"

#include "grp_global.h"
#include "grp_header.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

void           bb_store_data(pan_group_p g, pan_fragment_p frag, grp_hdr_p hdr);
pan_fragment_p bb_collect_data(pan_group_p g, grp_hdr_p acpt);
void           bb_handle_rexmit(pan_group_p g, grp_hdr_p hdr);
void           bb_handle_pb_messid(pan_group_p g, grp_hdr_p hdr);
void           bb_handle_own_bb_messid(pan_group_p g, grp_hdr_p hdr);

void           bb_buf_start(pan_group_p g, short int *bb_start);

void           bb_buf_create(pan_group_p g);
void           bb_buf_clear(pan_group_p g);

#endif


#endif
