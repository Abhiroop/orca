#ifndef __PAN_SYS_NNN12_H__
#define __PAN_SYS_NNN12_H__

#include <pan_sys.h>			/* for type pan_fragment_p */

#include "pan_msg_cntr.h"		/* for type pan_msg_counter_t */
#include "pan_bcst_hst.h"		/* for type history_t */




			/* The public part of the broadcast state */

typedef struct PUB_BCAST_DATA_T {
    history_t         hist;		/* The history buffer */
    int               cong_all;		/* # of items in the queue */
    pan_msg_counter_t snd_counter;	/* sync between senders */
} pub_bcast_state_t, *pub_bcast_state_p;


/* Macros for easier access to the history fields of a bcast_state_p */

#define BC_HIS_POINTER(st,i)	HIS_POINTER(&(st)->hist,i)

#define BC_SET_HIS(st,i,m)	SET_HIS(&(st)->hist,i,m)
#define BC_CLEAR_HIS(st,i,f)	CLEAR_HIS(&(st)->hist,i,f)
#define BC_TEST_HIS(st,i,f)	TEST_HIS(&(st)->hist,i,f)

#define BC_PUT_HIS(st,i,m)	PUT_HIS(&(st)->hist,i,m)
#define BC_GET_HIS(st,i)	GET_HIS(&(st)->hist,i)

#define BC_HIS_MK_EMPTY(st,i)	HIS_MARK_EMPTY(&(st)->hist,i)
#define BC_HIS_MKED_EMPTY(st,i)	HIS_MARKED_EMPTY(&(st)->hist,i)

#define BC_HIS_LAST(st)		HIS_POINTER(&(st)->hist,(st)->hist.circ_base)
#define BC_HIS_UPB(st)		((st)->hist.circ_base + (st)->hist.max_quota)

#define BC_HIS_SIZE(st)		((st)->hist.max_quota)

#define BC_HIS_BASE(st)		((st)->hist.circ_base)



extern pub_bcast_state_t pan_bcast_state;	/* public part of broadcast
						 * data structure */


			/* Broadcast a fragment: a system layer header is
			 * pushed on it, and it is enqueued in the bcast
			 * history buffer.
			 * ! The fragment is lost after this call. */
void pan_comm_bcast_snd_new(pan_fragment_p frag,
			    int control, int seqno, int sender,
			    pan_msg_counter_p x_counter);

			/* Wait until processing is ready up to this seqno
			 */
void pan_comm_bcast_await(int seqno);

			/* Fill in bcast forward info */
void pan_comm_bcast_fwd_info(void);

			/* Start bcast forward module */
void pan_comm_bcast_fwd_start(void);

			/* End bcast forward module */
void pan_comm_bcast_fwd_end(void);

			/* Print bcast state */
void pan_sys_bcast_print_state(void);

			/* Print our image of neighbour bcast state */
void pan_sys_neighb_print_state(void);

#endif
