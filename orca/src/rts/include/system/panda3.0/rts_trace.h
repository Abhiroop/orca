/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_TRACE_H__
#define __RTS_TRACE_H__

#include "pan_sys.h"
#include "pan_trace.h"
#include "stddef.h"

#include "orca_types.h"

#define LEVEL 4998

#define oper_id( type, index)	(td_registration(type)<<16 | (index))

extern trc_event_t proc_create, proc_exit, obj_create, obj_destroy,
		   op_descr, op_inv, op_rpc, op_mcast, op_block, op_cont,
		   wait_seq, bcast_block, bcast_cont, rpc_block, rpc_cont,
		   man_decision, man_change, objtype_descr, cont_create,
		   cont_done, pure_write;

extern void rts_trc_start(int trace_level);
extern void *str2traceinfo( char *str);
extern void *map_info( int n, char *s);

#endif
