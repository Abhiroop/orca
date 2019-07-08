/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* Implementation of a simple record structure that stores information about
 * blocked operations. Sometimes threads explicitly block on these records
 * (i_block() call).
 */

#include <assert.h>
#include "fragment.h"
#include "invocation.h"
#include "msg_marshall.h"
#include "proxy.h"

void
i_init(invocation_p i, fragment_p frag, f_status_t status,
       tp_dscr *obj_type, int opindex, pan_upcall_p upcall,
       void **argv, int *op_flags)
{
    /* Initialise the record.
     */
    i->resumed = pan_cond_create(frag->fr_lock);
    i->frag     = frag;
    i->f_status = status;
    i->opindex  = opindex;
    i->upcall   = upcall;
    i->argv     = argv;
    i->op_flags = op_flags;
    i->result   = I_BLOCKED;
}

/* i_get_info: extract fields from invocation record.
 */
void 
i_get_info(invocation_p i, int *opindex, tp_dscr **objtype,
		       fragment_p *frag, void ***argv, f_status_p status,
		       pan_upcall_p *upcall, int **op_flags)
{
    *opindex = i->opindex;
    *frag    = i->frag;
    *objtype = (*frag)->fr_type;
    *argv    = i->argv;
    *status  = i->f_status;
    *upcall  = i->upcall;
    *op_flags  = i->op_flags;
}

i_status_t 
i_block(invocation_p i)
{
    while (i->result == I_BLOCKED) {
	pan_cond_wait(i->resumed);   /* block */
    }
    return i->result;
}

/* i_wakeup: write status into the invocation record and wake up
 * any blocked threads.
 */
void
i_wakeup(invocation_p i, i_status_t status)
{
    i->result = status;
    pan_cond_broadcast(i->resumed);
}
