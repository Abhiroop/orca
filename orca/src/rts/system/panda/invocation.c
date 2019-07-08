/* Implementation of a simple record structure that stores information about
 * blocked operations. Sometimes threads explicitly block on these records
 * (i_block() call).
 */

#include <assert.h>
#include "invocation.h"
#include "msg_marshall.h"
#include "policy.h"
#include "proxy.h"


void
i_init(invocation_p i, fragment_p frag, f_status_t status,
       tp_dscr *obj_type, int opindex, pan_upcall_t upcall,
       void **argv)
{
    /* Initialise the record.
     */
    sys_cond_init(&i->resumed);
    i->frag     = frag;
    i->f_status = status;
    i->obj_type = obj_type;
    i->opindex  = opindex;
    i->upcall   = upcall;
    i->argv     = argv;
    i->result   = I_BLOCKED;
}


/* i_get_info: extract fields from invocation record.
 */
void 
i_get_info(invocation_p i, int *opindex, tp_dscr **objtype,
		       fragment_p *frag, void ***argv, f_status_p status,
		       pan_upcall_p upcall)
{
    *opindex = i->opindex;
    *objtype = i->obj_type;
    *frag    = i->frag;
    *argv    = i->argv;
    *status  = i->f_status;
    *upcall  = i->upcall;
}


i_status_t 
i_block(invocation_p i, mutex_p lock)
{
    while (i->result == I_BLOCKED) {
	sys_cond_wait(&i->resumed, lock);   /* block */
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
    sys_cond_broadcast(&i->resumed);
}

