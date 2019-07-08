#include "pan_sys.h"		/* Provides a system interface */
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_global.h"
#include "pan_glocal.h"
#include "pan_error.h"

static int key_id;

void
pan_sys_glocal_start(void)
{
}

void
pan_sys_glocal_end(void)
{
}

pan_key_p
pan_key_create(void)
{
    pan_key_p key;

    if (key_id == MAX_KEY) {
	pan_panic("%d) pan_key_create: too many keys (%d)\n",
		   pan_sys_pid, MAX_KEY);
    }

    key = (pan_key_p)pan_malloc(sizeof(struct pan_key));
    if (!key) {
	pan_panic("%d) pan_key_create: malloc failed\n", pan_sys_pid);
    }

    key->k_id = key_id++;

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
    pan_thread_p thread = ot_thread_getspecific(ot_current_thread());

    assert(0 <= key->k_id && key->k_id < key_id);
    thread->t_glocal[key->k_id] = ptr;
}


void *
pan_key_getspecific(pan_key_p key)
{
    pan_thread_p thread = ot_thread_getspecific(ot_current_thread());

    assert(0 <= key->k_id && key->k_id < key_id);
    return thread->t_glocal[key->k_id];
}
