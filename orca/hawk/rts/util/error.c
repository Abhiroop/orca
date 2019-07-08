/*
 * Author:         Tim Ruhl
 *
 * Date:           Oct 8, 1995
 *
 * error module:
 *                 Error printing routines
 */


#include "util.h"		/* Part of the utilities module */
#include "synchronization.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#define ERROR_DUMP_CORE

/* Lazy lock initialization */
static int initialized;
po_lock_p rts_error_lock;
static int warnings = 0;

#ifdef __GNUC__
void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
#endif


/*
 * rts_error:
 *                 Print an error message with a variable number of
 *                 arguments.
 */
void
rts_error(char *fmt, ...)
{
    va_list ap;

    if (!initialized) {
	rts_error_lock = new_lock();
	initialized = 1;
    }

    lock(rts_error_lock);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    unlock(rts_error_lock);

#ifdef ERROR_DUMP_CORE
    abort();
#else
    exit(1);
#endif
}


static char *warning_class[] = {
    "Performance warning: ",
    "Semantic warning: ",
    "DEBUG: "
};

/*
 * rts_warning:
 *                 Print a warning. Warnings are classified.
 */
void 
rts_warning(int class, char *fmt, ...)
{
    va_list ap;

    if (warnings){
	if (!initialized) {
	    rts_error_lock = new_lock();
	    initialized = 1;
	}
	
	lock(rts_error_lock);
	
	va_start(ap, fmt);
	fprintf(stderr, warning_class[class]);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	
	unlock(rts_error_lock);
    }
}

int
rts_warnings_set(int w)
{
    int old = warnings;

    warnings = w;

    return old;
}

#ifdef TIM
#include "pan_sys.h"

/*
 * debug:
 *                 Print a debug message.
 */
void 
debug(char *fmt, ...)
{
    va_list ap;

    if (!initialized) {
	rts_error_lock = new_lock();
	initialized = 1;
    }

    lock(rts_error_lock);

    va_start(ap, fmt);
    fprintf(stderr, "%p: ", (void *)pan_thread_self());
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    unlock(rts_error_lock);
}
#endif
