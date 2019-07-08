/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Function prototypes for data structure with event types.
 *
 * Problem: for deposit of each event, the event type-specific data size
 *          is looked up in the event type data structure. Concurrently,
 *          new event types can be added. However, locking (for reads)of the
 *          event type data structure is undesirable.
 * Solution: The event data structure is reentrant for multiple readers and
 *          one writer _IF_ writes are atomic. Locks are applied for writing
 *          but not for reading.
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */

#ifndef _TRACE_TRC_EVENT_LST_H
#define _TRACE_TRC_EVENT_LST_H


#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"


#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"


extern const trc_event_t TRC_NO_SUCH_EVENT;



/* Initialise an external event type struct */
void        trc_event_descr_init(trc_event_descr_p e);

/* Clear an external event type struct */
void        trc_event_descr_clear(trc_event_descr_p e);

/* Copy an external event type struct */
void        trc_event_descr_cpy(trc_event_descr_p from, trc_event_descr_p to);

/* Test event type structs for contents equality */
boolean     trc_event_descr_equal(trc_event_descr_p e1, trc_event_descr_p e2);


/* Init event type data structure */
trc_event_lst_p trc_event_lst_init(void);

/* A new event type. This call is complete in itself */
trc_event_t trc_event_lst_add(trc_event_lst_p block, int level, size_t size,
			      char *name, char *fmt);

/* For recreating a previously existing event type data structure.
 * Name/event number binding is restored. Use for instance when reading
 * event type info from a file. */
void        trc_event_lst_bind(trc_event_lst_p block, trc_event_t num,
			       trc_event_t e_extern, int level, size_t size,
			       char *name, char *fmt);

/* Delete an event type. */
void        trc_event_lst_unbind(trc_event_lst_p block, trc_event_t num);


/* Clear event type data structure */
void        trc_event_lst_clear(trc_event_lst_p event_list);


/* Get event type info, enough to read/write/copy an event. */
void        trc_event_lst_query(trc_event_lst_p lst, trc_event_t e,
				size_t *size, int *level);

/* Get complete event type info, also extern and print info. */
void        trc_event_lst_query_extern(trc_event_lst_p lst, trc_event_t e,
				       trc_event_p e_extern, char **name,
				       char **fmt);


/* Locate event type from symbolic name */
trc_event_t trc_event_lst_find(trc_event_lst_p block, char *name);


/* (Re)set extern event id */
void        trc_event_lst_bind_extern(trc_event_lst_p block, trc_event_t num,
				      trc_event_t e_extern);


/* Convenience macros for (faster?)query */


size_t      trc_event_lst_usr_size_f(trc_event_lst_p lst, trc_event_t e);

int         trc_event_lst_level_f(trc_event_lst_p lst, trc_event_t e);

trc_event_t trc_event_lst_extern_id_f(trc_event_lst_p lst, trc_event_t e);

char       *trc_event_lst_name_f(trc_event_lst_p lst, trc_event_t e);

char       *trc_event_lst_fmt_f(trc_event_lst_p lst, trc_event_t e);


#define trc_event_lst_usr_size(lst, e) \
		((e < TRC_EVENT_BLOCK) ? (lst)->info[e].usr_size : \
			trc_event_lst_usr_size_f(lst, e))

#define trc_event_lst_level(lst, e) \
		((e < TRC_EVENT_BLOCK) ? (lst)->info[e].level : \
			trc_event_lst_level_f(lst, e))

#define trc_event_lst_extern_id(lst, e) \
		((e < TRC_EVENT_BLOCK) ? (lst)->info[e].extern_id : \
			trc_event_lst_extern_id_f(lst, e))

#define trc_event_lst_name(lst, e) \
		((e < TRC_EVENT_BLOCK) ? (lst)->info[e].name : \
			trc_event_lst_name_f(lst, e))

#define trc_event_lst_fmt(lst, e) \
		((e < TRC_EVENT_BLOCK) ? (lst)->info[e].fmt : \
			trc_event_lst_fmt_f(lst, e))



/* Traverse event types */

int         trc_event_lst_num(trc_event_lst_p block);

trc_event_t trc_event_lst_first(trc_event_lst_p block);

trc_event_t trc_event_lst_next(trc_event_lst_p block, trc_event_t e);


/* Set of event types */


int		trc_event_tp_set_length(int max_event);

trc_event_tp_set_p trc_event_tp_set_create(int max_event);

void		trc_event_tp_set_clear(trc_event_tp_set_p set);

boolean		trc_is_set(trc_event_tp_set_p set, int bit);

void		trc_set(trc_event_tp_set_p set, int bit);

void		trc_unset(trc_event_tp_set_p set, int bit);

#endif
