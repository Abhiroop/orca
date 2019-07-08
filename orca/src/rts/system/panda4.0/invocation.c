/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* Implementation of a simple record structure that stores information about
 * blocked operations.
 */

#ifndef INVOCATION_INLINE
#define INVOCATION_SRC
#endif

#include <assert.h>
#include "fragment.h"
#include "invocation.h"
#include "msg_marshall.h"
#include "proxy.h"

#ifdef INVOCATION_SRC
#undef INLINE_FUNCTIONS
#endif
#include "inline.h"

pan_cond_p rts_invocation_done;


INLINE void i_clear(invocation_p inv)
{}


INLINE void
i_init(invocation_p inv, fragment_p frag, f_status_t status,
       op_dscr *op, int upcall,
       void **argv, int *op_flags)
{
    /* Initialise the record.
     */
    inv->frag     = frag;
    inv->f_status = status;
    inv->op       = op;
    inv->upcall   = upcall;
    inv->argv     = argv;
    inv->result   = I_BLOCKED;
    inv->op_flags = op_flags;
}

/* i_get_info: extract fields from invocation record.
 */
INLINE void 
i_get_info(invocation_p inv, op_dscr **op, fragment_p *frag,
	   void ***argv, f_status_p status, int *upcall,
	   int **op_flags)
{
    *op      = inv->op;
    *frag    = inv->frag;
    *argv    = inv->argv;
    *status  = inv->f_status;
    *upcall  = inv->upcall;
    *op_flags  = inv->op_flags;
}


/* i_wakeup: write status into the invocation record and wake up
 * any blocked threads.
 */
INLINE void
i_wakeup(invocation_p inv, int res)
{
    inv->result = res;

    pan_cond_broadcast(rts_invocation_done);
}


INLINE i_status_t 
i_block(invocation_p inv)
{
    while (inv->result == I_BLOCKED) {
	pan_cond_wait(rts_invocation_done);
    }
    return inv->result;
}

#ifdef OUTLINE
void
i_start(void)
{
    rts_invocation_done = pan_cond_create(rts_global_lock);
}
 
void
i_end(void)
{
    pan_cond_clear(rts_invocation_done);
}
#endif /* OUTLINE */
 
#undef INVOCATION_SRC
