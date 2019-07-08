#ifndef _SYS_T800_COMM_
#define _SYS_T800_COMM_

#include <sys/sem.h>
#include <sys/link.h>
#include <sys/thread.h>

#include "pan_nsap.h"
#include "pan_msg_cntr.h"
#include "pan_bcst_hst.h"		/* for type word_t */

#include "pan_trace.h"			/* for type trc_event_t */


/* Tags for 'tag' field of REQUEST-Messages: */

typedef enum sys_msg_tag {
			/* control tags */
    DEBUG_TAG_1,		/* unused */
    DEBUG_TAG_2,		/* unused */
    STOP_TAG,			/* Terminate sequencer link/RR thread */
    TERM_TAG,			/* Terminate sequencer RR thread */
			/* sequence number request */
    REQUEST_TAG,		/* Request */
    SIMPLE_REQUEST_TAG,		/* Express message request */
    			/* reply tags */
    REPLY_TAG,			/* Sequencer decides it is GSB */
    PB_REPLY_TAG,		/* Sequencer requests data for PB */
    PB_DONE_TAG			/* Sequencer has done PB */
} sys_msg_tag_t, *sys_msg_tag_p;



typedef struct mcast_req {
    union {
	int       seqno;
	int       sender;
    } i;
    sys_msg_tag_t tag;
} mcast_req_t, *mcast_req_p;




/* Bits in 'control' field of data/meta-messages: */

typedef enum tag_bits {
    CLOCKWISE		= (0x1 << 0),
    COUNTERCW		= (0x1 << 1),	/* for Broadcast-Orientation */

    DATA_MESSAGE	= (0x1 << 2),
    META_MESSAGE	= (0x1 << 3),
    UNICAST		= (0x1 << 4),	/* for Message-Identification */

    STOP_PROGRAM	= (0x1 << 5),
    FINAL_STOP		= (0x1 << 6)	/* for Termination / Debugging */
} tag_bits_t, *tag_bits_p;

#define REQ_UPDATE(b)   ((b) << 7)	/* 2^7 > FINAL_STOP !!! */

#define ORIENTATION_BITS (CLOCKWISE | COUNTERCW)
#define MESSAGE_BITS     (DATA_MESSAGE | META_MESSAGE | UNICAST)
#define REPORT_BITS      (STOP_PROGRAM | FINAL_STOP)


typedef struct BCAST_HDR {
    unsigned short int control;
    short int          sender;
    short int          row_lim;
    short int          col_lim;
    int                seqno;
    word_t             mirror1;
    int                tag1;
    word_t             mirror2;
    int                tag2;
    mcast_req_t        mcast_req;
} bcast_hdr_t, *bcast_hdr_p;


typedef struct UCAST_HDR {
    unsigned short int control;
    short int          dest;
} ucast_hdr_t, *ucast_hdr_p;


				/* The small msg lay-out is enforced by fragment
				 * data lay-out. DO NOT CHANGE thoughtlessly! */
typedef struct BCAST_SMALL_MSG {
    char          data[MAX_SMALL_SIZE];
    bcast_hdr_t   hdr;
    nsap_t        nsap_id;
} bcast_small_msg_t, *bcast_small_msg_p;

typedef struct UCAST_SMALL_MSG {
    char          data[MAX_SMALL_SIZE];
    ucast_hdr_t   hdr;
    nsap_t        nsap_id;
} ucast_small_msg_t, *ucast_small_msg_p;


#define SYSTEM_PRIORITY_L   LOW_PRIORITY
#define SYSTEM_PRIORITY_H   LOW_PRIORITY

#define STACK               8192	/* Stack-Size of system-layer threads */


/* RequestIDs for mailbox msg exchange ("random" communication).
 * Chosen arbitrarily */

#define REQ_ORMU   456		/* Bcast id: request/reply for sequence no */
#define GET_MESS   789		/* Unicast id */


/* RequestIDs for MakeLink/GetLink/ConnectLink.
 * Chosen arbitrarily */

#define STAR_TOP_1	901	/* ID for bcast request link to sequencer */
#define STAR_TOP_2	914	/* ID for bcast data link to sequencer */

#define UCAST_TOP	1000	/* ID for unicast links */

#define INFO_M		75	/* ID for info exchange */


/* Events for rough monitoring of system thread activity */

extern trc_event_t	pan_trc_start_upcall;
extern trc_event_t	pan_trc_end_upcall;


/* Data and locks shared between bcast, seq and ucast */

extern int		pan_sys_x;
extern int		pan_sys_y;	/* Node-Coordinates of this processor */

extern LinkCB_t	       *pan_bcast_data_link;
extern LinkCB_t	       *pan_bcast_req_link;

extern Semaphore_t	pan_comm_upcall_lock;	/* make upcalls one by one! */
extern Semaphore_t	pan_comm_req_lock;	/* request seq no's 1 by 1 */




#define BCAST_COMM_HDR_SIZE	univ_align(sizeof(bcast_hdr_t))
#define UCAST_COMM_HDR_SIZE	univ_align(sizeof(ucast_hdr_t))

#define MAX_COMM_HDR_SIZE	(BCAST_COMM_HDR_SIZE > UCAST_COMM_HDR_SIZE ? \
				 BCAST_COMM_HDR_SIZE : UCAST_COMM_HDR_SIZE)




#define MAKE_UPCALL(qp) \
	do { \
	    Wait(&pan_comm_upcall_lock); \
	    trc_event(pan_trc_start_upcall, NULL); \
	    if (qp->nsap->type == PAN_NSAP_SMALL) { \
		qp->nsap->rec_small(qp->data); \
	    } else { \
		qp->nsap->rec_frag(qp); \
	    } \
	    trc_event(pan_trc_end_upcall, NULL); \
	    Signal(&pan_comm_upcall_lock); \
	} while (0)


pan_fragment_p pan_comm_small2frag(void *data, pan_nsap_p nsap);

void           pan_sys_comm_start(int n_servers);

void           pan_sys_comm_end(void);

void           pan_sys_frag_print(pan_fragment_p frag);

#endif
