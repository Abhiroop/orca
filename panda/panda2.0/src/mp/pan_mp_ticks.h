/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __TICKS__
#define __TICKS__

#include "pan_sys.h"

extern void pan_mp_ticks_start(void);
extern void pan_mp_ticks_end(void);
extern void pan_mp_ticks_register(void (*handler)(int finish),
				  pan_mutex_p mutex);
extern void pan_mp_ticks_release(void);


#endif
