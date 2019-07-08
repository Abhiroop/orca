/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* BB buffer. Implemented, for instance, as an array of message buffers. */


#ifndef _GROUP_GRP_BB_H_
#define _GROUP_GRP_BB_H_



#include "pan_sys.h"

#include "grp_types.h"

#include "grp_msg_buf.h"


typedef struct BB_BUF_T bb_buf_t, *bb_buf_p;

#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

boolean        bbb_acceptable(bb_buf_p bb_buf, int sender, int messid);
boolean        bbb_is_unused(bb_buf_p bb_buf, int sender, int messid);
boolean        bbb_is_written(bb_buf_p bb_buf, int sender, int messid);
boolean        bbb_is_read(bb_buf_p bb_buf, int sender, int messid);

pan_fragment_p bbb_get(bb_buf_p bb_buf, int sender, int messid);
boolean        bbb_put(bb_buf_p bb_buf, int sender, int messid,
		       pan_fragment_p frag);
void           bbb_advance(bb_buf_p bb_buf, int sender, int messid);

boolean        bbb_no_consistent(bb_buf_p bb_buf, int sender);

void           bbb_start(bb_buf_p bb_buf, int sender, int messid);

bb_buf_p       bbb_create(int bb_buf_size, int n_members);
int            bbb_clear(bb_buf_p bb_buf);

#endif

#endif
