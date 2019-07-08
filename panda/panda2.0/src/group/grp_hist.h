/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Numbered message buffer implementation.
 */


#ifndef _GROUP_PAN_HIST_H
#define _GROUP_PAN_HIST_H

#include <assert.h>


#include "pan_sys.h"

#include "grp_types.h"




#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


extern int pan_hist_nearly_full_perc;
extern int pan_hist_send_status_perc;



#ifndef STATIC_CI

boolean        pan_hist_nearly_full(hist_p hist);
int            pan_hist_upb(hist_p buf);
int            pan_hist_next_accept(hist_p buf);

int            pan_hist_reserve(hist_p buf);
void           pan_hist_copy(hist_p hist, pan_fragment_p frag, int idx,
			     int preserve);
void           pan_hist_release_upto(hist_p buf, int new_last);

pan_fragment_p pan_hist_look(hist_p buf, int seqno);
pan_fragment_p pan_hist_locate(hist_p buf, int sender, unsigned short int messid,
			       grp_msg_t tp);

void           pan_hist_init(hist_p hist, int size);
int            pan_hist_clear(hist_p hist);

#endif


#endif
