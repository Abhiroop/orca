#ifndef __GROUP_DISPATCH_H__
#define __GROUP_DISPATCH_H__


#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif

STATIC_CI void pan_grp_msg_switch(pan_msg_p msg);

STATIC_CI void pan_grp_disp_start(void);

STATIC_CI void pan_grp_disp_end(void);

#endif
