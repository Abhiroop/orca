#ifndef _SYS_CMAML_COMM_
#define _SYS_CMAML_COMM_

#include "pan_nsap.h"

typedef struct pan_receive *pan_receive_p;

void pan_sys_comm_start(void);
void pan_sys_comm_wakeup(void);
void pan_sys_comm_end(void);

#define MAX_NSAP_SMALL      8

int sys_sprint_comm_stat_size(void);
void sys_sprint_comm_stats(char *buf);
void pan_small_enqueue(int a0, int a1, int a2, int a3, int i);

#endif /* _SYS_CMAML_COMM_ */

