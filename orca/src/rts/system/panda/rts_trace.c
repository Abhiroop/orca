#include <string.h>
#include "rts_trace.h"


trc_event_t proc_create, proc_exit, obj_create, obj_destroy, op_descr,
	    op_inv, op_rpc, op_mcast, op_block, op_cont, wait_seq,
	    bcast_block, bcast_cont, rpc_block, rpc_cont, man_decision,
	    man_change, objtype_descr, cont_create, cont_done;

typedef struct {
    int num;
    char str[80];
} mapping;

void
rts_trc_start(int trace_level)
{
    trc_set_level(trace_level);
    
    proc_create = trc_new_event(LEVEL,(size_t) 80,"start process","%80s");
    proc_exit = trc_new_event(LEVEL,(size_t) 80,"exit process","%80s");
    
    obj_create = trc_new_event( LEVEL, sizeof(mapping),
			       "create object","type = %h, id = %h.%p, name = %80s");
    obj_destroy = trc_new_event( LEVEL, sizeof(struct {int cpu; void *rts;}),
				"destroy object", "id = %d.%p");
    
    objtype_descr = trc_new_event( LEVEL, sizeof(mapping),
			     "object type mapping", "type = %d, name = %80s");

    op_descr = trc_new_event( LEVEL, sizeof(mapping),
			     "operation mapping", "op = %h.%h, name = %80s");
    op_inv = trc_new_event( LEVEL, sizeof(op_inv_info),
			   "do operation", "op = %h, obj = %h.%p");
    op_rpc = trc_new_event( LEVEL, sizeof(op_inv_info),
			   "handle rpc op", "op = %h, obj = %h.%p");
    op_mcast = trc_new_event( LEVEL, sizeof(op_inv_info),
			     "handle mcast op", "op = %h, obj = %h.%p");
    op_block = trc_new_event( LEVEL-1, sizeof(op_inv_info),
			     "block on guard", "op = %h, obj = %h.%p");

    /* RAFB: changed text from "retry guard" to "guard true." 
     *
     * In some cases this text is wrong; e.g., when an object
     * has is migrated and we need to wake up pending RPCs.
     */
    op_cont = trc_new_event( LEVEL-1, sizeof(op_cont_info),
			    "guard true", "op = %h, obj = %h.%p");

    cont_create = trc_new_event( LEVEL-1, sizeof(op_cont_info),
				"create continuation",
				"op = %h, obj = %h.%p, state = %p");

    cont_done   = trc_new_event( LEVEL-1, sizeof(op_cont_info),
				"continuation done",
				"op = %h, obj = %h.%p, state = %p");


/*
    rpc_block = trc_new_event( LEVEL-1, sizeof(op_inv_info),
			      "issue rpc; block","op = %h,obj = %h.%p");
*/
    rpc_block = trc_new_event(6500, sizeof(op_inv_info),
			      "issue rpc; block","op = %h,obj = %h.%p");
/*
    rpc_cont = trc_new_event( LEVEL-1, sizeof(op_inv_info),
			     "rpc reply; continue","op = %h,obj = %h.%p");
*/
    rpc_cont = trc_new_event(6500, sizeof(op_inv_info),
			     "rpc reply; continue","op = %h,obj = %h.%p");

    bcast_block = trc_new_event( LEVEL-1, sizeof(op_inv_info),
				"send bcast; block","op = %h,obj = %h.%p");
    bcast_cont = trc_new_event( LEVEL-1, sizeof(op_inv_info),
			       "got bcast; continue","op = %h,obj = %h.%p");
    
    wait_seq = trc_new_event( LEVEL-2, 2*sizeof(int),
			     "wait for seqno", "tag = %d, seq = %d");
    
    man_decision = trc_new_event( LEVEL+1, 80, "manager decision", "%s");

    man_change = trc_new_event( LEVEL+1, 80, "manager action", "%s");
}


void *
str2traceinfo(char *str)
{
	/* ugly! truncate to 80 characters */
	if ( strlen(str) >= 80)
		str[79] = '\0';
	return( (void *) str);
}


void *
map_info(int n, char *s)
{
	static mapping tmp;

	tmp.num = n;
	strncpy(tmp.str, s, 80);
	tmp.str[79] = '\0';
	return ((void *)&tmp);
}

