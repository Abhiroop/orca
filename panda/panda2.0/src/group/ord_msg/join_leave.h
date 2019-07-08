#ifndef __JOIN_LEAVE_H__
#define __JOIN_LEAVE_H__

#include "pan_sys_msg.h"

#include "header.h"

void        pan_grp_jl_start(void);

void        pan_grp_jl_end(void);

void        pan_grp_handle_join(grp_hdr_p hdr, pan_msg_p msg);

void        pan_grp_handle_leave(grp_hdr_p hdr, pan_msg_p msg);

#endif
