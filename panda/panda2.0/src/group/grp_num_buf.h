/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Numbered message buffer implementation.
 */


#ifndef _GROUP_PAN_NUM_BUF_H
#define _GROUP_PAN_NUM_BUF_H

#include <assert.h>


#include "pan_sys.h"

#include "grp_types.h"




#ifndef STATIC_CI
#  define STATIC_CI	static
#endif




#ifndef STATIC_CI

int            pan_num_buf_upb(num_buf_p buf);
int            pan_num_buf_next_accept(num_buf_p buf);
pan_fragment_p pan_num_buf_last(num_buf_p buf);

boolean        pan_num_buf_in_use(num_buf_p buf, int idx);
boolean        pan_num_buf_in_range(num_buf_p num_buf, int idx);

boolean        pan_num_buf_advance(num_buf_p buf, int seqno);
pan_fragment_p pan_num_buf_get(num_buf_p buf);
void           pan_num_buf_accept(num_buf_p buf, pan_fragment_p frag, int idx);

void           pan_num_buf_start(num_buf_p buf, int start);
void           pan_num_buf_init(num_buf_p num_buf, int size);
int            pan_num_buf_clear(num_buf_p num_buf);

#endif


#endif
