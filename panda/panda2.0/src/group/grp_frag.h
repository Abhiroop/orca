/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Fragment buffer.
 * Implemented, for instance, as an array of message buffers. */


#ifndef _GROUP_GRP_FRAG_H_
#define _GROUP_GRP_FRAG_H_



#include "pan_sys.h"

#include "grp_types.h"
#include "grp_num_buf.h"



typedef struct FRAG_BUF_T frag_buf_t, *frag_buf_p;



#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

pan_msg_p   pan_grp_fb_locate(frag_buf_p frag_buf, int sender, int id);
pan_msg_p   pan_grp_fb_delete(frag_buf_p frag_buf, int sender, int id);
boolean     pan_grp_fb_store(frag_buf_p frag_buf, int sender, int id,
			     pan_msg_p msg);
frag_buf_p  pan_grp_fb_create(int size, int n_members);
int         pan_grp_fb_clear(frag_buf_p frag_buf);

#endif


#endif
