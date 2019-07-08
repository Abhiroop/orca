#ifndef __RTS_TRACE_H__
#define __RTS_TRACE_H__

#include "trace/trace.h"
#include "stddef.h"

#include "orca_types.h"

#define LEVEL 4998

extern trc_event_t proc_create, proc_exit, obj_create, obj_destroy,
		   op_descr, op_inv, op_rpc, op_mcast, op_block, op_cont,
		   wait_seq, bcast_block, bcast_cont, rpc_block, rpc_cont,
		   man_decision, man_change, objtype_descr, cont_create,
		   cont_done;

extern void rts_trc_start(int trace_level);
extern void *str2traceinfo( char *str);
extern void *map_info( int n, char *s);

#endif
