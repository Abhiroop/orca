/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stdio.h>
#include <stdarg.h>
#include "pan_sys.h"
#include "pan_mp_error.h"

static int counters[NR_WARNINGS];
static int changed[NR_WARNINGS];
static int acc_changed;

static char *code[] = {
    "State grow",
    "Retrans",
    "First lost",
    "Too late",
    "Discard",
    "Wrong state",
    "Seqno overrun", 
    "Queue full",
    "Progress"
};


/*
 * pan_mp_warning:
 *                 Prints a message passing warning, or collect the
 *                 warnings for the dump routine.
 */
void
pan_mp_warning(warning_t type, char *code, ...)
{
#ifndef NOVERBOSE
#ifdef DEBUG    
    va_list args;

    if (type == PROGRESS) return;

    va_start(args, code);
    fprintf(stderr, "MP warning: cpu: %d) ", pan_my_pid());
    (void)vfprintf(stderr, code, args); 
    va_end(args);
#else
    counters[type]++;
    changed[type] = 1;
    if (type != PROGRESS) acc_changed = 1;
#endif
#endif
}

/*
 * pan_mp_dump:
 *                 Dump all collected warnings
 */
void
pan_mp_dump(void)
{
    int i;

    if (!acc_changed) return;

    acc_changed = 0;

    fprintf(stderr, "MP warning: cpu: %d) ", pan_my_pid());
    for(i = 0; i < NR_WARNINGS; i++){
	if (changed[i]){
	    changed[i] = 0;
	    fprintf(stderr, "%s: %d ", code[i], counters[i]);
	}
    }
    fprintf(stderr, "\n");
}


/*
 * pan_mp_error:
 *                 Prints a message passing error and stops
 */
void
pan_mp_error(char *code, ...)
{
    va_list args;

    va_start(args, code);
    fprintf(stderr, "MP error: cpu: %d) ", pan_my_pid());
    (void)vfprintf(stderr, code, args); 
    va_end(args);
    exit(0);
}

/*
 * pan_mp_debug:
 *                 Prints a message passing debug statement
 */
void
pan_mp_debug(char *code, ...)
{
#ifdef DEBUG
    va_list args;

    va_start(args, code);
    fprintf(stderr, "MP debug: cpu: %d) ", pan_my_pid());
    (void)vfprintf(stderr, code, args); 
    va_end(args);
#else
    return;
#endif
}
