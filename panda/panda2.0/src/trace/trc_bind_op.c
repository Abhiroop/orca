/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Library to rebind the numerical identifiers of operations to the text strings
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
#include "trc2ascii.h"
#include "trc_align.h"
#include "trc_bind_op.h"



#define OUT_BUF_SIZE 1048576



const  int    TRC_NO_OBJ_TYPE	= -99;
const  int    TRC_NO_OPERATION	= -99;



typedef enum EV_FIELDS_T {
    EV_OPER		= 0,
    EV_OBJ_CPU		= 1,
    EV_OBJ_ADDR		= 2,
    EV_PAST_FIELDS	= 3
} ev_fields_t, *ev_fields_p;


typedef enum CREATE_FIELDS_T {
    CR_OBJ_TYPE		= 0,
    CR_OBJ_CPU		= 1,
    CR_OBJ_ADDR		= 2,
    CR_NAME		= 3
} create_fields_t, *create_fields_p;


typedef enum OPER_FIELDS_T {
    MAP_OBJ_TYPE	= 0,
    MAP_OPER		= 1,
    MAP_OP_NAME		= 2
} oper_fields_t, *oper_fields_p;


typedef enum OBJ_TYPE_FIELDS_T {
    MAP_TYPE_ID		= 0,
    MAP_TYPE_NAME	= 1
} obj_type_fields_t, *obj_type_fields_p;


typedef struct OBJECT_T object_t, *object_p;

struct OBJECT_T {
    short int		type;
    trc_object_id_t	id;
    trc_ev_name_t	name;
};

typedef struct OPER_T oper_t, *oper_p;

struct OPER_T {
    short int		op;
    trc_ev_name_t	name;
};


typedef struct EV_PAIR_T ev_pair_t, *ev_pair_p;

struct EV_PAIR_T {
    trc_event_t old;
    trc_event_t new;
};


typedef struct OBJ_TYPE_T obj_type_t, *obj_type_p;

struct OBJ_TYPE_T {
    int             id;
    trc_ev_name_t   name;
    oper_p          oper;
    size_t          n_oper;
    size_t          max_oper;
    obj_type_p      next;
};




/* typedef struct TRC_REBIND_T trc_rebind_t, *trc_rebind_p; */

struct TRC_REBIND_T {
    obj_type_p  obj_types;
    int         n_obj_types;
    object_p    object;
    size_t      n_object;
    size_t      max_object;
    ev_pair_p   obj_ev;
    size_t      n_obj_ev;
    size_t      max_obj_ev;
    trc_event_t obj_cr;
    trc_event_t map_op;
    trc_event_t obj_tp;
    char       *obj_cr_fmt;
    char       *map_op_fmt;
    char       *obj_tp_fmt;
    boolean     is_old_format;
};


#define ARRAY_SIZE 10




static void
resize_array(size_t entry, size_t *size, void *array[], size_t elt_size)
{
    int         old_size;
    int         new_size = *size;

    old_size = new_size;
    while (entry >= new_size) {
	new_size *= 2;
    }
    if (old_size != new_size) {
	*array = pan_realloc(*array, new_size * elt_size);
	*size = new_size;
	memset(((char *)(*array)) + old_size * elt_size, '\0',
	       (new_size - old_size) * elt_size);
    }
}


static int
fscan_upto(FILE *fp, char str[], char sentinel)
{
    char        c;

    while (TRUE) {
	c = getc(fp);
	if (c == EOF) {
	    return EOF;
	} else if (c == sentinel) {
	    *str = '\0';
	    fscanf(fp, " ");
	    return 1;
	} else if (c == '\\') {
	    c = getc(fp);
	    if (c == '\n') {
		*str = ' ';
		++str;
	    } else if (c == sentinel) {
		*str = c;
		++str;
	    } else {
		*str = '\\';
		++str;
		*str = c;
		++str;
	    }
	} else {
	    *str = c;
	    ++str;
	}
    }
}



static int
fscan_string(FILE *fp, char str[])
{
    char        c;

    c = getc(fp);
    if (c == '"') {
	return fscan_upto(fp, str, '"');
    } else if (c == '\'') {
	return fscan_upto(fp, str, '\'');
    } else {
	*str = c;
	++str;
	return fscanf(fp, "%s ", str);
    }
}



int
trc_read_obj_ev_file(char *filename, trc_ev_name_p *x_names)
{
    trc_ev_name_p   names;
    int             max_ev = ARRAY_SIZE;
    int             n_ev;
    FILE           *ev_file;

    names = pan_calloc(max_ev, sizeof(trc_ev_name_t));
    n_ev = 0;
    ev_file = fopen(filename, "r");
    if (ev_file == NULL) {
	return -1;
    }
    while (!feof(ev_file)) {
	if (n_ev >= max_ev) {
	    max_ev *= 2;
	    names = pan_realloc(names, max_ev * sizeof(trc_ev_name_t));
	}
	fscan_string(ev_file, names[n_ev]);
	++n_ev;
    }
    fclose(ev_file);
    *x_names = names;
    return n_ev;
}


					/* Locate new type if already recorded,
					 * otherwise record it */
static obj_type_p
add_obj_type(trc_rebind_p rebind, int obj_type_id, char *name)
{
    obj_type_p new;

    new = rebind->obj_types;
    while (new != NULL && new->id != obj_type_id) {
	new = new->next;
    }
    if (new == NULL) {
	new = pan_malloc(sizeof(obj_type_t));
	new->id = obj_type_id;
	if (name == NULL) {
	    new->name[0] = '\0';
	} else {
	    strcpy(new->name, name);
	}
	new->max_oper = ARRAY_SIZE;	/* create operation slots */
	new->n_oper = 0;
	new->oper = pan_calloc(rebind->max_object, sizeof(oper_t));

	new->next = rebind->obj_types;	/* prepend to the list */
	rebind->obj_types = new;
	++rebind->n_obj_types;
    } else if (new->name[0] == '\0' && name != NULL) {
	strcpy(new->name, name);
    } else {
	new->id = obj_type_id;
    }

    return new;
}


trc_rebind_p
trc_init_rebind(void)
{
    trc_rebind_p    rb;

    rb = pan_malloc(sizeof(trc_rebind_t));
    rb->max_object = ARRAY_SIZE;
    rb->max_obj_ev = ARRAY_SIZE;
    rb->n_object = 0;
    rb->n_obj_ev = 0;
    rb->object = pan_calloc(rb->max_object, sizeof(object_t));
    rb->obj_ev = pan_calloc(rb->max_obj_ev, sizeof(ev_pair_t));

    rb->obj_types = NULL;
    rb->n_obj_types = 0;

    return rb;
}



void
trc_clear_rebind(trc_rebind_p rb)
{
    obj_type_p hold;

    rb->max_object = 0;
    rb->max_obj_ev = 0;
    rb->n_object = 0;
    rb->n_obj_ev = 0;

    if (rb->object != NULL)
	pan_free(rb->object);
    if (rb->obj_ev != NULL)
	pan_free(rb->obj_ev);
    
    rb->n_obj_types = 0;
    while (rb->obj_types != NULL) {
	hold = rb->obj_types;
	rb->obj_types = hold->next;
	if (hold->oper != NULL) {
	    pan_free(hold->oper);
	}
	pan_free(hold);
    }

    if (rb->obj_cr_fmt != NULL)
	pan_free(rb->obj_cr_fmt);
    if (rb->map_op_fmt != NULL)
	pan_free(rb->map_op_fmt);
    if (rb->obj_tp_fmt != NULL)
	pan_free(rb->obj_tp_fmt);
}



static object_p
locate_object(trc_rebind_p rebind, trc_object_id_p id)
{
    int         i;

    for (i = 0; i < rebind->n_object; i++) {
	if (rebind->object[i].id.cpu == id->cpu &&
		rebind->object[i].id.addr == id->addr) {
	    return &rebind->object[i];
	}
    }
    return NULL;
}



static oper_p
locate_oper(trc_rebind_p rebind, short int obj_type, short int op)
{
    int         i;
    obj_type_p  obj_tp;

    obj_tp = rebind->obj_types;
    while (obj_tp != NULL && obj_tp->id != obj_type) {
	obj_tp = obj_tp->next;
    }
    if (obj_tp == NULL) {
	return NULL;
    }

    for (i = 0; i < obj_tp->n_oper; i++) {
	if (obj_tp->oper[i].op == op) {
	    return &obj_tp->oper[i];
	}
    }

    return NULL;
}



static boolean
xlate_fmt(char fmt[], char new_fmt[], size_t *usr_size)
{
    int         i;
    int         j;
    int         n;
    ev_fields_t fields;

    n = strlen(fmt) + 1;
    fields = EV_OPER;
    j = 0;
    for (i = 0; i < n; i++) {
	if (fields == EV_PAST_FIELDS || fmt[i] != '%') {
	    new_fmt[j] = fmt[i];
	    ++j;
	} else if (fmt[i + 1] == '%') {
	    /* Encoded '%' */
	    new_fmt[j] = fmt[i];
	    ++j;
	    ++i;
	    new_fmt[j] = fmt[i];
	    ++j;
	} else {
	    if (fields == EV_OPER) {
		/* Short to designate operation */
		++i;
		if (fmt[i] != 'h') {
		    fprintf(stderr, "%s: field %d != 'h'\n",
			    "format translation error", EV_OPER);
		    return FALSE;
		}
		fields = EV_OBJ_CPU;
		new_fmt[j] = '%';
		++j;
		sprintf(&new_fmt[j], "%d", TRC_NAME_LENGTH);
		j = strchr(new_fmt, '\0') - &new_fmt[0];
		new_fmt[j] = 's';
		++j;
	    } else if (fields == EV_OBJ_CPU) {
		/* Second short to designate object cpu */
		++i;
		if (fmt[i] != 'h') {
		    fprintf(stderr, "%s: field %d != 'h'\n",
			    "format translation error", fields);
		    return FALSE;
		}
		/* Skip chars between %h fields */
		++i;
		while (i < n && fmt[i] != '%') {
		    ++i;
		    if (fmt[i] == '%' && fmt[i + 1] == '%') {
			i += 2;	/* Encoded '%' */
		    }
		}
		if (i == n) {
		    fprintf(stderr, "%s: field %d lacks\n",
			    "format translation error", fields);
		    return FALSE;
		}
		fields = EV_OBJ_ADDR;
		++i;		/* Designate object address */
		if (fmt[i] != 'p') {
		    fprintf(stderr, "%s: field %d != 'p'\n",
			    "format translation error", fields);
		    return FALSE;
		}
		fields = EV_PAST_FIELDS;
		new_fmt[j] = '%';
		++j;
		sprintf(&new_fmt[j], "%d", TRC_NAME_LENGTH);
		j = strchr(new_fmt, '\0') - &new_fmt[0];
		new_fmt[j] = 's';
		++j;
	    } else {
		fprintf(stderr, "%s: field > %d\n",
			"format translation error", EV_OBJ_ADDR);
		return FALSE;
	    }
	}
    }
    *usr_size += 2 * TRC_NAME_LENGTH - 2 * sizeof(short int) - sizeof(void *);
    return TRUE;
}



void
trc_mk_obj_ev(trc_event_lst_p ev_lst, trc_rebind_p rb,
	      trc_ev_name_p obj_ev_name, int n_ev, boolean unbind_original)
{
    int         i;
    ev_pair_p   ev;
    size_t      usr_size;
    int         level;
    trc_event_t e_ex;
    char       *name;
    char       *fmt;
    char       *new_fmt;
    size_t      n_new_fmt;
    char       *obj_cr_name;
    char       *map_op_name;
    char       *map_obj_type_name;

    obj_cr_name = "create object";
    rb->obj_cr = trc_event_lst_find(ev_lst, obj_cr_name);
    if (rb->obj_cr == -1) {	/* Not generated by Orca program */
	return;
    }
    rb->obj_cr_fmt = strdup(trc_event_lst_fmt(ev_lst, rb->obj_cr));

    map_op_name = "operation mapping";
    rb->map_op = trc_event_lst_find(ev_lst, map_op_name);
    rb->map_op_fmt = strdup(trc_event_lst_fmt(ev_lst, rb->map_op));

    map_obj_type_name = "object type mapping";
    rb->obj_tp = trc_event_lst_find(ev_lst, map_obj_type_name);
    rb->obj_tp_fmt = strdup(trc_event_lst_fmt(ev_lst, rb->obj_tp));

    n_new_fmt = TRC_NAME_LENGTH;
    new_fmt = pan_malloc(n_new_fmt);

    for (i = 0; i < n_ev; i++) {
	resize_array(rb->n_obj_ev, &rb->max_obj_ev,
		     (void **)&rb->obj_ev, sizeof(ev_pair_t));
	ev = &rb->obj_ev[rb->n_obj_ev];
	ev->old = trc_event_lst_find(ev_lst, obj_ev_name[i]);
	if (ev->old == TRC_NO_SUCH_EVENT) {
	    fprintf(stderr, "No such event: \"%s\"\n", obj_ev_name[i]);
	} else {
	    trc_event_lst_query(ev_lst, ev->old, &usr_size, &level);
	    trc_event_lst_query_extern(ev_lst, ev->old, &e_ex, &name, &fmt);
	    if (strlen(fmt) > n_new_fmt - 2) {	/* Add 1 char + '\0' */
		n_new_fmt = strlen(fmt) + 2;
		new_fmt = pan_realloc(new_fmt, n_new_fmt);
	    }
	    if (xlate_fmt(fmt, new_fmt, &usr_size)) {
		ev->new = trc_event_lst_add(ev_lst, level, usr_size, name,
					    new_fmt);
		++rb->n_obj_ev;
	    } else {
		fprintf(stderr, "object \"%s\" ignored\n", obj_ev_name[i]);
	    }
	    if (unbind_original) {
		trc_event_lst_unbind(ev_lst, ev->old);
	    }
	}
    }
    pan_free(new_fmt);
}



static void
redef_obj_cr(trc_rebind_p rebind, trc_event_descr_p e)
{
    object_p    obj;
    char       *fmt = rebind->obj_cr_fmt;

    resize_array(rebind->n_object, &rebind->max_object,
		    (void **)&rebind->object, sizeof(object_t));

    obj = &rebind->object[rebind->n_object];

    trc_get_one_tok(e->usr, fmt, &obj->type, CR_OBJ_TYPE);
    trc_get_one_tok(e->usr, fmt, &obj->id.cpu, CR_OBJ_CPU);
    trc_get_one_tok(e->usr, fmt, &obj->id.addr, CR_OBJ_ADDR);

    /* If object already in use, return */
    if (locate_object(rebind, &obj->id) != NULL) {
	return;
    }

    trc_get_one_tok(e->usr, fmt, &obj->name, CR_NAME);
    ++rebind->n_object;
}



static void
redef_map_op(trc_rebind_p rebind, trc_event_descr_p e)
{
    oper_p      oper;
    char       *fmt = rebind->map_op_fmt;
    short int   obj_type_id;
    short int   op;
    obj_type_p  obj_type;

    trc_get_one_tok(e->usr, fmt, &obj_type_id, MAP_OBJ_TYPE);
					/* Locate if already recorded,
					 * otherwise record with empty name */
    obj_type = add_obj_type(rebind, obj_type_id, NULL);
    trc_get_one_tok(e->usr, fmt, &op, MAP_OPER);
    if (locate_oper(rebind, obj_type_id, op) != NULL) {
					/* Don't enter doubles */
	return;
    }

    resize_array(obj_type->n_oper, &obj_type->max_oper,
		 (void **)&obj_type->oper, sizeof(oper_t));
    oper = &obj_type->oper[obj_type->n_oper];
    oper->op = op;
    trc_get_one_tok(e->usr, fmt, &oper->name, MAP_OP_NAME);
    ++obj_type->n_oper;
}


void
trc_redef_obj_op(trc_rebind_p rebind, trc_event_descr_p e)
{
    trc_ev_name_t name;
    int           obj_type_id;
    char         *fmt;

    if (e->type == rebind->obj_cr) {
	redef_obj_cr(rebind, e);
    } else if (e->type == rebind->map_op) {
	redef_map_op(rebind, e);
    } else if (e->type == rebind->obj_tp) {
	fmt = rebind->obj_tp_fmt;
	trc_get_one_tok(e->usr, fmt, &obj_type_id, MAP_TYPE_ID);
	trc_get_one_tok(e->usr, fmt, name, MAP_TYPE_NAME);
	add_obj_type(rebind, obj_type_id, name);
    }
}


void
trc_obj_op_get(trc_event_lst_p ev_lst, trc_rebind_p rebind, trc_event_descr_p e,
	       short int *obj_type, short int *op, trc_object_id_p obj_id)
{
    char       *fmt;
    object_p    obj;

    fmt = trc_event_lst_fmt(ev_lst, e->type);

					/* Get operation shorts */
    trc_get_one_tok(e->usr, fmt, op, EV_OPER);
    trc_get_one_tok(e->usr, fmt, &obj_id->cpu, EV_OBJ_CPU);
    trc_get_one_tok(e->usr, fmt, &obj_id->addr, EV_OBJ_ADDR);

					/* Get object type */
    obj = locate_object(rebind, obj_id);
    *obj_type = obj->type;
}


static boolean
do_rebind_op(trc_event_lst_p event_lst, trc_event_descr_p e, trc_event_t e_new,
	     trc_rebind_p rebind)
{
    char       *new_usr;
    size_t      usr_size;
    short int   obj_type;
    short int   op;
    oper_p      oper;
    trc_object_id_t obj_id;
    char       *old_rest;
    char       *fmt;
    object_p    obj;

    fmt = trc_event_lst_fmt(event_lst, e->type);
    usr_size = trc_event_lst_usr_size(event_lst, e_new);
    new_usr = pan_malloc(usr_size);

					/* Get object type */
    trc_get_one_tok(e->usr, fmt, &obj_id.cpu, EV_OBJ_CPU);
    trc_get_one_tok(e->usr, fmt, &obj_id.addr, EV_OBJ_ADDR);
    obj = locate_object(rebind, &obj_id);
    obj_type = obj->type;
    if (obj == NULL) {
	return FALSE;
    }
					/* Get operation */
    trc_get_one_tok(e->usr, fmt, &op, EV_OPER);

					/* Replace operation name */
    oper = locate_oper(rebind, obj_type, op);
    if (oper == NULL) {
	return FALSE;
    }
    strcpy(new_usr, oper->name);

					/* Replace object name */
    strcpy(&new_usr[TRC_NAME_LENGTH], obj->name);
    old_rest = e->usr;
    old_rest += sizeof(short int);
    old_rest = ptr_align(old_rest, sizeof(short int));
    old_rest += sizeof(short int);
    old_rest = ptr_align(old_rest, sizeof(short int));
    old_rest += sizeof(void *);
    old_rest = ptr_align(old_rest, sizeof(void *));
    memcpy(&new_usr[2 * TRC_NAME_LENGTH], old_rest,
	   e->usr_size - (old_rest - e->usr));
    pan_free(e->usr);
    e->usr = new_usr;
    e->usr_size = usr_size;
    e->type = e_new;

    return TRUE;
}



boolean
trc_rebind_op(trc_event_lst_p event_lst, trc_rebind_p rebind,
	      trc_event_descr_p e)
{
    int         i;

    for (i = 0; i < rebind->n_obj_ev; i++) {
	if (e->type == rebind->obj_ev[i].old) {
	    return do_rebind_op(event_lst, e, rebind->obj_ev[i].new, rebind);
	}
    }

    return FALSE;
}



trc_event_t
trc_is_rebind_op(trc_event_lst_p event_lst, trc_rebind_p rebind, trc_event_t e)
{
    int         i;

    for (i = 0; i < rebind->n_obj_ev; i++) {
	if (e == rebind->obj_ev[i].old) {
	    return rebind->obj_ev[i].new;
	}
    }

    return TRC_NO_SUCH_EVENT;
}



trc_event_t
trc_is_rebound_op(trc_event_lst_p event_lst, trc_rebind_p rebind, trc_event_t e)
{
    int         i;

    for (i = 0; i < rebind->n_obj_ev; i++) {
	if (e == rebind->obj_ev[i].new) {
	    return rebind->obj_ev[i].old;
	}
    }

    return TRC_NO_SUCH_EVENT;
}


/*---- Utility functions to access the rebind data struct --------------------*/

int
trc_rb_n_obj_types(trc_rebind_p rb)
{
    return rb->n_obj_types;
}


int
trc_rb_first_obj_type(trc_rebind_p rb)
{
    if (rb->n_obj_types == 0) {
	return TRC_NO_OBJ_TYPE;
    }
    return rb->obj_types->id;
}


int
trc_rb_next_obj_type(trc_rebind_p rb, int id)
{
    obj_type_p p;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL || p->next == NULL) {
	return TRC_NO_OBJ_TYPE;
    }
    return p->next->id;
}


char *
trc_rb_obj_type_name(trc_rebind_p rb, int id)
{
    obj_type_p p;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL) {
	return NULL;
    }
    return p->name;
}


int
trc_rb_n_ops(trc_rebind_p rb, int id)
{
    obj_type_p p;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL) {
	return 0;
    }
    return p->n_oper;
}


int
trc_rb_first_op(trc_rebind_p rb, int id)
{
    obj_type_p p;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL || p->n_oper == 0) {
	return TRC_NO_OBJ_TYPE;
    }
    return p->oper[0].op;
}


int
trc_rb_next_op(trc_rebind_p rb, int id, int op)
{
    obj_type_p p;
    int        i;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL) {
	return TRC_NO_OPERATION;
    }

    for (i = 0; i < p->n_oper && op != p->oper[i].op; i++);

    i++;

    if (i == p->n_oper) {
	return TRC_NO_OPERATION;
    }

    return p->oper[i].op;
}

char *
trc_rb_op_name(trc_rebind_p rb, int id, int op)
{
    obj_type_p p;
    int        i;

    p = rb->obj_types;
    while (p != NULL && p->id != id) {
	p = p->next;
    }
    if (p == NULL) {
	return NULL;
    }

    for (i = 0; i < p->n_oper && op != p->oper[i].op; i++);
    if (i == p->n_oper) {
	return NULL;
    }

    return p->oper[i].name;
}


trc_object_id_p
trc_rb_first_obj(trc_rebind_p rb, int tp)
{
    int         i;

    for (i = 0; i < rb->n_object && tp != rb->object[i].type; i++);
    if (i == rb->n_object) {
	return NULL;
    }
    return &rb->object[i].id;
}


trc_object_id_p
trc_rb_next_obj(trc_rebind_p rb, int tp, trc_object_id_p id)
{
    int         i;

    for (i = 0; i < rb->n_object && id != &rb->object[i].id; i++);

    for (i++; i < rb->n_object && tp != rb->object[i].type; i++);

    if (i >= rb->n_object) {
	return NULL;
    }
    return &rb->object[i].id;
}


char *
trc_rb_obj_name(trc_rebind_p rb, trc_object_id_p id)
{
    object_p obj;

    obj = locate_object(rb, id);
    return obj->name;
}


boolean
trc_object_id_eq(trc_object_id_p id1, trc_object_id_p id2)
{
    return (id1->cpu == id2->cpu && id1->addr == id2->addr);
}


/*---- Merge a rebind struct into an accumulating rebind struct --------------*/


static obj_type_p
locate_obj_type(trc_rebind_p rb, obj_type_p tp)
{
    int i;

    for (i = 0; i < rb->n_obj_types; i++) {
	if (rb->obj_types[i].id == tp->id) {
	    return &rb->obj_types[i];
	}
    }

    return NULL;
}


static ev_pair_p
locate_obj_ev(trc_rebind_p rb, ev_pair_p obj_ev)
{
    int i;

    for (i = 0; i < rb->n_obj_ev; i++) {
	if (rb->obj_ev[i].old == obj_ev->old &&
	    rb->obj_ev[i].new == obj_ev->new) {
	    return &rb->obj_ev[i];
	}
    }

    return NULL;
}



static obj_type_p
copy_obj_type(obj_type_p scan)
{
    obj_type_p new;
    int        i;

    new = pan_malloc(sizeof(obj_type_t));
    *new = *scan;
    strcpy(new->name, scan->name);

    new->oper = pan_calloc(scan->n_oper, sizeof(oper_t));
    for (i = 0; i < scan->n_oper; i++) {
	new->oper[i] = scan->oper[i];
	strcpy(new->oper[i].name, scan->oper[i].name);
    }

    return new;
}


static void
copy_object(object_p to, object_p from)
{
    *to = *from;
    strcpy(to->name, from->name);
}


void
trc_bind_merge(trc_rebind_p from, trc_rebind_p acc)
{
    int i;
    obj_type_p scan;
    obj_type_p new;

    if (from == NULL) {
	return;
    }

    scan = from->obj_types;
    while (scan != NULL) {
	if (locate_obj_type(acc, scan) == NULL) {
	    new = copy_obj_type(scan);
	    new->next = acc->obj_types;
	    acc->obj_types = new;
	    ++acc->n_obj_types;
	}
	scan = scan->next;
    }

    acc->max_object = acc->n_object + from->n_object;
    acc->object = pan_realloc(acc->object, acc->max_object * sizeof(object_t));
    for (i = 0; i < from->n_object; i++) {
	if (locate_object(acc, &from->object[i].id) == NULL) {
	    copy_object(&acc->object[acc->n_object], &from->object[i]);
	    ++acc->n_object;
	}
    }

    acc->max_obj_ev = acc->n_obj_ev + from->n_object;
    acc->obj_ev = pan_realloc(acc->obj_ev, acc->max_obj_ev * sizeof(ev_pair_t));
    for (i = 0; i < from->n_obj_ev; i++) {
	if (locate_obj_ev(acc, &from->obj_ev[i]) == NULL) {
	    acc->obj_ev[acc->n_obj_ev] = from->obj_ev[i];
	    ++acc->n_obj_ev;
	}
    }
}



/*---- IO functions for detected object types, operations and instantiations -*/


static void
read_oper(FILE *s, oper_p oper)
{
    int n;

    fread(&oper->op, sizeof(short int), 1, s);
    fread(&n, sizeof(int), 1, s);
    fread(oper->name, 1, n, s);
}


static void
read_obj_type(FILE *s, obj_type_p tp)
{
    int i;
    int n;

    fread(&tp->id, sizeof(int), 1, s);
    fread(&n, sizeof(int), 1, s);
    fread(tp->name, 1, n, s);
    fread(&tp->n_oper, sizeof(int), 1, s);
    tp->oper = pan_calloc(tp->n_oper, sizeof(oper_t));
    for (i = 0; i < tp->n_oper; i++) {
	read_oper(s, &tp->oper[i]);
    }
    tp->max_oper = tp->n_oper;
}


static void
read_obj(FILE *s, object_p obj)
{
    int n;

    fread(&obj->type, sizeof(short int), 1, s);
    fread(&obj->id.cpu, sizeof(short int), 1, s);
    fread(&obj->id.addr, sizeof(void *), 1, s);
    fread(&n, sizeof(int), 1, s);
    fread(obj->name, 1, n, s);
}


trc_rebind_p
trc_rebind_read(FILE *s)
{
    int i;
    obj_type_p scan;
    trc_rebind_p rb;

    rb = pan_malloc(sizeof(trc_rebind_t));

    fread(&rb->n_obj_types, sizeof(int), 1, s);

    rb->obj_types = NULL;
    for (i = 0; i < rb->n_obj_types; i++) {
	scan = pan_malloc(sizeof(obj_type_t));
	read_obj_type(s, scan);
	scan->next = rb->obj_types;
	rb->obj_types = scan;
    }

    fread(&rb->n_object, sizeof(int), 1, s);
    rb->object = pan_calloc(rb->n_object, sizeof(object_t));
    for (i = 0; i < rb->n_object; i++) {
	read_obj(s, &rb->object[i]);
    }
    rb->max_object = rb->n_object;

    rb->max_obj_ev = ARRAY_SIZE;
    rb->n_obj_ev   = 0;
    rb->obj_ev     = pan_calloc(rb->max_obj_ev, sizeof(ev_pair_t));

    return rb;
}


static void
write_oper(FILE *s, oper_p oper)
{
    int n;

    fwrite(&oper->op, sizeof(short int), 1, s);
    n = strlen(oper->name) + 1;
    fwrite(&n, sizeof(int), 1, s);
    fwrite(oper->name, 1, n, s);
}


static void
write_obj_type(FILE *s, obj_type_p tp)
{
    int i;
    int n;

    fwrite(&tp->id, sizeof(int), 1, s);
    n = strlen(tp->name) + 1;
    fwrite(&n, sizeof(int), 1, s);
    fwrite(tp->name, 1, n, s);
    fwrite(&tp->n_oper, sizeof(int), 1, s);
    for (i = 0; i < tp->n_oper; i++) {
	write_oper(s, &tp->oper[i]);
    }
}


static void
write_obj(FILE *s, object_p obj)
{
    int n;

    fwrite(&obj->type, sizeof(short int), 1, s);
    fwrite(&obj->id.cpu, sizeof(short int), 1, s);
    fwrite(&obj->id.addr, sizeof(void *), 1, s);
    n = strlen(obj->name) + 1;
    fwrite(&n, sizeof(int), 1, s);
    fwrite(obj->name, 1, n, s);
}


void
trc_rebind_write(FILE *s, trc_rebind_p rb)
{
    int i;
    obj_type_p scan;

    fwrite(&rb->n_obj_types, sizeof(int), 1, s);
    scan = rb->obj_types;
    for (i = 0; i < rb->n_obj_types; i++) {
	write_obj_type(s, scan);
	scan = scan->next;
    }
    if (scan != NULL) {
	fprintf(stderr, "Inconsistency in trc_rebind_write: next != NULL\n");
    }

    fwrite(&rb->n_object, sizeof(int), 1, s);
    for (i = 0; i < rb->n_object; i++) {
	write_obj(s, &rb->object[i]);
    }
}


static void
skip_oper(FILE *s)
{
    int n;

    fseek(s, sizeof(short int), 1);
    fread(&n, sizeof(int), 1, s);
    fseek(s, n, 1);
}


static void
skip_obj_type(FILE *s)
{
    int i;
    int n;

    fseek(s, sizeof(int), 1);
    fread(&n, sizeof(int), 1, s);
    fseek(s, n, 1);
    fread(&n, sizeof(int), 1, s);
    for (i = 0; i < n; i++) {
	skip_oper(s);
    }
}


static void
skip_obj(FILE *s)
{
    int n;

    fseek(s, sizeof(short int), 1);
    fseek(s, sizeof(short int), 1);
    fseek(s, sizeof(void *), 1);
    fread(&n, sizeof(int), 1, s);
    fseek(s, n, 1);
}


void
trc_rebind_skip(FILE *s)
{
    int i;
    int n;

    fread(&n, sizeof(int), 1, s);

    for (i = 0; i < n; i++) {
	skip_obj_type(s);
    }

    fread(&n, sizeof(int), 1, s);
    for (i = 0; i < n; i++) {
	skip_obj(s);
    }
}


void
trc_rebind_print(FILE *s, trc_rebind_p rb)
{
    obj_type_p scan;
    int        i;
    object_p   obj;

    fprintf(s, "======== Object data collected ======================\n");

    if (rb == NULL) {
	return;
    }

    fprintf(s, "%d object types\n", rb->n_obj_types);
    scan = rb->obj_types;
    while (scan != NULL) {
	fprintf(s, "object type %d %s\n", scan->id, scan->name);
	fprintf(s, "\t%d operations\n", scan->n_oper);
	for (i = 0; i < scan->n_oper; i++) {
	    fprintf(s, "\toperation %d %s\n",
			scan->oper[i].op, scan->oper[i].name);
	}
	scan = scan->next;
    }

    fprintf(s, "%d objects\n", rb->n_object);
    for (i = 0; i < rb->n_object; i++) {
	obj = &rb->object[i];
	fprintf(s, "\tobject type %d id <%d,%p> name %s\n",
		obj->type, obj->id.cpu, obj->id.addr, obj->name);
    }
}
