#ifndef __PAN_SYS_BCAST_HIST_H__
#define __PAN_SYS_BCAST_HIST_H__


#include "pan_sys.h"





#define SLOT_EMPTY   32		/* mark for 'no message' */


typedef long unsigned int word_t;  /* this restricts hist.max_quota to 32 */


typedef struct HISTORY_ITEM_T {
    pan_fragment_p msgp;	/* pointer to a message */
    int            mark;	/* forwarding directions, SLOT_EMPTY, 'marks' */
    int            waiters;	/* number of waiting threads that want to use
				 * this slot */
    int            token;	/* number of threads signaled */
    int            seqno_sample;/* remember a seqno, to break the carry-cycle */
} history_item_t, *history_item_p;


typedef struct HISTORY_T {
    int               circ_base;	/* expected seq no of orderer */
    int               max_quota;
    pan_msg_counter_t counter;
    history_item_p    buf;
} history_t, *history_p;



#define BUF_MOD(h,i)		(i & ((h)->max_quota - 1))
#define HIS_ELE(h,i)		((h)->buf[BUF_MOD(h,i)])
#define HIS_POINTER(h,i)	(&((h)->buf[BUF_MOD(h,i)]))

#define GET_HIS(h,i)		(HIS_ELE(h,i).msgp)
#define PUT_HIS(h,i,p)		(HIS_ELE(h,i).msgp = p)

#define SET_HIS(h,i,b)		(HIS_ELE(h,i).mark |= (b))
#define CLEAR_HIS(h,i,b)	(HIS_ELE(h,i).mark &= ~(b))
#define TEST_HIS(h,i,b)		(HIS_ELE(h,i).mark & (b))

#define SET__HIS(his_elem,b)	((his_elem)->mark |= (b))
#define CLEAR__HIS(his_elem,b)	((his_elem)->mark &= ~(b))
#define TEST__HIS(his_elem,b)	((his_elem)->mark & (b))

#define HIS_MESSAGE(h,i)	(HIS_ELE(h,i).msgp)
#define HIS_MARK(h,i)		(HIS_ELE(h,i).waiters)
#define HIS_TOKEN(h,i)		(HIS_ELE(h,i).token)
#define HIS_SAMPLE(h,i)		(HIS_ELE(h,i).seqno_sample)

#define HIS_MARK_EMPTY(h,i)	(HIS_MESSAGE(h,i) = (pan_fragment_p) -1)
#define HIS_MARKED_EMPTY(h,i)	(HIS_MESSAGE(h,i) == (pan_fragment_p) -1)

#define HIS__MESSAGE(his_elem)	((his_elem)->msgp)
#define HIS__MARK(his_elem)	((his_elem)->waiters)
#define HIS__TOKEN(his_elem)	((his_elem)->token)
#define HIS__SAMPLE(his_elem)	((his_elem)->seqno_sample)

#define HIS__MARK_EMPTY(his_elem) ((his_elem)->msgp = (pan_fragment_p) -1)
#define HIS__MARKED_EMPTY(his_elem) ((his_elem)->msgp == (pan_fragment_p) -1)


void pan_bcast_hist_print(history_p h);

void pan_bcast_hist_init(history_p h);

void pan_bcast_hist_clear(history_p h);

void dump_buffer_state(history_p h);


#endif
