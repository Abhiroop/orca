/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Filter on:
 *  - event type
 *  - processor
 *  - thread
 *  - object:
 *	- type
 *	- instantiation
 *	- operation
 */

/* Syntax:
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
#include <limits.h>
#include <string.h>

#include "pan_trace.h"
#include "trc_event_tp.h"
#include "trc_bind_op.h"
#include "trc_filter.h"

#include "pan_util.h"


typedef enum FILTER_DISPLAY_T {
    HIDE,
    SHOW
} filter_display_t, *filter_display_p;


typedef struct USR_SEL_T usr_sel_t, *usr_sel_p;

struct USR_SEL_T {
    void	     *data;
    char              name[256];
};


static const int		ANY_THREAD		= -2;
static const int		THREAD_SPECF_EVENT_TP	= -1;

static const int		ANY_PE			= -1;

static const trc_event_t	ANY_EVENT_TYPE		= -2;
static const trc_event_t	ANY_OBJECT_EVENT	= -1;

static trc_p global_trc;




typedef struct OBJ_OP_SEL_T sel_obj_op_t, *sel_obj_op_p;

struct OBJ_OP_SEL_T {
    short int		type;
    short int		operation;
    trc_object_id_p	object;
    char                name[256];
};


static const int        ANY_OBJ_TYPE		= -2;
static const trc_object_id_p   ANY_OBJECT	= NULL;
static const int	ANY_OPERATION		= -1;


/* Global variables for communication between different buttons/windows */



typedef struct EVENT_SEL_T event_sel_t, *event_sel_p;

struct EVENT_SEL_T {
    filter_display_t	display;
    trc_event_descr_t	event;
    trc_event_t		thread_logged_event;
    union {
	usr_sel_t         usr;
	sel_obj_op_t      obj_op;
    }			u;
    event_sel_p		next;
    int                 level;
    char                name[256];
};


/* Global variables for communication between different buttons/windows */

static event_sel_p      the_filter;	/* The current filter stack*/


static filter_display_t default_display = HIDE;	/* default action at bottom of
						 * filter stack */








static boolean
preset_obj_op(char *s, event_sel_p new_filter)
{
    int             tp;
    int             op;

    if (strcmp(s, "any") == 0) {
	new_filter->u.obj_op.operation = ANY_OPERATION;
	return TRUE;
    }

    if (new_filter->u.obj_op.type == ANY_OBJ_TYPE) {
	fprintf(stderr, "No operation \"%s\" on object of type \"any\"\n", s);
	return FALSE;
    }

    tp = new_filter->u.obj_op.type;

    op = trc_rb_first_op(global_trc->rebind, tp);
    while (op != TRC_NO_OBJ_TYPE) {
	if (strcmp(s, trc_rb_op_name(global_trc->rebind, tp, op)) == 0) {
	    new_filter->u.obj_op.operation = op;
	    return TRUE;
	}

	op = trc_rb_next_op(global_trc->rebind, tp, op);
    }

    fprintf(stderr, "No operation \"%s\" on object of type \"%s\"\n",
	    s, trc_rb_obj_type_name(global_trc->rebind, tp));
    return FALSE;
}





static boolean
preset_obj_id(char *s, event_sel_p new_filter)
{
    int             tp;
    trc_object_id_p obj;

    if (strcmp(s, "any") == 0) {
	new_filter->u.obj_op.object = ANY_OBJECT;
	return TRUE;
    }

    if (new_filter->u.obj_op.type == ANY_OBJ_TYPE) {
	fprintf(stderr, "No object \"%s\" of type \"any\"\n", s);
	return FALSE;
    }

    tp = new_filter->u.obj_op.type;

    obj = trc_rb_first_obj(global_trc->rebind, tp);
    while (obj != NULL) {
	if (strcmp(s, trc_rb_obj_name(global_trc->rebind, obj)) == 0) {
	    new_filter->u.obj_op.object = obj;
	    return TRUE;
	}

	obj = trc_rb_next_obj(global_trc->rebind, tp, obj);
    }

    fprintf(stderr, "No object \"%s\" of type \"%s\"\n",
	    s, trc_rb_obj_type_name(global_trc->rebind, tp));

    return FALSE;
}


static boolean
preset_obj_type(char *s, event_sel_p new_filter)
{
    int             tp;

    if (strcmp(s, "any") == 0) {
	new_filter->u.obj_op.type = ANY_OBJ_TYPE;
	return TRUE;
    }

    tp = trc_rb_first_obj_type(global_trc->rebind);
    while (tp != TRC_NO_OBJ_TYPE) {

	if (strcmp(s, trc_rb_obj_type_name(global_trc->rebind, tp)) == 0) {
	    new_filter->u.obj_op.type = tp;
	    return TRUE;
	}

	tp = trc_rb_next_obj_type(global_trc->rebind, tp);
    }

    return FALSE;
}



static boolean
preset_object(int argc, char *argv[], event_sel_p new_filter)
{
    int             i;
    char           *s;
    char           *arg;
    char            my_argv[]  = "object=";
    char            type_str[] = "type:";
    char            obj_str[]  = "obj:";
    char            op_str[]   = "op:";

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    break;
	}
    }

    if (i == argc) {
	return TRUE;
    }

    arg = strchr(argv[i], '=') + 1;

    new_filter->u.obj_op.object    = ANY_OBJECT;
    new_filter->u.obj_op.operation = ANY_OPERATION;

    if ((s = strstr(arg, type_str)) != NULL) {
	s = strchr(s, ':') + 1;
	if (! preset_obj_type(s, new_filter)) {
	    return FALSE;
	}
    }

    if ((s = strstr(arg, obj_str)) != NULL) {
	s = strchr(s, ':') + 1;
	if (! preset_obj_id(s, new_filter)) {
	    return FALSE;
	}
    }

    if ((s = strstr(arg, op_str)) != NULL) {
	s = strchr(s, ':') + 1;
	if (! preset_obj_op(s, new_filter)) {
	    return FALSE;
	}
    }

    return TRUE;
}




static boolean
preset_event_type(int argc, char *argv[], event_sel_p new_filter)
{
    trc_event_t     e;
    int             i;
    int             n;
    char           *s;
    char            my_argv[] = "event=";
    trc_event_lst_p ev_lst;

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    break;
	}
    }

    if (i == argc) {
	return TRUE;
    }

			/* Value specified in argc/argv */
    s = strchr(argv[i], '=') + 1;

    if (strcmp(s, "all") == 0) {
					/* The wildcard */
	new_filter->event.type = ANY_EVENT_TYPE;
	return TRUE;
    }
    
    if (strcmp(s, "all object + op") == 0) {
					/* The obj/op wildcard */
	new_filter->event.type = ANY_OBJECT_EVENT;
	return TRUE;
    }

    ev_lst = global_trc->event_list;
    n = trc_event_lst_num(ev_lst);
    for (e = 0; e < n; e++) {
	if (strcmp(trc_event_lst_name(ev_lst, e), s) == 0) {
	    new_filter->event.type = e;
	    return TRUE;
	}
    }

    return FALSE;
}



static boolean
preset_thread_logged(char *s, event_sel_p new_filter)
{
    trc_event_t     e;
    int             n;
    trc_event_lst_p ev_lst;

    if (strcmp(s, "all") == 0) {
				    /* The wildcard */
	new_filter->thread_logged_event = ANY_EVENT_TYPE;
	return TRUE;
    }
    
    if (strcmp(s, "all object + op") == 0) {
				    /* The obj/op wildcard */
	new_filter->thread_logged_event = ANY_OBJECT_EVENT;
	return TRUE;
    }

    ev_lst = global_trc->event_list;
    n = trc_event_lst_num(ev_lst);

    for (e = 0; e < n; e++) {
	if (strcmp(trc_event_lst_name(ev_lst, e), s) == 0) {
	    new_filter->thread_logged_event = e;
	    return TRUE;
	}
    }

    return FALSE;
}



static boolean
preset_thread_id(int argc, char *argv[], event_sel_p new_filter)
{
    int                 i;
    char                my_argv[] = "thread=";
    int                 pe;
    int                 thr;
    trc_thread_info_p   thread;
    char               *s;
    char                log_str[] = "logged:";
    boolean             is_first;
    event_sel_p         next_filter;
    event_sel_p         tail_filter;

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    break;
	}
    }

    if (i == argc) {
	return TRUE;
    }
				/* Value specified in argc/argv */
    s = strchr(argv[i], '=') + 1;

    if (strncmp(s, log_str, strlen(log_str)) == 0) {
	s = strchr(argv[i], ':') + 1;
	return preset_thread_logged(s, new_filter);
    }

    pe = new_filter->event.thread_id.my_pid;

    if (pe == ANY_PE) {
	
	is_first = TRUE;

	for (pe = 0; pe < global_trc->n_pids; pe++) {
	    thread = global_trc->thread[pe].threads;
	    thr = 0;

	    while (thread != NULL) {
		if (strcmp(thread->name, s) == 0) {
		    if (is_first) {
			next_filter = new_filter;
		    } else {
			next_filter = pan_malloc(sizeof(event_sel_t));
			*next_filter = *new_filter;
			tail_filter->next = next_filter;
			next_filter->next = NULL;
		    }
		    tail_filter = next_filter;
		    next_filter->event.thread_id.my_pid    = pe;
		    next_filter->event.thread_id.my_thread = thr;
		    is_first = FALSE;
		    break;
		}

		thread = thread->next_thread;
		++thr;
	    }
	}

	return ! is_first;

    } 

    thread = global_trc->thread[pe].threads;
    thr = 0;
    while (thread != NULL) {

	if (strcmp(thread->name, s) == 0) {
	    new_filter->event.thread_id.my_thread = thr;
	    return TRUE;
	}

	thread = thread->next_thread;
	++thr;
    }

    return FALSE;
}



static boolean
preset_pe_number(int argc, char *argv[], event_sel_p new_filter)
{
    int             i;
    char            my_argv[] = "pe=";
    int             pe;
    char           *s;

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    break;
	}
    }

    if (i == argc) {	/* No value specified in argc/argv */
	return TRUE;
    }

    s = strchr(argv[i], '=') + 1;

    if (strcmp(s, "all") == 0) {
	new_filter->event.thread_id.my_pid = ANY_PE;
	return TRUE;
    }
    
    if (sscanf(s, "%d", &pe) == 1 && pe >= 0 && pe < global_trc->n_pids) {
	new_filter->event.thread_id.my_pid = pe;
	return TRUE;
    }

    return FALSE;
}



static boolean
preset_level(int argc, char *argv[], event_sel_p new_filter)
{
    int             i;
    char            my_argv[] = "level=";
    char           *s;

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    break;
	}
    }

    if (i == argc) {
	return TRUE;
    }

		    /* Value specified in argc/argv */
    s = strchr(argv[i], '=') + 1;
    if (sscanf(s, "%d", &new_filter->level) != 1) {
	return FALSE;
    }

    return TRUE;
}



static boolean
preset_mode(int argc, char *argv[], event_sel_p new_filter)
{
    int             i;
    char            my_argv[] = "hide";

    for (i = 0; i < argc; i++) {
	if (strncmp(argv[i], my_argv, strlen(my_argv)) == 0) {
	    new_filter->display = HIDE;
	    break;
	}
    }

    return TRUE;
}



static void
do_set_any_filter(event_sel_p new_filter)
{
    new_filter->display                   = SHOW;
    new_filter->level                     = 0;

    new_filter->event.type                = ANY_EVENT_TYPE;
    new_filter->event.thread_id.my_pid    = ANY_PE;
    new_filter->event.thread_id.my_thread = ANY_THREAD;
    new_filter->thread_logged_event       = ANY_EVENT_TYPE;

    new_filter->u.obj_op.type             = ANY_OBJ_TYPE;
    new_filter->u.obj_op.operation        = ANY_OPERATION;
    new_filter->u.obj_op.object           = ANY_OBJECT;

    new_filter->next                      = NULL;
}



static void
do_add_filter(event_sel_p new_filter)
{
    while (new_filter->next != NULL) {
	new_filter = new_filter->next;
    }
				/* Prepend current filter */
    new_filter->next = the_filter;
    the_filter       = new_filter;
}





static boolean
strip_matching(char *s, int len, char lm, char rm)
{
    char *lp;
    char *rp;
    char *p;
    char *end;
    int   inner = 0;

    lp = strchr(s, lm);
    if (lp == NULL) {
	return FALSE;
    }

    end = s + len;

    for (rp = lp+1; rp < end; rp++) {
	if (*rp == rm) {
	    if (inner == 0) {
		break;
	    } else {
		--inner;
	    }
	} else if (*rp == lm) {
	    ++inner;
	}
    }

    if (rp == end) {
	fprintf(stderr, "Non-matching '%c' in string %s\n", lm, s);
	abort();
    }

    for (p = lp; p < rp - 1; p++) {
	*p = *(p+1);
    }
    for (p = rp - 1; p < end - 1; p++) {
	*p = *(p+2);
    }

    return TRUE;
}


static void
strip_outer_quotes(char *s)
{
    int n;

    n = strlen(s);
    if (! strip_matching(s, n, '\'', '\'')) {
	strip_matching(s, n, '"', '"');
    }

}



void
trc_filter_start(trc_p trc)
{
    global_trc = trc;
}


void
trc_filter_end(void)
{
}


void
trc_create_filter(int argc, char *argv[])
{
    char  *filter_argstr;
    char  *s;
    int    i;
    int    arg;
    int    filter_argc;
    char **filter_argv;
    event_sel_p new_filter;

    the_filter = NULL;		/* Total wildcard */

				/* Parse the filter options; convert each of the
				 * mask strings into argc/argv format, and
				 * compile them into masks */
    for (arg = 1; arg < argc; arg++) {

	filter_argc = 0;

	if (strcmp(argv[arg], "-filter") == 0) {
	    ++arg;
	    filter_argstr = strdup(argv[arg]);
				/* Count the options */
	    s = filter_argstr - 1;
	    do {
		++filter_argc;
	    } while ((s = strchr(s + 1, ';')) != NULL);
	    filter_argv = pan_malloc(filter_argc * sizeof(char *));

				/* Fill in argv[i] to the separate options */
	    i = 0;
	    filter_argv[i++] = strtok(filter_argstr, ";");
	    while ((s = strtok(NULL, ";")) != NULL) {
		filter_argv[i++] = s;
	    }

	    for (i = 0; i < filter_argc; i++) {
		strip_outer_quotes(filter_argv[i]);
	    }
	}

	if (filter_argc != 0) {

	    new_filter = pan_malloc(sizeof(event_sel_t));
				/* Set new mask to default values */
	    do_set_any_filter(new_filter);

	    preset_mode(filter_argc, filter_argv, new_filter);
	    preset_level(filter_argc, filter_argv, new_filter);
	    preset_event_type(filter_argc, filter_argv, new_filter);
	    if (trc_is_rebind_op(global_trc->event_list, global_trc->rebind,
			         new_filter->event.type) != TRC_NO_SUCH_EVENT) {
		preset_object(filter_argc, filter_argv, new_filter);
	    }
	    if (! preset_pe_number(argc, argv, new_filter)) {
	    }
	    if (! preset_thread_id(argc, argv, new_filter)) {
	    }


	    do_add_filter(new_filter);
	}
    }

    new_filter = the_filter;
    if (new_filter != NULL) {
	while (new_filter->next != NULL) {
	    new_filter = new_filter->next;
	}
	if (new_filter->display != SHOW) {
	    default_display = SHOW;
	} else {
	    default_display = HIDE;
	}
    } else {
	default_display = SHOW;
    }
}




static boolean
sel_event_match(trc_event_descr_p e, event_sel_p sel)
{
    trc_thread_id_p se_p;
    trc_thread_id_p e_p;

    if (trc_event_lst_level(global_trc->event_list, e->type) < sel->level) {
	return FALSE;
    }

    e_p  = &e->thread_id;
    se_p = &sel->event.thread_id;

    return (se_p->my_pid == ANY_PE ||
	     (se_p->my_pid == e_p->my_pid &&
		(se_p->my_thread == ANY_THREAD ||
		 se_p->my_thread == e_p->my_thread)));
}


static boolean
sel_obj_op_match(trc_event_descr_p e, trc_rebind_p rb, event_sel_p sel)
{
    short int       tp;
    short int       op;
    trc_object_id_t obj;
    trc_event_t     ev_tp;

    ev_tp = sel->event.type;

    if (ev_tp == ANY_EVENT_TYPE)
	return TRUE;

    if (rb == NULL) {
	return (ev_tp == e->type && (sel->u.usr.data == NULL || TRUE));
    }

    if ((ev_tp == ANY_OBJECT_EVENT || ev_tp == e->type) &&
	trc_is_rebind_op(global_trc->event_list, rb, e->type) !=
		TRC_NO_SUCH_EVENT) {
	trc_obj_op_get(global_trc->event_list, rb, e, &tp, &op, &obj);
	return ( (sel->u.obj_op.type == ANY_OBJ_TYPE ||
		    sel->u.obj_op.type == tp) &&
		 (sel->u.obj_op.operation == ANY_OPERATION ||
		    sel->u.obj_op.operation == op) &&
		 (sel->u.obj_op.object == ANY_OBJECT ||
		    (sel->u.obj_op.object->cpu == obj.cpu &&
			sel->u.obj_op.object->addr == obj.addr)));
    }

    return (ev_tp == e->type && (sel->u.usr.data == NULL || TRUE));
}


boolean
trc_filter_thread_logged_events(trc_thread_id_p e_p, trc_rebind_p rb)
{
    event_sel_p     scan;
    trc_event_tp_set_p set;
    trc_event_t     e;
    trc_event_lst_p ev_lst;
    trc_thread_id_p se_p;

    if (the_filter == NULL) {
	return TRUE;
    }

    set = global_trc->thread[e_p->my_pid].threads[e_p->my_thread].event_tp_set;
    ev_lst = global_trc->event_list;

    scan = the_filter;
    while (scan != NULL) {

	se_p = &scan->event.thread_id;

	if (se_p->my_pid == ANY_PE ||
		(se_p->my_pid == e_p->my_pid &&
		    (se_p->my_thread == ANY_THREAD ||
		     se_p->my_thread == e_p->my_thread))) {

	    if (scan->thread_logged_event == ANY_EVENT_TYPE) {

		e = trc_event_lst_first(ev_lst);
		while (e != -1) {
		    if (trc_event_lst_level(ev_lst, e) >= scan->level &&
			trc_is_set(set, e) && scan->display != HIDE) {

			return TRUE;
		    }
		    e = trc_event_lst_next(ev_lst, e);
		}

	    } else if (scan->thread_logged_event == ANY_OBJECT_EVENT) {

		e = trc_event_lst_first(ev_lst);
		while (e != -1) {
		    if (trc_event_lst_level(ev_lst, e) >= scan->level &&
			trc_is_rebind_op(ev_lst, rb, e) != TRC_NO_SUCH_EVENT &&
			trc_is_set(set, e) && scan->display != HIDE) {

			return TRUE;
		    }

		    e = trc_event_lst_next(ev_lst, e);
		}

	    } else if (trc_event_lst_level(ev_lst, scan->thread_logged_event) >=
			    scan->level &&
		       trc_is_set(set, scan->thread_logged_event) &&
		       scan->display != HIDE) {

		return TRUE;
	    }
	}

	scan = scan->next;
    }

    return FALSE;
}


boolean
trc_filter_event_match(trc_event_descr_p e, trc_rebind_p rb)
{
    event_sel_p scan;

    if (the_filter == NULL) {		/* total wildcard */
	return TRUE;
    }

    scan = the_filter;
    while (scan != NULL) {
	if (sel_event_match(e, scan) && sel_obj_op_match(e, rb, scan)) {
	    return scan->display != HIDE;
	}
	scan = scan->next;
    }

    return (default_display == SHOW);
}
