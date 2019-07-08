/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_INIT_H__
#define __RTS_INIT_H__

/* These functions take care of initialising and terminating all
 * RTS modules.
 */

extern void rts_init(int trace_level);
extern void rts_end(void);

#endif
