/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Main to filter events.
 * Syntax:
 *
 * a.out <options> <infile>
 *
 * <options> ::= <filter_option>* | <other_options>
 *
 * <filter_option>    ::= "-filter <filter_list>"
 * <filter_list>      ::= <field_list> | <field_list> "|" <filter_list>
 * <field_list>       ::= <field> | <field> ";" <field_list>
 * <field>            ::= "hide" | <tag> "=" <value>
 * <value>            ::= <basic value> | <typed_value_list>
 * <typed_value_list> ::= <typed_value> | <typed_value> "&" <typed_value_list>
 * <typed_value>      ::= <value_tag> ":" <value_field>
 *
 * <tag> ::= "event" | "pe" | "thread" | "level" | "object"
 *
 * <tag> == "thread" :
 *	<value_tag>   ::= "logging"
 *
 * <tag> == "object" :
 *      <value_tag>   ::= "type" | "obj" | "op"
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#include "trc_filter.h"

#define OUT_BUF_SIZE 1048576





static void
usage(char *prog_name)
{
    fprintf(stderr, "Usage: %s [-filter <filter]* [-e obj_op_file] [-o out_file] in_file\n", prog_name);
}



int
main(int argc, char *argv[])
{
    int                n_ev;
    trc_ev_name_p      obj_ev_name;
    int                i;
    trc_t              trace_state;
    trc_event_descr_t  e;
    trc_thread_id_t    pseudo_thread = trc_no_such_thread;
    char              *rebind_file = NULL;
    char              *out_file = NULL;
    char              *in_file = NULL;
    int                option;
    int                pe;
    int                thr;
    int                n_threads;
    int                n_ev_tp;

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-e") == 0) {
	    ++i;
	    rebind_file = argv[i];
	} else if (strcmp(argv[i], "-filter") == 0) {
	    ++i;	/* skip arg, parse it in the filter init */
	} else if (strcmp(argv[i], "-o") == 0) {
	    ++i;
	    out_file = argv[i];
	} else {
	    if (option == 0) {
		in_file = argv[i];
		++option;
	    } else {
		usage(argv[0]);
		return 33;
	    }
	}
    }
    if (option != 1) {
	usage(argv[0]);
	return 34;
    }

    trc_init_state(&trace_state);
    if (! trc_open_infile(&trace_state, in_file)) {
	fprintf(stderr, "%s: illegal trace file %s\n", argv[0], in_file);
	return 1;
    }
    if (! trc_state_read(&trace_state)) {
	fprintf(stderr, "%s: no state block\n", argv[0]);
	return 1;
    }

    trc_filter_start(&trace_state);
    trc_create_filter(argc, argv);

    if (rebind_file != NULL) {
	n_ev = trc_read_obj_ev_file(rebind_file, &obj_ev_name);
	if (n_ev == -1) {
	    perror("No such object/operation file");
	    return 5;
	}
	trace_state.rebind = trc_init_rebind();
	trc_mk_obj_ev(trace_state.event_list, trace_state.rebind, obj_ev_name,
		      n_ev, TRUE);
	pan_free(obj_ev_name);
    }

				/* empty the event type sets */
    n_ev_tp = trc_event_lst_num(trace_state.event_list);
    for (pe = 0; pe < trace_state.n_pids; pe++) {
	n_threads = trace_state.thread[pe].n_threads;
	for (thr = 0; thr < n_threads; ++thr) {
	    trc_event_tp_set_clear(trace_state.thread[pe].threads[thr].event_tp_set);
	    trace_state.thread[pe].threads[thr].event_tp_set =
		trc_event_tp_set_create(n_ev_tp);
	}
    }

    trc_open_outfile(&trace_state, out_file, OUT_BUF_SIZE, pseudo_thread);

    trc_event_descr_init(&e);
    while (trc_read_next_event(&trace_state, &e)) {
	if (trc_filter_event_match(&e, trace_state.rebind)) {
	    pe = e.thread_id.my_pid;
	    thr = e.thread_id.my_thread;
				/* refill the event type set */
	    trc_set(trace_state.thread[pe].threads[thr].event_tp_set, e.type);
				/* write event */
	    trc_write_next_event(&trace_state, &e);
	}
	trc_event_descr_clear(&e);
    }

    trc_state_write(&trace_state);

    trc_close_files(&trace_state);

    trc_filter_end();

    return 0;
}
