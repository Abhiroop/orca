#ifndef __PAN_SYS_UCAST_H__
#define __PAN_SYS_UCAST_H__

#include "pan_sys.h"

extern pan_thread_p    pan_ucast_rcve_thread;



void pan_comm_ucast_info(void);

void pan_comm_ucast_start(void);

void pan_comm_ucast_end(void);

#endif
