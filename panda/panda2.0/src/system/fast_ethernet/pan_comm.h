#ifndef _SYS_GENERIC_COMM_
#define _SYS_GENERIC_COMM_


/* The constants & macros that govern the message lay-out */

/*
 *   <--- User -->  <---- Fragm. hdr ---->  <---- Communication hdr --->   *
 *  ---------------------------------------------------------------------  *
 *  | User data   || Fragment | Fragment || System            | nsap id |  *
 *  | (Orca,      || common   | info     || communication     |         |  *
 *  |  RTS hdr,   || hdr      | hdr      || hdr               |         |  *
 *  |  Panda hdr) ||          |          || (unicast/multic.) |         |  *
 *  ---------------------------------------------------------------------  *
 *  ^              ^                      ^                                *
 *  fragment_data()fragment_common_hdr()  fragment_comm_hdr()              *
 *  |             || FRAG_HDR_SIZE(frag) ||   COMM_HDR_SIZE(frag)       |  *
 *  |             ||                     ||   <= MAX_COMM_HDR_SIZE      |  *
 *  |             ||              TOTAL_HDR_SIZE(frag)                  |  *
 *                                                                         *
 * Detail of fragmentation hdr:                                            *
 *                                                                         *
 *   <-------------------- Fragmentation header ----------------------->   *
 *  ---------------------------------------------------------------------  *
 *  |  Common header, shared between   |         fragment header:       |  *
 *  |    fragments of one message      |  fragment size/offset + flags  |  *
 *  ---------------------------------------------------------------------  *
 *  ^                                  ^                                   *
 *  fragment_common_hdr(frag)          fragment_frag_hdr(frag)             *
 *  |       COMMON_HDR_SIZE(frag)      |       FRAG_FRAG_HDR_SIZE       |  *
 *  |         <= MAX_HEADER_SIZE       |                                |  *
 *  |                          FRAG_HDR_SIZE(frag)                      |  *
 *                                                                         *
 * Detail of communication hdr:                                            *
 *                                                                         *
 *   <-------------------- communication header ----------------------->   *
 *  ---------------------------------------------------------------------  *
 *  |    System communication header       |        nsap id             |  *
 *  |      (unicast/multicast hdr)         |                            |  *
 *  ---------------------------------------------------------------------  *
 *  ^                                      ^                               *
 *  fragment_comm_hdr(frag)                fragment_nsap_hdr(frag)         *
 *  |          XCAST_HDR_SIZE(frag)        |        NSAP_HDR_SIZE       |  *
 *  |                            COMM_HDR_SIZE(frag)                    |  *
 *  |                            <= MAX_COMM_HDR_SIZE                   |  *
 *                                                                         *
 *
 *
 * On alignment:
 * The headers are restored from backwards on. This only works, if the
 * alignment of the current header is the maximum of its intrinsic alignment
 * and the alignment of the header after it. So, traversing backwards,
 * the alignment of the headers grows (though not strictly).
 *
 * The system communication hdr struct _must_ have as its _last_ member
 * a short int slot to contain the nsap id, whose name is to be public.
 */


#include <stddef.h>

#include <fm.h>

#include "pan_sys.h"

#include "pan_global.h"
#include "pan_nsap.h"

#include "pan_trace.h"



#define UNIVERSAL_ALIGNMENT	8	/* maximum alignment of all types. */

				/* Works only for alignment to 2-powers */
#define do_align(p, align)	(((p) + (align) - 1) & ~((align) - 1))
#define univ_align(p)		do_align(p, UNIVERSAL_ALIGNMENT)
#define aligned(n, align)	(((n) & ((align) - 1)) == 0)

#define align_of(tp)		offsetof(struct{char __a; tp __tp;}, __tp)



typedef struct PAN_NSAP_HDR_T {
    short int	nsap;
} pan_nsap_hdr_t, *pan_nsap_hdr_p;


typedef struct PAN_UCAST_HDR_T {
    short int	nsap;
} pan_ucast_hdr_t, *pan_ucast_hdr_p;

typedef struct PAN_BCAST_HDR_T {
    short int	nsap;
} pan_mcast_hdr_t, *pan_mcast_hdr_p;





			/* Macros for the offsets in the fragment buffer */

#define NSAP_HDR_ALIGN		align_of(pan_nsap_hdr_t)
#define NSAP_HDR_SIZE		sizeof(pan_nsap_hdr_t)


#define UCAST_COMM_HDR_ALIGN	MAX(align_of(pan_ucast_hdr_t), NSAP_HDR_ALIGN)
#define UCAST_HDR_SIZE		offsetof(pan_ucast_hdr_t, nsap)
#define UCAST_COMM_HDR_SIZE	(UCAST_HDR_SIZE + NSAP_HDR_SIZE)

#define BCAST_COMM_HDR_ALIGN	MAX(align_of(pan_mcast_hdr_t), NSAP_HDR_ALIGN)
#define BCAST_HDR_SIZE		offsetof(pan_mcast_hdr_t, nsap)
#define BCAST_COMM_HDR_SIZE	(BCAST_HDR_SIZE + NSAP_HDR_SIZE)

#define XCAST_HDR_SIZE(n)	(UCAST_HDR_SIZE == BCAST_HDR_SIZE ? \
				 UCAST_HDR_SIZE : \
				 ((n)->type & PAN_NSAP_MULTICAST) ? \
				 BCAST_HDR_SIZE : UCAST_HDR_SIZE)

#define COMM_HDR_ALIGN(n)	(UCAST_COMM_HDR_ALIGN == \
				 BCAST_COMM_HDR_ALIGN ? \
				 UCAST_COMM_HDR_ALIGN : \
				 (((n)->type & PAN_NSAP_MULTICAST) ? \
				  BCAST_COMM_HDR_ALIGN : UCAST_COMM_HDR_ALIGN))
#define COMM_HDR_SIZE(n)	(UCAST_COMM_HDR_SIZE == BCAST_COMM_HDR_SIZE ? \
				 UCAST_COMM_HDR_SIZE : \
				 ((n)->type & PAN_NSAP_MULTICAST) ? \
				 BCAST_COMM_HDR_SIZE : UCAST_COMM_HDR_SIZE)


#define MAX_COMM_HDR_ALIGN	MAX(UCAST_COMM_HDR_ALIGN, BCAST_COMM_HDR_ALIGN)
#define MAX_COMM_HDR_SIZE	MAX(UCAST_COMM_HDR_SIZE, BCAST_COMM_HDR_SIZE)


#define FRAG_FRAG_HDR_ALIGN(n)	MAX(align_of(frag_hdr_t), COMM_HDR_ALIGN(n))
#define MAX_FRAG_FRAG_HDR_ALIGN	MAX(align_of(frag_hdr_t), MAX_COMM_HDR_ALIGN)
#define FRAG_FRAG_HDR_SIZE(n)	do_align(sizeof(frag_hdr_t), \
					 FRAG_FRAG_HDR_ALIGN(n))
#define MAX_FRAG_FRAG_HDR_SIZE	do_align(sizeof(frag_hdr_t), \
					 MAX_FRAG_FRAG_HDR_ALIGN)

#define COMMON_HDR_SIZE(n)	do_align((n)->hdr_size, FRAG_FRAG_HDR_ALIGN(n))

#define FRAG_HDR_SIZE(n)	(((n)->type & PAN_NSAP_SMALL) ? \
				 0 : COMMON_HDR_SIZE(n) + FRAG_FRAG_HDR_SIZE(n))

#define MAX_COMMON_HDR_SIZE	do_align(MAX_HEADER_SIZE, \
					 MAX_FRAG_FRAG_HDR_ALIGN)
#define MAX_FRAG_HDR_SIZE	(MAX_COMMON_HDR_SIZE + MAX_FRAG_FRAG_HDR_SIZE)

#define TOTAL_HDR_SIZE(n)	(FRAG_HDR_SIZE(n) + COMM_HDR_SIZE(n))

#define MAX_TOTAL_HDR_SIZE	(MAX_FRAG_HDR_SIZE + MAX_COMM_HDR_SIZE)





typedef void (*pan_fragment_rcve_p)(pan_fragment_p frag);


extern pan_mutex_p	pan_fm_send_lock;


void pan_sys_comm_start(void);
void pan_sys_comm_wakeup(void);
void pan_sys_comm_end(void);


int  pan_sys_sprint_comm_stat_size(void);
void pan_sys_sprint_comm_stats(char *buf);

#ifdef STATISTICS
extern pan_mutex_p pan_comm_statist_lock;

#define STATINC(counter) \
	do { \
	    pan_mutex_lock(pan_comm_statist_lock); \
	    ++(counter); \
	    pan_mutex_unlock(pan_comm_statist_lock); \
	} while (0)

extern int pan_n_mcast_frag_rcve;
extern int pan_n_mcast_small_rcve;
extern int pan_n_mcast_frag_send;
extern int pan_n_mcast_small_send;
#else
#define STATINC(counter)
#endif



void pan_rcve_enqueue(pan_fragment_p frag, pan_fragment_rcve_p rcve_func);


extern trc_event_t     trc_start_upcall;
extern trc_event_t     trc_end_upcall;

#endif

