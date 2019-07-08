/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_FRAGMENT_
#define _SYS_GENERIC_FRAGMENT_

#include "pan_sys.h"		/* for fragment flags */
#include "pan_sys_pool.h"
#include "pan_comm.h"		/* for MAX_COMM_HDR_SIZE */

/* Alignment operations */
/* #define UNIVERSAL_ALIGNMENT 8	\* Must be a factor of 2 */
/* define the universal alignment in the Makefile for the system layer,
 * because it is architecture dependent.
 */

#define aligned(p, align)   ((((long)(p)) & ((align) - 1)) == 0)
#define do_align(p, align)  (((p) + (align) - 1) & ~((align) - 1))

/* if mul is not a factor of 2 */
#define multiple(x, mul) ((((x) + (mul) - 1) / (mul)) * (mul))



/*
 * Fragment header information. A fragment is a piece of data that is
 * sent as a packet. The user is responsible to assemble the fragments in
 * the order in which they were generated. A fragment identifies the size
 * of the buffer and some flags.
 *
 * A special implementation can be acquired if the compile-time flag
 * SINGLE_FRAGMENT is set. In this case, a fragment always consists of
 * the complete message data. This is useful if the underlying system
 * supports large messages. To receive such a fragment, we distinguish
 * three cases:
 * - The fragment to be sent is smaller than PACKET_SIZE. In this case,
 *   the fragment can be sent to a normal receive fragment. (All
 *   fragments have a buffer of PACKET_SIZE by default.)
 * - The fragment to be sent is larger than PACKET_SIZE. In this case,
 *   the receiver has to be notified somehow of the data size to be
 *   received. A call to pan_sys_fragment_resize is provided to set the
 *   data buffer of the receive fragment so that it fits the data to be
 *   received. (The data buffer size is always a multiple of
 *   PACKET_SIZE.)
 * - If the underlying system layer delivers a malloced buffer. XXX: todo
 */

#ifndef SINGLE_FRAGMENT
typedef struct{
    char flags;			/* Flags FIRST, LAST*/
    short size;			/* Size of buffer in #PACKET_SIZE */
}frag_hdr_t, *frag_hdr_p;

#define FRAG_HDR_SIZE      (do_align(sizeof(frag_hdr_t), alignof(frag_hdr_t)))
#else  /* SINGLE_FRAGMENT */
#define FRAG_HDR_SIZE      0
#endif /* SINGLE_FRAGMENT */

#define MAX_TOTAL_HDR_SIZE (MAX_HEADER_SIZE + FRAG_HDR_SIZE + \
			    MAX_COMM_HDR_SIZE + NSAP_HDR_SIZE)

typedef struct pan_fragment{
    POOL_HEAD;			/* Pool managment */
    char           *data;	/* Data buffer */
    int             buf_size;	/* Size of data buffer */
    int             size;	/* Size of data in data buffer */
    int             owner;	/* Owner of data buffer */
#ifndef SINGLE_FRAGMENT
    int             offset;	/* Offset of this fragment in the message */
    frag_hdr_p      header;	/* Fragment header info */
#endif
    pan_nsap_p      nsap;	/* Nsap associated with this fragment */
}pan_fragment_t;

extern void       pan_sys_fragment_start(void);
extern void       pan_sys_fragment_end(void);

/*
 * At the system layer, some headers are pushed onto a fragment:
 *
 * 1. If the communication layer does piggy-backing of (some of) its
 *    state, it can push a header of size BCAST_COMM_HDR_SIZE or
 *    UCAST_COMM_HDR_SIZE, depending on the multicast flag of this
 *    nsap. These sizes are defined in the non-generic file pan_comm.h;
 *
 * 2. The id of the nsap to which the fragment belongs. This id (of size
 *    NSAP_HDR_ID) is always at the end of a sent fragment.
 *
 * The following functions provide access.
 *
 * Upon receipt of a fragment, always call pan_sys_fragment_nsap_pop()
 * immediately to obtain the nsap to which the fragment corresponds.
 * This function has side-effects on the fragment so be sure not to call
 * it twice!
 *
 * Before sending out a fragment, push:
 * 1. communication layer hdr (if needs be)
 * 2. nsap id.
 */

extern pan_nsap_p pan_sys_fragment_nsap_pop(pan_fragment_p frag, int len);
extern void      *pan_sys_fragment_comm_hdr_pop(pan_fragment_p frag);

/*
 * After network receive of a fragment, pop:
 * 1. nsap id
 * 2. communication layer hdr (if needs be)
 */
extern int        pan_sys_fragment_nsap_push(pan_fragment_p frag);
extern void      *pan_sys_fragment_comm_hdr_push(pan_fragment_p frag);

/* After reception or push: */
extern pan_nsap_p pan_sys_fragment_nsap_look(pan_fragment_p frag);
extern void      *pan_sys_fragment_comm_hdr_look(pan_fragment_p frag);

#ifdef SINGLE_FRAGMENT
void pan_sys_fragment_resize(pan_fragment_p frag, int size);
#endif

#define TOTAL_HDR_SIZE(frag) \
(NSAP_HDR_SIZE + (frag)->nsap->comm_hdr_size + \
 (((frag)->nsap->type == PAN_NSAP_SMALL) \
  ? 0 : ((frag)->nsap->hdr_size) + FRAG_HDR_SIZE))


#endif


