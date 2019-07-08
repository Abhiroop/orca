/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_TRACE_FILTER_H__
#define __PANDA_TRACE_FILTER_H__

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


#include "pan_trace.h"

#include "trc_types.h"


void    trc_filter_start(trc_p trc);

void    trc_filter_end(void);

void    trc_create_filter(int argc, char *argv[]);

boolean trc_filter_thread_logged_events(trc_thread_id_p e_p, trc_rebind_p rb);

boolean trc_filter_event_match(trc_event_descr_p e, trc_rebind_p rb);


#endif
