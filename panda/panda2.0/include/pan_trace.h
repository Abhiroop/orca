/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Public declarations of the panda trace package.
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */


#ifndef _TRACE_TRACE_H
#define _TRACE_TRACE_H

#include <stdlib.h>



typedef int		trc_event_t, *trc_event_p;


#ifdef TRACING

extern void		trc_start(char *filename, int max_buf_size);

extern void		trc_end(void);

extern void		trc_new_thread(int buf_size, char *name);

extern int		trc_set_level(int level);

extern trc_event_t	trc_new_event(int      level,
				      size_t   u_size,
				      char    *name,
				      char    *fmt);

extern void		trc_event(trc_event_t e, void *user_info);

extern void             trc_flush(void);

#else		/* TRACING */

					/* Undefine the trace function calls */

#  define	trc_start(filename, max_buf_size)
#  define	trc_end()
#  define	trc_new_thread(buf_size, name)
#  define	trc_set_level(level)				0
#  define	trc_new_event(level, u_size, name, fmt)		0
#  define	trc_event(e, user_info)
#  define	trc_flush()


#endif		/* TRACING */


#endif
