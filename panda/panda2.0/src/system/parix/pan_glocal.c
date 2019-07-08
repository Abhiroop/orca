#include "pan_sys.h"		/* Provides a system interface */

#include "pan_system.h"
#include "pan_threads.h"
#include "pan_sync.h"


static int         n_keys;		/* number of keys */
static pan_mutex_p glocal_lock;

pan_key_p
pan_key_create(void)
{
    int key;

    pan_mutex_lock(glocal_lock);
    if (n_keys >= PANDA_DATAKEYS_MAX) {
	pan_panic("pan_key_create failed");
    }
    key = n_keys++;
    pan_mutex_unlock(glocal_lock);
    return (pan_key_p)key;
}

/*ARGSUSED*/
void
pan_key_clear(pan_key_p key)
{
}

void
pan_key_setspecific(pan_key_p key, void *ptr)
{
    if ((int)key < 0 || (int)key >= n_keys) {
	pan_panic("pan_setspecific failed for key %d\n", key);
    }
    pan_thread_self()->glocal[(int)key] = ptr;
}


void *
pan_key_getspecific(pan_key_p key)
{
    if ((int)key < 0 || (int)key >= n_keys) {
	pan_panic("pan_getspecific failed for key %d\n", key);
    }
    return pan_thread_self()->glocal[(int)key];
}


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
