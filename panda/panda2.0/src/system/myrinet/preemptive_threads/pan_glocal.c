#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_sync.h"
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_glocal.h"
#include "pan_error.h"

static int key_id;
static pan_mutex_p glocal_lock;

void
pan_sys_glocal_start(void)
{
    glocal_lock = pan_mutex_create();
}

void
pan_sys_glocal_end(void)
{
    pan_mutex_clear(glocal_lock);
}

pan_key_p
pan_key_create(void)
{
    pan_key_p key;

    pan_mutex_lock(glocal_lock);

    if (key_id == MAX_KEY) {
	pan_panic("%d) pan_key_create: too many keys (%d)\n",
		   pan_sys_pid, MAX_KEY);
    }

    key = (pan_key_p)pan_malloc(sizeof(struct pan_key));
    if (!key) {
	pan_panic("%d) pan_key_create: malloc failed\n", pan_sys_pid);
    }

    key->k_id = key_id++;;

    pan_mutex_unlock(glocal_lock);

    return key;
}

void
pan_key_clear(pan_key_p key)
{
    pan_free(key);
}

void
pan_key_setspecific(pan_key_p key, void *ptr)
{
    assert(0 <= key->k_id && key->k_id < key_id);
    pan_cur_thread->t_glocal[key->k_id] = ptr;
}


void *
pan_key_getspecific(pan_key_p key)
{
    assert(0 <= key->k_id && key->k_id < key_id);
    return pan_cur_thread->t_glocal[key->k_id];
}

