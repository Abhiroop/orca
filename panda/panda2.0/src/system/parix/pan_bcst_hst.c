#include <assert.h>
#include <limits.h>

#include "pan_sys.h"

#include "pan_sys_pool.h"

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include "pan_comm_inf.h"
#include "pan_msg_cntr.h"
#include "pan_bcst_hst.h"


#define MARK_CIR(h,i)		(HIS_ELE(h,i).mark)


void
pan_bcast_hist_print(history_p h)
{
    pan_sys_printf("me: %d%d%d%d%d%d\n",
	(MARK_CIR(h, h->circ_base) & SLOT_EMPTY) == 0,
	(MARK_CIR(h, h->circ_base + 1) & SLOT_EMPTY) == 0,
	(MARK_CIR(h, h->circ_base + 2) & SLOT_EMPTY) == 0,
	(MARK_CIR(h, h->circ_base + 3) & SLOT_EMPTY) == 0,
	(MARK_CIR(h, h->circ_base + 4) & SLOT_EMPTY) == 0,
	(MARK_CIR(h, h->circ_base + 5) & SLOT_EMPTY) == 0);
}



void
pan_bcast_hist_init(history_p h)
{				/* initialize history and other shared data */
    int         i;
    history_item_p b;

    h->max_quota = 1;
    while (h->max_quota < 2 * pan_sys_total_platforms && h->max_quota < 32) {
	h->max_quota *= 2;
    }
    assert(h->max_quota <= 32);

    h->buf = pan_calloc(1, h->max_quota * sizeof(history_item_t));
    pan_msg_counter_init(&h->counter, h->max_quota, "HISTORY");
    pan_comm_info_register_counter(HISTORY_CNT, &h->counter);

    h->circ_base = 0;		/* index in the array */

    for (i = 0; i < h->max_quota; i++) {	/* the buffer array */
	pan_msg_counter_hit(&h->counter);
	b = &h->buf[i];
	HIS__MESSAGE(b) = NULL;
	SET__HIS(b, SLOT_EMPTY);
	HIS__MARK(b) = 0;
	HIS__TOKEN(b) = 0;
	HIS__SAMPLE(b) = INT_MAX;
    }
}


void
pan_bcast_hist_clear(history_p h)
{
    int         i;
    pan_fragment_p frag;
    history_item_p b;

    for (i = h->circ_base; i < h->circ_base + h->max_quota; i++) {
	b = HIS_POINTER(h, i);
	if (!TEST__HIS(b, SLOT_EMPTY))
	    pan_sys_printf("history seqno %d is not empty\n", i);
	frag = HIS__MESSAGE(b);
	if (frag != NULL && ! HIS__MARKED_EMPTY(b)) {
	    assert(((pool_entry_p)frag)->pool_mode == OUT_POOL_ENTRY);
	    pan_fragment_clear(frag);
	    pan_sys_printf("message %d left in history?\n", i);
	}
	pan_msg_counter_relax(&h->counter);
    }

    pan_msg_counter_clear(&h->counter);
    pan_free(h->buf);
}



void
dump_buffer_state(history_p h)
{
    pan_fragment_p   ip;
    int         i;

    for (i = 0; i < h->max_quota; i++) {
	ip = HIS_MESSAGE(h, h->circ_base + i);
    }
}
