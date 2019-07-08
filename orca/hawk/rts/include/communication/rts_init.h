#ifndef __rts_init__
#define __rts_init__

#include "po.h"

int init_rts(int *argc, char *argv[]);
int start_rts(void);
int sync_rts(void);
int finish_rts(void);

int get_group_configuration(int *me, int *group_size, int *proc_debug);

#endif
