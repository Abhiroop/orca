#ifndef _SYS_GENERIC_COMM_
#define _SYS_GENERIC_COMM_


/* The constants & macros that govern the message lay-out */

/*
 *  <-- Data buffer -->  <---------- Communication hdr ------------>   *
 *  -----------------------------------------------------------------  *
 *  | User data        || System communication  |     nsap id       |  *
 *  |  (Orca, RTS hdr, || hdr (unicast/multic)  |                   |  *
 *  |  Panda hdr)      ||                       |                   |  *
 *  -----------------------------------------------------------------  *
 *  ^                   ^                       ^                      *
 *  msg_data(msg)       msg_comm_hdr(msg)       msg_nsap_hdr(msg)      *
 *                      |   [UB]CAST_HDR_SIZE   | NSAP_HDR_SIZE     |  *
 *                      |            [UB]CAST_COMM_HDR_SIZE         |  *
 *                                                                     *
 *
 * The system communication hdr struct _must_ have as its _last_ member
 * a short int slot to contain the nsap id, whose name is to be public.
 */

#include <stddef.h>

#include "pan_sys_msg.h"

#include "pan_nsap.h"

#include "pan_trace.h"



#define UNIVERSAL_ALIGNMENT	8	/* maximum alignment of all types. */

				/* Works only for alignment to 2-powers */
#define do_align(p, align)	(((p) + (align) - 1) & ~((align) - 1))
#define univ_align(p)		do_align(p, UNIVERSAL_ALIGNMENT)
#define aligned(n, align)	(((n) & ((align) - 1)) == 0)

#define align_of(tp)		offsetof(struct{char __a; tp __tp;}, __tp)


#define MIN(x, y)		((x) < (y) ? (x) : (y))
#define MAX(x, y)		((x) > (y) ? (x) : (y))

typedef struct PAN_NSAP_HDR_T {
    short int	nsap;
} pan_nsap_hdr_t, *pan_nsap_hdr_p;



typedef struct PAN_UCAST_HDR_T {
    short int	nsap;
} pan_ucast_hdr_t, *pan_ucast_hdr_p;

#include "pan_mcast_header.h"


			/* Macros for the offsets in the fragment buffer */

#define NSAP_HDR_ALIGN		align_of(pan_nsap_hdr_t)
#define NSAP_HDR_SIZE		sizeof(pan_nsap_hdr_t)

#define UCAST_HDR_ALIGN		MAX(align_of(pan_ucast_hdr_t), NSAP_HDR_ALIGN)
#define UCAST_HDR_SIZE		offsetof(pan_ucast_hdr_t, nsap)
#define BCAST_HDR_ALIGN		MAX(align_of(pan_mcast_hdr_t), NSAP_HDR_ALIGN)
#define BCAST_HDR_SIZE		offsetof(pan_mcast_hdr_t, nsap)

#define UCAST_COMM_HDR_ALIGN	UCAST_HDR_ALIGN
#define UCAST_COMM_HDR_SIZE	(UCAST_HDR_SIZE + NSAP_HDR_SIZE)
#define BCAST_COMM_HDR_ALIGN	BCAST_HDR_ALIGN
#define BCAST_COMM_HDR_SIZE	(BCAST_HDR_SIZE + NSAP_HDR_SIZE)

#define MAX_COMM_HDR_ALIGN	MAX(BCAST_COMM_HDR_ALIGN, UCAST_COMM_HDR_ALIGN)
#define MAX_COMM_HDR_SIZE	MAX(BCAST_COMM_HDR_SIZE, UCAST_COMM_HDR_SIZE)

#define XCAST_HDR_ALIGN(nsp)	(((nsp)->type & PAN_NSAP_MULTICAST) ? \
				 BCAST_HDR_ALIGN : UCAST_HDR_ALIGN)
#define XCAST_HDR_SIZE(nsp)	(((nsp)->type & PAN_NSAP_MULTICAST) ? \
				 BCAST_HDR_SIZE : UCAST_HDR_SIZE)

#define COMM_HDR_ALIGN(nsp)	XCAST_HDR_ALIGN(nsp)
#define COMM_HDR_SIZE(nsp)	(XCAST_HDR_SIZE(nsp) + NSAP_HDR_SIZE)

#define TOTAL_HDR_SIZE(nsp)	COMM_HDR_SIZE(nsp)

#define MAX_TOTAL_HDR_SIZE	MAX_COMM_HDR_SIZE



void pan_sys_comm_start(void);
void pan_sys_comm_wakeup(void);
void pan_sys_comm_end(void);


int  pan_sys_sprint_comm_stat_size(void);
void pan_sys_sprint_comm_stats(char *buf);

#ifdef STATISTICS

#include "pan_sync.h"

extern pan_mutex_p pan_comm_statist_lock;

extern int pan_n_mcast_rcve_data;
extern int pan_n_mcast_rcve_small;
extern int pan_n_mcast_send_data;
extern int pan_n_mcast_send_small;

#  define SYS_STATINC(n) \
	do { \
	    pan_mutex_lock(pan_comm_statist_lock); \
	    ++(n); \
	    pan_mutex_unlock(pan_comm_statist_lock); \
	} while (0)

#else

#  define SYS_STATINC(n)

#endif

extern trc_event_t     trc_start_upcall;
extern trc_event_t     trc_end_upcall;

#endif

