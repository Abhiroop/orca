/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_MP_ERROR__
#define __PAN_MP_ERROR__

typedef enum {
    STATE_GROW, RETRANS, FIRST_LOST, TOO_LATE, 
    DISCARD, WRONG_STATE, OVERRUN, QUEUE_FULL,
    PROGRESS, NR_WARNINGS
} warning_t;

extern void pan_mp_warning(warning_t type, char *code, ...);
extern void pan_mp_error(char *code, ...);
extern void pan_mp_debug(char *code, ...);
extern void pan_mp_dump(void);



#endif
