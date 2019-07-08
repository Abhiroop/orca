#ifndef __GROUP_GROUP_TAB_H__
#define __GROUP_GROUP_TAB_H__

#include "global.h"
#include "grp.h"


extern pan_group_p group_tab[MAX_GROUPS];

/* pan_group_p pan_gtab_find(group_id_t gid); */

#define pan_gtab_find(gid)	group_tab[gid]


void        pan_gtab_enter(group_id_t gid, pan_group_p g);

void        pan_gtab_delete(group_id_t gid);

void        pan_gtab_await_empty(void);

void        pan_gtab_start(pan_mutex_p lock);
void        pan_gtab_end(void);

#endif

