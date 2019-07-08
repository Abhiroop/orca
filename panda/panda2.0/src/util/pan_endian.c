/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_util.h"
#include "pan_endian.h"

typedef union INT_CHAR {
    long int n;
    char     c[sizeof(long int)];
} int_char_t, *int_char_p;


int
pan_is_bigendian(void)
{
    int_char_t ic;

    ic.n = 0x1;

    return (ic.c[sizeof(long int) - 1] == 0x1);
}

void
pan_endian_start(void)
{
}

void
pan_endian_end(void)
{
}
