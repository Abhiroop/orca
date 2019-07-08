#include <assert.h>
#include <string.h>

#include "pan_util.h"

#include "trc_types.h"
#include "trc_lib.h"
#include "trc_io.h"
#include "trc_bind_op.h"

#include "states.h"


typedef struct PROC_THR_T proc_thr_t, *proc_thr_p;

struct PROC_THR_T {
    trc_thread_info_p *thread;
    int                n_threads;
    int                thread_idx;
};


typedef struct OP_COUNT_T {
    int		       *count;
    int                *state_count;
    pan_time_fix_p	state_time;
    pan_time_fix_p	state_start;
} op_count_t, *op_count_p;


typedef struct OP_DESCR_T {
    short int           op;
    op_count_t	      **stats;
} op_descr_t, *op_descr_p;


typedef struct OBJECT_DESCR_T {
    trc_object_id_t	object;
    int                 n_ops;
    op_descr_t         *ops;
} object_descr_t, *object_descr_p;


static trc_t trace_state;

static int   column_width = 10;
static int   thread_width = 30;


static object_descr_t **
create_accu(short int *object_type, int *n_objects, trc_event_t *rebind_ev,
	    int n_ev)
{
    int                pe;
    int                thr;
    int                n_thr;
    trc_event_lst_p    lst;
    trc_rebind_p       rb;
    short int          tp;
    int                tp_id;
    int                n_obj_tp;
    trc_object_id_p    obj;
    object_descr_p     p_obj;
    int                id;
    int                op_id;
    short int          op;
    op_descr_p         p_op;
    op_count_t        *p_cnt;
    int                n_states;
    object_descr_t   **op_count;
    int                ev;

    rb = trace_state.rebind;
    lst = trace_state.event_list;
    n_states = num_states();
    n_obj_tp = trc_rb_n_obj_types(rb);

    op_count = pan_malloc(n_obj_tp * sizeof(object_descr_t *));

    tp = trc_rb_first_obj_type(rb);
    for (tp_id = 0; tp_id < n_obj_tp; tp_id++) {

	op_count[tp_id] = pan_malloc(n_objects[tp_id] * sizeof(object_descr_t));

	obj = trc_rb_first_obj(rb, tp);
	for (id = 0; id < n_objects[tp_id]; id++) {

	    p_obj = &op_count[tp_id][id];
	    p_obj->object = *obj;
	    p_obj->n_ops  = trc_rb_n_ops(rb, tp);
	    p_obj->ops    = pan_malloc(p_obj->n_ops * sizeof(op_descr_t));

	    op = trc_rb_first_op(rb, tp);
	    for (op_id = 0; op_id < p_obj->n_ops; op_id++) {

		p_op = &p_obj->ops[op_id];
		p_op->op = op;
		p_op->stats = pan_malloc(trace_state.n_pids *
					 sizeof(op_count_t *));

		for (pe = 0; pe < trace_state.n_pids; pe++) {

		    n_thr = trace_state.thread[pe].n_threads;
		    p_op->stats[pe] = pan_malloc(n_thr * sizeof(op_count_t));

		    for (thr = 0; thr < n_thr; thr++) {

			p_op->stats[pe][thr].count = pan_malloc(n_ev *
								sizeof(int));
			p_cnt = &p_op->stats[pe][thr];
			for (ev = 0; ev < n_ev; ev++) {
			    p_cnt->count[ev] = 0;
			}

			if (n_states > 0) {
			    p_cnt->state_count = pan_calloc(n_states,
						       sizeof(int));
			    p_cnt->state_time  = pan_calloc(n_states,
						       sizeof(pan_time_fix_t));
			    p_cnt->state_start = pan_calloc(n_states,
						       sizeof(pan_time_fix_t));
			}
		    }
		}

		op = trc_rb_next_op(rb, tp, op);
	    }
	    assert(op == TRC_NO_OPERATION);

	    obj = trc_rb_next_obj(rb, tp, obj);
	}
	assert(obj == NULL);

	tp = trc_rb_next_obj_type(rb, tp);
    }
    assert(tp == TRC_NO_OBJ_TYPE);

    return op_count;
}



static void
accu_counts(object_descr_t **op_count, int *n_objects, short int *object_type,
	    trc_event_t *rebind_ev, int n_ev)
{
    trc_event_descr_t  e;
    int                pe;
    int                thr;
    int                n;
    trc_event_lst_p    lst;
    trc_rebind_p       rb;
    short int          tp;
    int                tp_id;
    int                n_obj_tp;
    trc_object_id_t    object;
    object_descr_p     p_obj;
    int                id;
    int                op_id;
    short int          op;
    op_count_p         p_cnt;
    state_p            state;
    int                state_id;
    int                n_states;
    pan_time_fix_t     t;
    int                ev;

				/* Accumulate the values */
    rb = trace_state.rebind;
    lst = trace_state.event_list;
    n_states = num_states();
    n_obj_tp = trc_rb_n_obj_types(rb);

    n = 0;
    fprintf(stderr, "[");

    trc_event_descr_init(&e);
    while (trc_read_next_event(&trace_state, &e)) {

	++n;
	if (n % 1000 == 0) {
	    fprintf(stderr, ".");
	}

	if (trc_is_rebind_op(lst, rb, e.type) == TRC_NO_SUCH_EVENT) {
	    continue;
	}

	pe = e.thread_id.my_pid;
	thr = e.thread_id.my_thread;
	assert(pe >= 0);
	assert(pe < trace_state.n_pids);
	assert(thr >= 0);
	assert(thr < trace_state.thread[pe].n_threads);

	trc_obj_op_get(lst, rb, &e, &tp, &op, &object);

				/* find its object type */
	for (tp_id = 0; tp_id < n_obj_tp; tp_id++) {
	    if (object_type[tp_id] == tp) {
		break;
	    }
	}
	assert(tp_id < n_obj_tp);

				/* find its object id */
	for (id = 0; id < n_objects[tp_id]; id++) {
	    if (trc_object_id_eq(&op_count[tp_id][id].object, &object)) {
		break;
	    }
	}
	assert(id < n_objects[tp_id]);

				/* find its operation */
	p_obj = &op_count[tp_id][id];
	for (op_id = 0; op_id < p_obj->n_ops; op_id++) {
	    if (p_obj->ops[op_id].op == op) {
		break;
	    }
	}
	assert(op_id < p_obj->n_ops);

				/* find the event */
	for (ev = 0; ev < n_ev; ev++) {
	    if (rebind_ev[ev] == e.type) {
		break;
	    }
	}
	assert(ev < n_ev);

				/* update the count */
	p_cnt = &p_obj->ops[op_id].stats[pe][thr];
	++p_cnt->count[ev];

				/* check for the state */
	state_id = 0;
	for (state = first_state(); state != NULL; state = next_state(state)) {

	    if (e.type == state->start) {
				/* save start time until stop event */
		++p_cnt->state_count[state_id];
		p_cnt->state_start[state_id] = e.t;

		break;			/* event -> state unique */
	    } else if (e.type == state->end) {
				/* calculate delta-t and accumulate time */
		t = e.t;
		pan_time_fix_sub(&t, &p_cnt->state_start[state_id]);
		pan_time_fix_add(&p_cnt->state_time[state_id], &t);

		break;			/* event -> state unique */
	    }

	    ++state_id;
	}
    }

    fprintf(stderr, "]\n");

    trc_event_descr_clear(&e);
}



static boolean
print_op_count(char *buf, int pe, int thr, op_count_p p_cnt,
	       trc_event_t *rebind_ev, int n_ev)
{
    int                ev;
    int                n;
    char              *name;

    for (ev = 0; ev < n_ev; ev++) {
	if (p_cnt->count[ev] > 0) {
	    break;
	}
    }
    if (ev == n_ev) {
	return FALSE;
    }

    name = trace_state.thread[pe].threads[thr].name;
    n = strlen(name);
    if (n > thread_width) {
	name += n - thread_width;
    }

    buf = strchr(buf, '\0');

    sprintf(buf, "        pe %2d %-*s : ", pe, thread_width, name);
    buf = strchr(buf, '\0');
    for (ev = 0; ev < n_ev; ev++) {
	if (p_cnt->count[ev] > 0) {
	    sprintf(buf, " %*d", column_width, p_cnt->count[ev]);
	} else {
	    sprintf(buf, "%*s", column_width + 1, "");
	}
	buf = strchr(buf, '\0');
    }
    sprintf(buf, "\n");

    return TRUE;
}


static void
print_counts(object_descr_t **op_count, int *n_objects, trc_event_t *rebind_ev,
	     int n_ev)
{
    int                pe;
    int                thr;
    int                n_thr;
    trc_event_lst_p    lst;
    trc_rebind_p       rb;
    int                tp;
    int                tp_id;
    trc_object_id_p    obj;
    object_descr_p     p_obj;
    int                id;
    int                op_id;
    int                op;
    op_count_p         p_cnt;
    op_descr_p         p_op;
    int                n_obj_tp;
    int                ev;
    char              *buf;

    rb = trace_state.rebind;
    lst = trace_state.event_list;
    n_obj_tp = trc_rb_n_obj_types(rb);

    buf = pan_malloc(column_width + 1);
    printf("%-*s :   ", 14 + thread_width, "");
    for (ev = 0; ev < n_ev; ev++) {
	strncpy(buf, trc_event_lst_name(lst, rebind_ev[ev]), column_width);
	buf[column_width] = '\0';
	printf("|%*s", column_width, buf);
    }
    printf("|\n");
    pan_free(buf);

    buf = pan_malloc(2 * (22 + thread_width + n_ev * (column_width + 1)));
    tp = trc_rb_first_obj_type(rb);
    for (tp_id = 0; tp_id < n_obj_tp; tp_id++) {

	obj = trc_rb_first_obj(rb, tp);
	for (id = 0; id < n_objects[tp_id]; id++) {

	    printf("%s:\n", trc_rb_obj_name(rb, &op_count[tp_id][id].object));

	    p_obj = &op_count[tp_id][id];
	    op = trc_rb_first_op(rb, tp);
	    for (op_id = 0; op_id < p_obj->n_ops; op_id++) {

		p_op = &op_count[tp_id][id].ops[op_id];

		sprintf(buf, "    %s:\n", trc_rb_op_name(rb, tp, p_op->op));
		for (pe = 0; pe < trace_state.n_pids; pe++) {

		    n_thr = trace_state.thread[pe].n_threads;
		    for (thr = 0; thr < n_thr; thr++) {
			p_cnt = &p_op->stats[pe][thr];
			if (print_op_count(buf, pe, thr, p_cnt,
					   rebind_ev, n_ev)) {
			    printf("%s", buf);
			    buf[0] = '\0';
			}
		    }
		}

		op = trc_rb_next_op(rb, tp, op);
	    }
	    assert(op == TRC_NO_OPERATION);

	    obj = trc_rb_next_obj(rb, tp, obj);
	}
	assert(obj == NULL);

	tp = trc_rb_next_obj_type(rb, tp);
    }

    pan_free(buf);

    assert(tp == TRC_NO_OBJ_TYPE);


}


static boolean
print_state_count(char *buf, int pe, int thr, op_count_p p_cnt)
{
    state_p            state;
    int                i;
    int                n_states;
    int                n;
    char              *name;

    n_states = num_states();

    state = first_state();
    for (i = 0; i < n_states; i++) {
	if (p_cnt->state_count[i] > 0) {
	    break;
	}
	state = next_state(state);
    }

    if (i == n_states) {
	return FALSE;
    }

    name = trace_state.thread[pe].threads[thr].name;
    n = strlen(name);
    if (n > thread_width) {
	name += n - thread_width;
    }

    buf = strchr(buf, '\0');

    sprintf(buf, "        pe %2d %-*s : ", pe, thread_width, name);
    buf = strchr(buf, '\0');

    state = first_state();
    for (i = 0; i < n_states; i++) {
	if (p_cnt->state_count[i] > 0) {
	    sprintf(buf, " %*d %*.3f",
		    column_width, p_cnt->state_count[i],
		    column_width, pan_time_fix_t2d(&p_cnt->state_time[i]));
	} else {
	    sprintf(buf, "%*s", 2 * column_width + 2, "");
	}
	buf = strchr(buf, '\0');
	state = next_state(state);
    }
    sprintf(buf, "\n");

    return TRUE;
}


static void
print_states(object_descr_t **op_count, int *n_objects, trc_event_t *rebind_ev,
	     int n_ev)
{
    int                pe;
    int                thr;
    int                n_thr;
    trc_event_lst_p    lst;
    trc_rebind_p       rb;
    int                tp;
    int                tp_id;
    trc_object_id_p    obj;
    object_descr_p     p_obj;
    int                id;
    int                op_id;
    int                op;
    op_count_p         p_cnt;
    state_p            state;
    int                n_states;
    op_descr_p         p_op;
    int                n_obj_tp;
    char              *buf;

    rb = trace_state.rebind;
    lst = trace_state.event_list;
    n_states = num_states();
    n_obj_tp = trc_rb_n_obj_types(rb);

    printf("%-*s :   ", 14 + thread_width, "STATES");
    buf = pan_malloc(2 * column_width + 2);
    state = first_state();
    while (state != NULL) {
	strncpy(buf, state->name, 2 * column_width + 1);
	buf[2 * column_width + 1] = '\0';
	printf("|%*s", 2 * column_width + 1, buf);
	state = next_state(state);
    }
    printf("|\n");
    pan_free(buf);

    printf("%-*s : ", 14 + thread_width, "STATES");
    state = first_state();
    while (state != NULL) {
	printf(" %*s %*s", column_width, "state", column_width, "time");
	state = next_state(state);
    }
    printf("\n");

    buf = pan_malloc(2 * (22 + thread_width +
			  2 * n_states * (column_width + 1)));

    tp = trc_rb_first_obj_type(rb);
    for (tp_id = 0; tp_id < n_obj_tp; tp_id++) {

	obj = trc_rb_first_obj(rb, tp);
	for (id = 0; id < n_objects[tp_id]; id++) {

	    printf("%s:\n", trc_rb_obj_name(rb, &op_count[tp_id][id].object));

	    p_obj = &op_count[tp_id][id];
	    op = trc_rb_first_op(rb, tp);
	    for (op_id = 0; op_id < p_obj->n_ops; op_id++) {

		p_op = &op_count[tp_id][id].ops[op_id];

		sprintf(buf, "    %s:\n", trc_rb_op_name(rb, tp, p_op->op));
		for (pe = 0; pe < trace_state.n_pids; pe++) {

		    n_thr = trace_state.thread[pe].n_threads;
		    for (thr = 0; thr < n_thr; thr++) {
			p_cnt = &p_op->stats[pe][thr];
			if (print_state_count(buf, pe, thr, p_cnt)) {
			    printf("%s", buf);
			    buf[0] = '\0';
			}
		    }
		}

		op = trc_rb_next_op(rb, tp, op);
	    }
	    assert(op == TRC_NO_OPERATION);

	    obj = trc_rb_next_obj(rb, tp, obj);
	}
	assert(obj == NULL);

	tp = trc_rb_next_obj_type(rb, tp);
    }

    pan_free(buf);

    assert(tp == TRC_NO_OBJ_TYPE);


}



int
main(int argc, char *argv[], char **start_ev, char **end_ev)
{
    int                i;
    int                arg_start;
    char              *trc_file = NULL;
    char              *rebind_file = NULL;
    char              *state_file = NULL;
    trc_ev_name_p      obj_ev_names;
    int                n_ev = 0;
    trc_rebind_p       rb;
    short int          tp;
    int                tp_id;
    int                n_obj_tp;
    trc_object_id_p    obj;
    int                n_obj;
    object_descr_t   **op_count;
    short int         *object_type;
    trc_event_lst_p    lst;
    int               *n_objects;
    trc_event_t       *rebind_ev = NULL;

    arg_start = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	    case 'e' :	++i;
			rebind_file = argv[i];
			break;
	    case 's' :	++i;
			state_file = argv[i];
			break;
	    case 'w' :	if (sscanf(&argv[i][2], "%d", &column_width) != 1 &&
			    sscanf(argv[++i], "%d", &column_width) != 1) {
			    --i;
			    fprintf(stderr, "-w <column_width> should be int\n");
			}
			break;
	    default:	fprintf(stderr, "No such option: %s\n", argv[i]);
	    }
	} else {
	    switch (arg_start) {
	    case 0:	trc_file = argv[i];
			arg_start++;
			break;
	    default:	fprintf(stderr,
				"%s: no such argument: %s\n", argv[0], argv[i]);
	    		fprintf(stderr, "usage: %s [options] <log file>\n",
				argv[0]);
	    		fprintf(stderr, "options\n");
	    		fprintf(stderr, "\t-e <rebind file>\n");
	    		fprintf(stderr, "\t-s <state file>\n");
	    		fprintf(stderr, "\t-w <field width\n");
			exit(1);
	    }
	}
    }
    if (arg_start != 1) {
	fprintf(stderr, "usage: %s [options] <log file>\n", argv[0]);
	exit(1);
    }

    trc_init_state(&trace_state);
    if (! trc_open_infile(&trace_state, trc_file)) {
        fprintf(stderr, "ERROR: Cannot open trace file \"%s\"\n", trc_file);
        exit(5);
    }
    if (! trc_state_read(&trace_state)) {
        fprintf(stderr, "ERROR: Cannot read trace state \"%s\"\n", trc_file);
        exit(5);
    }

    lst = trace_state.event_list;

    if (rebind_file != NULL) {
	if (trace_state.rebind == NULL) {
	    trace_state.rebind = trc_init_rebind();
	}
	n_ev = trc_read_obj_ev_file(rebind_file, &obj_ev_names);
	trc_mk_obj_ev(trace_state.event_list, trace_state.rebind, obj_ev_names,
		      n_ev, FALSE);
	rebind_ev = pan_malloc(n_ev * sizeof(trc_event_t));
	for (i = 0; i < n_ev; i++) {
	    rebind_ev[i] = trc_event_lst_find(lst, obj_ev_names[i]);
	}
	pan_free(obj_ev_names);
    }

    start_state_module();
    if (state_file != NULL) {
	ReadStatefile(trace_state.event_list, state_file);
    }

    rb = trace_state.rebind;
    n_obj_tp = trc_rb_n_obj_types(rb);

    object_type = pan_malloc(n_obj_tp * sizeof(short int));
    n_objects   = pan_malloc(n_obj_tp * sizeof(int));
    tp_id = 0;
    for (tp = trc_rb_first_obj_type(rb); tp != TRC_NO_OBJ_TYPE;
	    tp = trc_rb_next_obj_type(rb, tp)) {

	object_type[tp_id] = tp;
				/* Calculate #objects of this type */
	n_obj = 0;
	for (obj = trc_rb_first_obj(rb, tp); obj != NULL;
		obj = trc_rb_next_obj(rb, tp, obj)) {
	    n_obj++;
	}
	n_objects[tp_id] = n_obj;

	tp_id++;
    }
    assert(tp_id == n_obj_tp);

				/* Create the accumulator array */
    op_count = create_accu(object_type, n_objects, rebind_ev, n_ev);
    accu_counts(op_count, n_objects, object_type, rebind_ev, n_ev);
    print_counts(op_count, n_objects, rebind_ev, n_ev);
    print_states(op_count, n_objects, rebind_ev, n_ev);

    fclose(trace_state.in_buf.stream);


    return 0;
}
