#ifndef __GROUP_NAME_H__
#define __GROUP_NAME_H__

#include "global.h"


void       pan_name_start(int init_size, int extend_size);

void       pan_name_end(void);

group_id_t pan_name_enter(char *name);

void       pan_name_delete(group_id_t gid);

#endif
