/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Public declarations of the panda trace package.
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */


#ifndef _TRACE_TRC_TRACE_H
#define _TRACE_TRC_TRACE_H

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif

#include "pan_trace.h"		/* For exported prototypes */


extern trc_event_t	START_TRACE;
extern trc_event_t	END_TRACE;


trc_event_t trc_renew_event(int level, size_t u_size, char *name, char *fmt);


#endif
