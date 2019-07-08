/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

#include "pan_sys.h"
#include "pan_util.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"
#include "trc_event_tp.h"
#include "trc_lib.h"
#include "trc_io.h"
#include "trc_bind_op.h"
#include "trc2ascii.h"
#include "trc2upshot.h"
#include "trc2picl.h"
#include "trc2xab.h"


#define TRC_EVENTS 256




static void
upshot_block_printf(pan_time_fix_p t_current, trc_p trace_state,
		    trc_thread_id_t thread_id, char *buf, int buf_size,
		    pan_time_fix_p t_start, boolean do_rebind)
{
    char             *p;
    trc_event_descr_t e;
    FILE             *s;
    trc_event_lst_p   lst;

    p = buf;
    trc_event_descr_init(&e);
    s = trace_state->out_buf.stream;
    lst = trace_state->event_list;

    while (p < buf + buf_size) {
	p += trc_event_get(p, trace_state, &e);

	if (e.type == CLOCK_SHIFT) {
	    *t_current = *(pan_time_fix_p)e.usr;
	} else {

	    if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		pan_time_fix_add(&e.t, t_current);
		e.thread_id.my_pid = trace_state->my_pid;
		e.thread_id.my_thread = thread_id.my_thread;
		*t_current = e.t;
	    }

	    if (do_rebind) {
		trc_rebind_op(trace_state->event_list, trace_state->rebind, &e);
	    }

	    e.thread_id.my_thread = trc_upshot_thread_id(trace_state,
							 e.thread_id);
	    upshot_event_printf(s, lst, &e, t_start);
	}
    }

    trc_event_descr_clear(&e);
}



static void
picl_block_printf(pan_time_fix_p t_current, trc_p trace_state,
		  trc_thread_id_t thread_id, char *buf, int buf_size,
		  pan_time_fix_p t_start, boolean do_rebind)
{
    char             *p;
    trc_event_descr_t e;
    FILE             *s;
    trc_event_lst_p   lst;

    p = buf;
    trc_event_descr_init(&e);
    s = trace_state->out_buf.stream;
    lst = trace_state->event_list;

    while (p < buf + buf_size) {
	p += trc_event_get(p, trace_state, &e);

	if (e.type == CLOCK_SHIFT) {
	    *t_current = *(pan_time_fix_p)e.usr;
	} else {

	    if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		pan_time_fix_add(&e.t, t_current);
		e.thread_id.my_pid = trace_state->my_pid;
		e.thread_id.my_thread = thread_id.my_thread;
		*t_current = e.t;
	    }

	    if (do_rebind) {
		trc_rebind_op(trace_state->event_list, trace_state->rebind, &e);
	    }

	    picl_event_printf(s, lst, &e, t_start);
	}
    }

    trc_event_descr_clear(&e);
}



static void
xab_block_printf(pan_time_fix_p t_current, trc_p trace_state,
		 trc_thread_id_t thread_id, char *buf, int buf_size,
		 pan_time_fix_p t_start, boolean do_rebind)
{
    char             *p;
    trc_event_descr_t e;
    FILE             *s;
    trc_event_lst_p   lst;

    p = buf;
    trc_event_descr_init(&e);
    s = trace_state->out_buf.stream;
    lst = trace_state->event_list;

    while (p < buf + buf_size) {
	p += trc_event_get(p, trace_state, &e);

	if (e.type == CLOCK_SHIFT) {
	    *t_current = *(pan_time_fix_p)e.usr;
	} else {

	    if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		pan_time_fix_add(&e.t, t_current);
		e.thread_id.my_pid = trace_state->my_pid;
		e.thread_id.my_thread = thread_id.my_thread;
		*t_current = e.t;
	    }

	    if (do_rebind) {
		trc_rebind_op(trace_state->event_list, trace_state->rebind, &e);
	    }

	    xab_event_printf(s, lst, &e, t_start);
	}
    }

    trc_event_descr_clear(&e);
}



static void
ascii_block_printf(pan_time_fix_p t_current, trc_p trace_state,
	           trc_thread_id_t thread_id, char *buf, int buf_size,
	           pan_time_fix_p t_start, boolean do_rebind)
{
    char             *p;
    trc_event_descr_t e;
    FILE             *s;
    trc_event_lst_p   lst;

    p = buf;
    trc_event_descr_init(&e);
    s = trace_state->out_buf.stream;
    lst = trace_state->event_list;

    while (p < buf + buf_size) {
	p += trc_event_get(p, trace_state, &e);

	if (e.type == CLOCK_SHIFT) {
	    *t_current = *(pan_time_fix_p)e.usr;
	} else {

	    if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		pan_time_fix_add(&e.t, t_current);
		e.thread_id.my_pid = trace_state->my_pid;
		e.thread_id.my_thread = thread_id.my_thread;
		*t_current = e.t;
	    }

	    if (do_rebind) {
		trc_rebind_op(trace_state->event_list, trace_state->rebind, &e);
	    }

	    trc_event_printf(s, lst, &e, t_start);
	}
    }

    trc_event_descr_clear(&e);
}


static void
trc_data_printf(trc_p trace_state, char *buf, trc_block_hdr_t hdr,
		trc_output_t out_t, boolean abs_times, boolean do_rebind)
{
    trc_thread_info_p thread_info;
    pan_time_fix_t    t_offset;
    pan_time_fix_p    t;

    assert(hdr.size > 0);

    if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {

					/* Print the header */
	switch (out_t) {

	case TRC_OUTPUT_UPSHOT:
	    upshot_block_hdr_printf(trace_state->out_buf.stream, hdr.size,
				    hdr.thread_id);
	    break;

	case TRC_OUTPUT_PICL:
	    picl_block_hdr_printf(trace_state->out_buf.stream, hdr.size,
				  hdr.thread_id);
	    break;

	case TRC_OUTPUT_XAB:
	    xab_block_hdr_printf(trace_state->out_buf.stream, hdr.size,
				 hdr.thread_id);
	    break;

	case TRC_OUTPUT_ASCII:
	default:
	    fprintf(trace_state->out_buf.stream, "%s", trc_marker);
	    thread_info = trc_thread_locate(trace_state, hdr.thread_id);
	    trc_block_hdr_printf(trace_state->out_buf.stream, hdr.size,
				    thread_info->name, hdr.thread_id);

	}

	thread_info = trc_thread_locate(trace_state, hdr.thread_id);
	t = &thread_info->t_fix_current;
    }

    if (abs_times) {
	t_offset = pan_time_fix_zero;
    } else {
	t_offset = trace_state->t_start;
    }

					/* Print the data */
    switch (out_t) {

    case TRC_OUTPUT_UPSHOT:
	upshot_block_printf(t, trace_state, hdr.thread_id, buf, hdr.size,
			    &t_offset, do_rebind);
	break;

    case TRC_OUTPUT_PICL:
	picl_block_printf(t, trace_state, hdr.thread_id, buf, hdr.size,
			  &t_offset, do_rebind);
	break;

    case TRC_OUTPUT_XAB:
	xab_block_printf(t, trace_state, hdr.thread_id, buf, hdr.size,
			 &t_offset, do_rebind);
	break;

    case TRC_OUTPUT_ASCII:
    default:
	ascii_block_printf(t, trace_state, hdr.thread_id, buf, hdr.size,
			   &t_offset, do_rebind);
    }
}


static void
trc_state_printf(trc_p trace_state, trc_output_t out_t)
{
    FILE  *s;

    s = trace_state->out_buf.stream;

    switch (out_t) {

    case TRC_OUTPUT_UPSHOT:
	upshot_fmt_printf(s, trace_state);
	upshot_event_lst_printf(s, trace_state->event_list);
	break;

    case TRC_OUTPUT_PICL:
	picl_fmt_printf(s, trace_state);
	picl_event_lst_printf(s, trace_state->event_list);
	break;

    case TRC_OUTPUT_XAB:
	picl_fmt_printf(s, trace_state);
	xab_event_lst_printf(s, trace_state->event_list);
	break;

    case TRC_OUTPUT_ASCII:
    default:
	fprintf(s, "%s", trc_marker);
	trc_fmt_printf(s, trace_state);
	fprintf(s, "%s", trc_marker);
	trc_event_lst_printf(s, trace_state->event_list);
    }
}


static void
usage(char *progname)
{
    fprintf(stderr, "Usage: %s [-s] [-a] [-l] [-r] [-e obj_op_file] [filename]\n", progname);
    exit(55);
}


static void
trc_ev_set_print(FILE *s, trc_p trc)
{
    int               e;
    trc_event_lst_p   lst;
    int               p;
    trc_thread_info_p thr;

    lst = trc->event_list;
    for (p = -1; p <= trc->n_pids; p++) {
	thr = trc->thread[p].threads;
	fprintf(s, "\n");
	while (thr != NULL) {
	    fprintf(s, "%2d: %-32s logged events of type:\n", 
		    p, thr->name);
	    e = trc_event_lst_first(lst);
	    while (e != -1) {
		if (trc_is_set(thr->event_tp_set, e)) {
		    fprintf(s, "\t%3d %s\n", e, trc_event_lst_name(lst, e));
		}
		e = trc_event_lst_next(lst, e);
	    }
	    thr = thr->next_thread;
	}
    }
}



int
main(int argc, char *argv[])
{
    trc_t              trace_state;
    trc_output_t       out_t;
    char              *basename;
    boolean            abs_times = FALSE;
    trc_thread_info_p  thr;
    int                n_threads;
    int                p;
    trc_block_hdr_t    hdr;
    char              *buf;
    char              *filename = NULL;
    boolean            be_silent = FALSE;
    boolean            print_ev_set = FALSE;
    boolean            print_rb = FALSE;
    char              *rebind_file = NULL;
    trc_ev_name_p      obj_ev_names;
    int                n_ev;
    int                i;
    FILE              *s;

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	    case 'a': 	abs_times = TRUE;
			break;
	    case 'e':	rebind_file = argv[++i];
			break;
	    case 'l':	print_ev_set = TRUE;
			break;
	    case 'r':	print_rb = TRUE;
			break;
	    case 's':	be_silent = TRUE;
			break;
	    default:	fprintf(stderr, "No such option: %s\n", argv[i]);
			usage(argv[0]);
	    }
	} else {
	    if (filename == NULL) {
		filename = argv[i];
	    } else {
		fprintf(stderr, "Only 1 filename allowed: ignore %s\n",
				argv[i]);
		usage(argv[0]);
	    }
	}
    }

    if (filename != NULL) {
	if (freopen(filename, "r", stdin) == NULL) {
	    fprintf(stderr, "%s: file not found: %s\n", argv[0], filename);
	    usage(argv[0]);
	}
    }

    trc_init_state(&trace_state);
    trc_open_infile(&trace_state, NULL);

    basename = strrchr(argv[0], '/');
    if (basename == NULL) {
	basename = argv[0];
    } else {
	++basename;
    }
    if (strcmp(basename, "trc2upshot") == 0) {
	out_t = TRC_OUTPUT_UPSHOT;
    } else if (strcmp(basename, "trc2picl") == 0) {
	out_t = TRC_OUTPUT_PICL;
    } else if (strcmp(basename, "trc2xab") == 0) {
	out_t = TRC_OUTPUT_XAB;
    } else {
	out_t = TRC_OUTPUT_ASCII;
    }

    if (! trc_state_read(&trace_state)) {
	fprintf(stderr, "%s: no state block", argv[0]);
	return 1;
    }

    if (rebind_file != NULL) {
	if (trace_state.rebind == NULL) {
	    trace_state.rebind = trc_init_rebind();
	}
	n_ev = trc_read_obj_ev_file(rebind_file, &obj_ev_names);
	trc_mk_obj_ev(trace_state.event_list, trace_state.rebind, obj_ev_names,
		      n_ev, FALSE);
	pan_free(obj_ev_names);
    }

    if (out_t == TRC_OUTPUT_UPSHOT) {
	n_threads = 0;
	if (trace_state.my_pid == -1) {
	    for (p = -1; p < trace_state.n_pids + PANDA_1; p++) {
		thr = trace_state.thread[p].threads;
		while (thr != NULL) {
		    thr->upshot_id = n_threads;
		    ++n_threads;
		    thr = thr->next_thread;
		}
	    }
	} else {
	    thr = trace_state.thread[trace_state.my_pid].threads;
	    while (thr != NULL) {
		thr->upshot_id = n_threads;
		++n_threads;
		thr = thr->next_thread;
	    }
	}
    }

    trace_state.out_buf.stream = stdout;
    trace_state.out_buf.format |= trace_state.in_buf.format;

    s = trace_state.out_buf.stream;

    if (! be_silent) {
	trc_state_printf(&trace_state, out_t);
	fprintf(s, "%s", trc_marker);
    }

    if (print_rb) {
	trc_rebind_print(s, trace_state.rebind);
    }

    if (print_ev_set) {
	trc_ev_set_print(s, &trace_state);
    }

    while (TRUE) {
	buf = trc_block_read(&trace_state, &hdr);
	if (buf == NULL) {
	    break;
	}

	if (! be_silent && hdr.size != 0) {
	    trc_data_printf(&trace_state, buf, hdr, out_t, abs_times,
			    rebind_file != NULL);
	}

	if (hdr.size > 0) {
	    pan_free(buf);
	}
    }

    trc_state_clear(&trace_state);

    return 0;
}
