/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"
/* #include "trc_io.h" */
#include "trc2picl.h"


void
picl_block_hdr_printf(FILE *s, size_t size, trc_thread_id_t thread_id)
{
}


void
picl_event_printf(FILE *s, trc_event_lst_p event_list, trc_event_descr_p e,
		  pan_time_fix_p t_offset)
{
}


void
picl_fmt_printf(FILE *s, trc_p state)
{
}


void
picl_event_lst_printf(FILE *s, trc_event_lst_p event_list)
{
}
