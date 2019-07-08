/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_glocal.h"
#include "pan_error.h"
#include <string.h>

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
    memset(key, 0, sizeof(struct pan_key));

    if (thr_keycreate(&key->key, NULL) != 0) {
        pan_panic("thr_keycreate failed\n");
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
    if (thr_setspecific(key->key, ptr) != 0) {
        pan_panic("thr_setspecific failed\n");
    }
}


void *
pan_key_getspecific(pan_key_p key)
{
    void *ptr;

    if (thr_getspecific(key->key, &ptr) != 0) {
        pan_panic("thr_getspecific failed\n");
    }

    return ptr;
}
