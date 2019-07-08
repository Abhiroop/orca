/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 28, 1995
 *
 * Lock module
 *      Implement the RTS lock interface on top of Panda.
 */


#include "synchronization.h"	/* Part of the synchronization module */
#include "panda_lock.h"
#include "pan_sys.h"
#include "util.h"

int 
init_lock(int me, int group_size, int proc_debug)
{
    return 0;
}

int 
finish_lock(void)
{
    return 0;
}
 
po_lock_p 
new_lock(void)
{
    po_lock_p l;

    l = pan_malloc(sizeof(po_lock_t));

    l->lock = pan_mutex_create();

    return l;
}

int 
free_lock(po_lock_p l)
{
    pan_mutex_clear(l->lock);
    pan_free(l);

    return 0;
}
 
int 
lock(po_lock_p l)
{
    pan_mutex_lock(l->lock);
    
    return 0;
}

int 
unlock(po_lock_p l)
{
    pan_mutex_unlock(l->lock);

    return 0;
}
