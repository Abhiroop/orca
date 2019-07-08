#include <assert.h>

#include "global.h"
#include "group_tab.h"
#include "grp.h"


/* Author: Raoul Bhoedjang, October 1993
 *
 * Group_tab:
 *   simple table of group data structures
 */

pan_group_p        group_tab[MAX_GROUPS];
static int         size = 0;

static pan_mutex_p gtab_lock;
static pan_cond_p  gtab_empty;


void
pan_gtab_start(pan_mutex_p lock)
{
    gtab_lock  = lock;
    gtab_empty = pan_cond_create(gtab_lock);
}


void
pan_gtab_end(void)
{
    pan_cond_clear(gtab_empty);
}


void
pan_gtab_enter(group_id_t gid, pan_group_p g)
{
    assert(0 <= gid && gid < MAX_GROUPS);
    assert(!group_tab[gid]);

    group_tab[gid] = g;
    size++;
}


void
pan_gtab_delete(group_id_t gid)
{
    assert(0 <= gid && gid < MAX_GROUPS);
    assert(group_tab[gid]);

    pan_group_clear(group_tab[gid]);
    group_tab[gid] = NULL;
    if (--size == 0) {
	pan_cond_broadcast(gtab_empty);
    }
}


void
pan_gtab_await_empty(void)
{
    pan_mutex_lock(gtab_lock);

    while (size > 0) {
	pan_cond_wait(gtab_empty);
    }

    pan_mutex_unlock(gtab_lock);
}
