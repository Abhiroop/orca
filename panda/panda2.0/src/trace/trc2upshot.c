/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"

#include "pan_util.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"
#include "trc_event_tp.h"
#include "trc_io.h"
#include "trc_lib.h"
#include "trc2ascii.h"
#include "trc2upshot.h"


#define UPSHOT_STR      12


/* Include of alog/eventdefs.h:
 * Definitions of the reserved pseudo-event numbers.
 */

/* Start of include: */
/****************************************************************************
These are the reserved event types for logfile header records.  Unspecified
fields are either 0 or (in the case of string data)null.

e_type  proc_id  task_id  int_data  cycle  timestamp  string_data

 -1                                                    creator and date
 -2                       # events
 -3                       # procs
 -4                       # tasks
 -5                       # event types
 -6                                         start_time
 -7                                         end_time
 -8                       # timer_cycles
 -9                       event_type                   description
-10                       event_type                   printf string

*************************************************************************/

#define SYSTEM_TYPE     -1
#define NUM_EVENTS      -2
#define NUM_PROCS       -3
#define NUM_TASKS       -4
#define NUM_EVTYPES     -5
#define START_TIME      -6
#define END_TIME        -7
#define NUM_CYCLES      -8
#define EVTYPE_DESC     -9
#define EPRINT_FORMAT  -10

/* :end of include */

#define ROLLOVER       -11


void
upshot_block_hdr_printf(FILE *s, size_t size, trc_thread_id_t thread_id)
{
}

void
upshot_event_printf(FILE *s, trc_event_lst_p event_list, trc_event_descr_p e,
		    pan_time_fix_p t_start)
{
    char       *name;
    char       *fmt;
    size_t      n_fmt;
    size_t      n_usr;
    pan_time_fix_t    dt;
    pan_time_fix_t    ts;
    trc_event_t extern_id;
    int         i;
    size_t      usr_size;
    int         level;

    trc_event_lst_query(event_list, e->type, &usr_size, &level);
    trc_event_lst_query_extern(event_list, e->type, &extern_id,
			       &name, &fmt);
    fprintf(s, "%4d ", extern_id);
    fprintf(s, "%4d %4d   ", e->thread_id.my_pid, e->thread_id.my_thread);
    n_fmt = 0;
    n_usr = 0;
    trc_print_sel_tok(s, "dl", fmt, &n_fmt, e->usr, &n_usr);
    dt = e->t;
    ts = *t_start;
    ts.t_nsec = 0;
    pan_time_fix_sub(&dt, &ts);
    fprintf(s, " %4ld %7ld ", dt.t_sec, dt.t_nsec / 1000);
    for (i = 0; i < UPSHOT_STR && i < usr_size; i++) {
	fprintf(s, "%c", e->usr[i]);
    }
    fprintf(s, "\n");
}

void
upshot_fmt_printf(FILE *s, trc_p trc)
{
    assert(IS_THREADS_MERGED(trc->out_buf.format));
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    SYSTEM_TYPE, 0, 0, 0, 0, 0, UPSHOT_STR, trc->filename);
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    NUM_EVENTS, 0, 0, trc->entries,
	    0, 0, UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    NUM_PROCS, 0, 0, trc->n_pids,
	    0, 0, UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    NUM_TASKS, 0, 0, trc->n_threads - 1,
	    0, 0, UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    NUM_EVTYPES, 0, 0, trc_event_lst_num(trc->event_list),
	    0, 0, UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7ld %.*s\n",
	    START_TIME, 0, 0, 0, 0, trc->t_start.t_nsec / 1000,
	    UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7ld %.*s\n",
	    END_TIME, 0, 0, 0, 0, trc->t_stop.t_nsec / 1000,
	    UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4ld %4d %7d %.*s\n",
	    NUM_CYCLES, 0, 0, trc->t_stop.t_sec -
	    trc->t_start.t_sec + 1,
	    0, 0, UPSHOT_STR, "");
    fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
	    ROLLOVER, 0, 0, 0, 0, 1000000, UPSHOT_STR, "");
}

void
upshot_event_lst_printf(FILE *s, trc_event_lst_p event_list)
{
    trc_event_t e;
    trc_event_t e_extern;
    char       *name;
    char       *fmt;

    e = trc_event_lst_first(event_list);
    while (e != TRC_NO_SUCH_EVENT) {
	trc_event_lst_query_extern(event_list, e, &e_extern, &name, &fmt);
	fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
		EVTYPE_DESC, 0, 0, e_extern, 0, 0, UPSHOT_STR, name);
	fprintf(s, "%4d %4d %4d %4d %4d %7d %.*s\n",
		EPRINT_FORMAT, 0, 0, e_extern, 0, 0, UPSHOT_STR, fmt);
	e = trc_event_lst_next(event_list, e);
    }
}
