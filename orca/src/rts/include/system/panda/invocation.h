#ifndef __INVOCATION_H__
#define __INVOCATION_H__

#include "orca_types.h"

/* This module describes the interface to the invocation data type.
   Once started, an invocation may succeed, block, or fail. Blocked
   invocations can be resumed.
*/

#define i_clear(inv)  sys_cond_clear(&(inv)->resumed)

extern void i_init(invocation_p i, fragment_p f, f_status_t status,
		   tp_dscr *obj_type, int op_index, pan_upcall_t upcall, 
		   void **argv);

extern void i_get_info(invocation_p i, int *opindex, tp_dscr **objtype,
		       fragment_p *frag, void ***argv, f_status_p status,
		       pan_upcall_p upcall);

extern i_status_t i_block(invocation_p inv, mutex_p lock);

extern void i_wakeup(invocation_p inv, i_status_t result);

#endif


