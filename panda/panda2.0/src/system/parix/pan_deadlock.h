#ifndef __PAN_SYS_DEADLOCK_H__
#define __PAN_SYS_DEADLOCK_H__

#include <sys/time.h>

#include <limits.h>


/*
 * Deadlock detection
 */



/* For broadcast investigation */
extern int  magic_bcast_upcall_;

extern int  magic_cookie;
extern int  magic_watchdog[4];
extern int  magic_wachhond[4];

/* For unicast investigation */
extern int  magic_ucast_upcall_;


/* also defined in continuations.h!: */
#ifdef DEADLOCK_DETECT

#define detect_enter(x_name) ((magic_ ## x_name) = TimeNowHigh())
#define detect_exit(x_name)  ((magic_ ## x_name) = INT_MAX)

#else

#define detect_enter(x_name)
#define detect_exit(x_name)

#endif


void pan_comm_deadlock_start(void);

void pan_comm_deadlock_end(void);

#endif
