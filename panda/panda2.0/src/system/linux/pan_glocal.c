#include "pan_sys.h"
#include "pan_error.h"
#include "pan_global.h"
#include "pan_glocal.h"

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

    key = pan_malloc(sizeof(pan_key_t));
    memset(key, 0, sizeof(pan_key_t));

    if (pthread_key_create(&key->key, NULL) != 0) {
	pan_panic("pthread_key_create failed\n");
    }

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
    if (pthread_setspecific(key->key, ptr) != 0) {
	pan_panic("pthread_setspecific failed\n");
    }
}


void *
pan_key_getspecific(pan_key_p key)
{
    void *ptr;

    ptr = pthread_getspecific(key->key);

    return ptr;
}


