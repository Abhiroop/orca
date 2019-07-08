#ifndef __RTS_UTIL_H__
#define __RTS_UTIL_H__

#include "panda/panda.h"
#include "orca_types.h"


extern void ru_push_oid( message_p msg, oid_p id);

extern void ru_pop_oid( message_p msg, oid_p id);

#endif
