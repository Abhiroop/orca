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
 * IMPORTANT!
 *   The event-specific data format of the "some event"s MUST be in this form:
 *        -- the FIRST and SECOND fields MUST be short ints that designate
 *	     an operation;
 *	  -- the THIRD field MUST be a pointer that designates an object.
 *   The FIRST and THIRD parameter of "create object" and the FIRST, SECOND
 *   and THIRD parameter of "operation mapping" MUST be as indicated above.
 *
 * IMPORTANT!
 *   The event-specific data format of the "some event"s MUST be in this form:
 *        -- the FIRST be a short int that designates an operation;
 *	  -- the SECOND field MUST be a short int that designates the cpu
 *           that created this object;
 *	  -- the THIRD field MUST be a pointer that designates the address op
 *	     the object at the cpu that created it.
 *   The FIRST, SECOND and THIRD parameter of "create object" and the FIRST,
 *   SECOND THIRD parameter of "operation mapping" MUST be as indicated above.
 *
 * ------------------
 * version 1.1 rules:
 * ------------------
 *
 *    Given mappings:
 *		"create object"		"...%h...%h...%s..."
 *		"operation mapping"	"...%h.%h...%s..."
 *		"object type mapping"	"...%d...%s..."
 * Translate:
 *	"some event"	"....%h.%h....%p..."
 * to
 *	"some event"	"....%s....%s..."
 *	where "some event"[0] := "operation mapping"[2]
 *				 cond "operation mapping"[0] == "some event"[0]
 *				   && "operation mapping"[1] == "some event"[1]
 *            "some event"[1] := "create object"[2]
 *				 cond "create object"[0] == "some event"[2]
 * IMPORTANT!
 *   The event-specific data format of the "some event"s MUST be in this form:
 *        -- the FIRST and SECOND fields MUST be short ints that designate
 *	     an operation;
 *	  -- the THIRD field MUST be a pointer that designates an object.
 *   The FIRST and THIRD parameter of "create object" and the FIRST, SECOND
 *   and THIRD parameter of "operation mapping" MUST be as indicated above.
 *
 * ------------------
 * version 1.0 rules:
 * ------------------
 *
 *    Given mappings:
 *		"create object"		"...%p...%s..."
 *		"operation mapping"	"...%h.%h...%s..."
 * Translate:
 *	"some event"	"....%h.%h....%p..."
 * to
 *	"some event"	"....%s....%s..."
 *	where "some event"[0] := "operation mapping"[2]
 *				 cond "operation mapping"[0] == "some event"[0]
 *				   && "operation mapping"[1] == "some event"[1]
 *            "some event"[1] := "create object"[1]
 *				 cond "create object"[0] == "some event"[2]
 * IMPORTANT!
 *   The event-specific data format of the "some event"s MUST be in this form:
 *        -- the FIRST and SECOND fields MUST be short ints that designate
 *	     an operation;
 *	  -- the THIRD field MUST be a pointer that designates an object.
 *   The FIRST and SECOND parameter of "create object" and the FIRST, SECOND
 *   and THIRD parameter of "operation mapping" MUST be as indicated above.
 *
 * Parameters:
 *     trc_bind_op trc_file [-e obj_event_file] {obj_event}
 */


#ifndef __TRACE_TRC_BIND_OP_H__
#define __TRACE_TRC_BIND_OP_H__


#include <stdio.h>

#include "trc_types.h"



typedef struct TRC_OBJECT_ID_T trc_object_id_t, *trc_object_id_p;

struct TRC_OBJECT_ID_T {
    short int   cpu;
    void       *addr;
};


#define TRC_NAME_LENGTH 80

typedef char  trc_ev_name_t[TRC_NAME_LENGTH], (*trc_ev_name_p)[TRC_NAME_LENGTH];


				/* Init the rebind struct */
trc_rebind_p trc_init_rebind(void);

				/* Clear the rebind struct */
void     trc_clear_rebind(trc_rebind_p rebind);

				/* Read a file of event types to be rebound */
int	 trc_read_obj_ev_file(char *filename, trc_ev_name_p *x_names);

				/* Given a list of event names to be rebound,
				 * add these to the rebind struct. */
void	 trc_mk_obj_ev(trc_event_lst_p event_lst, trc_rebind_p rebind,
		       trc_ev_name_p   obj_ev_name,	/* event names */
		       int             n_ev,		/* # event names */
		       boolean         unbind_orig);	/* destroy old event? */

				/* "create object" and "map opertion" events
				 * are remembered. */
void	 trc_redef_obj_op(trc_rebind_p rebind, trc_event_descr_p e);

				/* rebind an event if it is of suitable type */
boolean	 trc_rebind_op(trc_event_lst_p event_lst, trc_rebind_p rebind,
		       trc_event_descr_p e);

				/* get the data of an object/op event */
void     trc_obj_op_get(trc_event_lst_p ev_lst, trc_rebind_p rebind,
			trc_event_descr_p e, short int *obj_type,
			short int *op, trc_object_id_p obj_id);

				/* is this an event that should be rebound?
				 * return the rebound event_t,
				 * else TRC_NO_SUCH_EVENT */
trc_event_t trc_is_rebind_op(trc_event_lst_p event_lst, trc_rebind_p rebind,
		      trc_event_t e);

				/* is this the rebound version of an event?
				 * return the original event_t,
				 * else TRC_NO_SUCH_EVENT */
trc_event_t trc_is_rebound_op(trc_event_lst_p event_lst, trc_rebind_p rebind,
		      trc_event_t e);

/*---- Utility functions to access the rebind data struct --------------------*/


extern const int    TRC_NO_OBJ_TYPE;	/* sentinel */
extern const int    TRC_NO_OPERATION;	/* sentinel */

int      trc_rb_n_obj_types(trc_rebind_p rb);

int      trc_rb_first_obj_type(trc_rebind_p rb);

int      trc_rb_next_obj_type(trc_rebind_p rb, int id);

int      trc_rb_n_ops(trc_rebind_p rb, int id);

int      trc_rb_first_op(trc_rebind_p rb, int id);

int      trc_rb_next_op(trc_rebind_p rb, int id, int op);

char    *trc_rb_obj_type_name(trc_rebind_p rb, int id);

char    *trc_rb_op_name(trc_rebind_p rb, int id, int op);

trc_object_id_p trc_rb_first_obj(trc_rebind_p rb, int tp);

trc_object_id_p trc_rb_next_obj(trc_rebind_p rb, int tp, trc_object_id_p id);

char    *trc_rb_obj_name(trc_rebind_p rb, trc_object_id_p id);

boolean  trc_object_id_eq(trc_object_id_p id1, trc_object_id_p id2);


/*---- Merge a rebind struct into an accumulating rebind struct --------------*/


void     trc_bind_merge(trc_rebind_p from, trc_rebind_p acc);


/*---- IO-functions for detected object types, operations and instantiations -*/


trc_rebind_p trc_rebind_read(FILE *s);

void     trc_rebind_write(FILE *s, trc_rebind_p rb);

void     trc_rebind_skip(FILE *s);

void     trc_rebind_print(FILE *s, trc_rebind_p rb);


#endif
