#include "panda/panda.h"
#include "obj_tab.h"
#include "rts_util.h"


void 
ru_push_oid( message_p msg, oid_p id)
{
    oid_marshall( id, (char *)sys_message_push( msg, sizeof(oid_t), 1));
}


void 
ru_pop_oid( message_p msg, oid_p id)
{
    oid_unmarshall( id, (char *)sys_message_pop( msg, sizeof(oid_t), 1));
}
