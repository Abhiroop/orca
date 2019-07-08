/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "pan_bg.h"
#include "pan_bg_error.h"

void 
pan_bg_warning(char *code, ...)
{
#ifdef VERBOSE
    va_list args;

    va_start(args, code);
    (void)vfprintf(stderr, code, args); 
    va_end(args);
#endif
}

void
pan_bg_error(char *code, ...)
{
    va_list args;
    
    va_start(args, code);
    fprintf(stderr, "ERROR: ");
    (void)vfprintf(stderr, code, args); 
    va_end(args);
    exit(1);
}
   
