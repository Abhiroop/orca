/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _TRACE_TRC2UPSHOT_H
#define _TRACE_TRC2UPSHOT_H


#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"


extern void
upshot_block_hdr_printf(FILE *s, size_t size, trc_thread_id_t thread_id);

extern void
upshot_event_printf(FILE *s, trc_event_lst_p event_list, trc_event_descr_p e,
		    pan_time_fix_p t_offset);

extern void upshot_fmt_printf(FILE *s, trc_p state);

extern void
upshot_event_lst_printf(FILE *s, trc_event_lst_p event_list);

#endif
