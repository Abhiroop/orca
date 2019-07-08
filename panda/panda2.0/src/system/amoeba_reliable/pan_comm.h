#ifndef _SYS_GENERIC_COMM_
#define _SYS_GENERIC_COMM_

#include "pan_nsap.h"

extern void pan_sys_comm_start(void);
extern void pan_sys_comm_wakeup(void);
extern void pan_sys_comm_end(void);

#define BCAST_COMM_HDR_SIZE 0
#define UCAST_COMM_HDR_SIZE 0

#define MAX_COMM_HDR_SIZE	(BCAST_COMM_HDR_SIZE > UCAST_COMM_HDR_SIZE ? \
				 BCAST_COMM_HDR_SIZE : UCAST_COMM_HDR_SIZE)

extern int sys_sprint_comm_stat_size(void);
extern void sys_sprint_comm_stats(char *buf);

#endif

