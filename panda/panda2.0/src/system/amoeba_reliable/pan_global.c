#include "pan_sys.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

 
void
pan_panic(const char *code, ...)
{
    va_list args;
    char buf[256];
 
    va_start(args, code);
    fprintf(stderr, "%2d/%p\tPanic: ", pan_my_pid(), pan_thread_self());
    (void)vsprintf(buf, code, args);
    perror(buf);
    va_end(args);
    abort();
}
