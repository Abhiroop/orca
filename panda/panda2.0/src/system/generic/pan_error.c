/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "pan_global.h"
#include "pan_error.h"
#include "pan_threads.h"

#include <stdarg.h>

void
pan_panic(const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    fprintf(stderr, "thread: %lx\tPanic: ", (long)pan_thread_self());
    (void)vsprintf(buf, fmt, args); 
    perror(buf);
    va_end(args);
    assert(0);
}