/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Main to rebind the numerical identifiers of operations to the text strings
 * to which the operations and the objects are mapped.
 *
 * ------------------
 * Version 1.2 rules:
 * ------------------
 *
 *    Given mappings:
 *		"create object"		"<type> %h <id> %h %p <name> %s"
 *		"operation mapping"	"<type> %h <operation> %h <name> %s"
 *		"object type mapping"	"<type> %d <name> %s"
 * Translate:
 *	"some event"	"....%h...%h.%p..."
 * to
 *	"some event"	"....%s....%s..."
 *	where "some event"[0] := "operation mapping"[2]
 *			     cond "operation mapping"[0] == "create object"[0]
 *				   cond "some event[1] == "create object[1]"
 *				     && "some event[2] == "create object[2]"
 *			       && "operation mapping"[1] == "some event"[1]
 *            "some event"[1] := "create object"[3]
 *				 cond "create object"[1] == "some event"[1]
 *				   && "create object"[2] == "some event"[2]
 *
 * IMPORTANT!
 *   The event-specific data format of the "some event"s MUST be in this form:
 *        -- the FIRST be a short int that designates an operation;
 *	  -- the SECOND field MUST be a short int that designates the cpu
 *           that created this object;
 *	  -- the THIRD field MUST be a pointer that designates the address op
 *	     the object at the cpu that created it.
 *   The FIRST, SECOND and THIRD parameter of "create object" and the FIRST,
 *   SECOND and THIRD parameter of "operation mapping" MUST be as indicated
 *   above.
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

#define ARRAY_SIZE 10
#define OUT_BUF_SIZE 10000




static void
get_obj_ev_names(int argc, char *argv[], trc_ev_name_p *obj_ev_name,
		 int *x_n_ev)
{
    int            max_ev = ARRAY_SIZE;
    int            n_ev;
    trc_ev_name_p  names;
    int        arg_start;
    int        i;

    if (argc > 2) {
	if (strcmp(argv[2], "-e") == 0) {
	    n_ev = trc_read_obj_ev_file(argv[3], &names);
	    if (n_ev == -1) {
		perror("No such object/operation file");
		exit(5);
	    }
	    max_ev = n_ev;
	    arg_start = 4;
	} else {
	    arg_start = 2;
	}
    } else {
	names = pan_calloc(max_ev, sizeof(trc_ev_name_t));
	n_ev = 0;
	arg_start = 1;
    }
    for (i = arg_start; i < argc; i++) {
	if (n_ev >= max_ev) {
	    max_ev *= 2;
	    names = pan_realloc(names, max_ev * sizeof(trc_ev_name_t));
	}
	strcpy(names[n_ev], argv[i]);
	++n_ev;
    }
    *obj_ev_name = names;
    *x_n_ev = n_ev;
}



int
main(int argc, char *argv[])
{
    trc_ev_name_p      obj_ev_name;
    int                n_ev;
    trc_t              trace_state;
    trc_event_descr_t  e;
    trc_thread_id_t    pseudo_thread = trc_no_such_thread;

    trc_init_state(&trace_state);
    if (! trc_open_infile(&trace_state, argv[1])) {
	fprintf(stderr, "%s: no trace file\n", argv[0]);
	exit(1);
    }
    if (! trc_state_read(&trace_state)) {
	fprintf(stderr, "%s: no state block\n", argv[0]);
	exit(1);
    }

    get_obj_ev_names(argc, argv, &obj_ev_name, &n_ev);
    trc_mk_obj_ev(trace_state.event_list, trace_state.rebind, obj_ev_name, n_ev,
		  TRUE);

    trc_open_outfile(&trace_state, NULL, OUT_BUF_SIZE, pseudo_thread);
    trc_state_write(&trace_state);

    while (trc_read_next_event(&trace_state, &e)) {
	trc_redef_obj_op(trace_state.rebind, &e);
	trc_rebind_op(trace_state.event_list, trace_state.rebind, &e);
	trc_write_next_event(&trace_state, &e);
	trc_event_descr_clear(&e);
    }

    trc_close_files(&trace_state);

    return 0;
}
