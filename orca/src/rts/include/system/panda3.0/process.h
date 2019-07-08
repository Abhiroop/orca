/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "orca_types.h"

#define p_get_descr(p)                 ( (p)->pdscr     )
 
#define p_set_blocking(p)              (p)->blocking = 1

#define p_get_blocking(p)              ( (p)->blocking  )

#define p_doing_operation(p)           ( (p)->depth > 0 )

#define p_create_process(descr, argv)  (void)p_init(0, descr, argv)

extern void p_start(void);

extern void p_end(void);

extern process_p p_init(pan_thread_p tid, struct proc_descr *orca_descr,
			void **argv);

extern void p_clear(process_p proc);

extern process_p p_self(void);

#endif
