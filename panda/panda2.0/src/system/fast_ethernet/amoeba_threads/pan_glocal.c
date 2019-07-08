#include "pan_sys_msg.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_glocal.h"
#include "pan_error.h"

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

    key = (pan_key_p)pan_malloc(sizeof(struct pan_key));
    assert(key);

    key->key = 0;

    if (thread_alloc(&key->key, sizeof(void *)) == (char *)0) {
        pan_panic("pan_key_create: thread_alloc failed\n");
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
    void **p;

    p = (void **)thread_alloc(&key->key, sizeof(void *));
    if (p == (void **)0) {
        pan_panic("pan_key_setspecific: thread_alloc failed\n");
    }
    *p = ptr;
}


void *
pan_key_getspecific(pan_key_p key)
{
    void **p;

    p = (void **)thread_alloc(&key->key, sizeof(void *));
    if (p == (void **)0) {
        pan_panic("pan_key_getspecific: thread_alloc failed\n");
    }

    return *p;
}

