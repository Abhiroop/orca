/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"
#include "pan_strdup.h"

char *
strdup(const char *s)
{
    size_t len;
    char *res;

    len = strlen(s) + 1;
    res = pan_malloc(len);
    memcpy(res, s, len);

    return res;
}

void
pan_strdup_start(void)
{
}

void
pan_strdup_end(void)
{
}
