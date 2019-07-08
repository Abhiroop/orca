/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_FRAGMENT_
#define _SYS_GENERIC_FRAGMENT_

#include "pan_sys.h"		/* for fragment flags */
#include "pan_buffer.h"
#include "pan_sys_pool.h"
#include "pan_comm.h"		/* for MAX_COMM_HDR_SIZE */

/*
 * Fragment header information. Should be compressed. A fragment is a
 * piece of data that is sent as a packet. It contains information to
 * identify its position in a message. The user is responsible to
 * assemble the fragments in the order in which they were generated. A
 * message consists of a list of buffers, and each buffer is split up
 * into multiple fragments, if necessary. (Ideally, all buffers consist
 * of one fragment, because that reduces data copying.) A fragment
 * identifies the buffer to which it belongs, the size of the buffer, and
 * some flags.
 */
typedef struct{
    int index;			/* Index of buffer in message */
    int size;			/* Size of buffer */
    int offset;			/* Offset of fragment in buffer */
    int flags;			/* Flags FIRST, LAST*/
}frag_hdr_t, *frag_hdr_p;


/*
 * Reserve some space for assembly information.
 */
#define FRAG_HDR_SIZE    (sizeof(frag_hdr_t))
#define FREE_SIZE        (MAX_HEADER_SIZE + FRAG_HDR_SIZE + \
			  MAX_COMM_HDR_SIZE + NSAP_HDR_SIZE)

/* Used in combination with PAN_FRAGMENT_(FIRST|LAST) */
#define PAN_FRAGBUF_FIRST 0x04
#define PAN_FRAGBUF_LAST  0x08

typedef struct pan_fragment{
    POOL_HEAD;			/* Pool managment */
    char           *data;
    int             size;
    int             owner;
    frag_hdr_p      header;
    pan_nsap_p      nsap;
}pan_fragment_t;

extern void       pan_sys_fragment_start(void);
extern void       pan_sys_fragment_end(void);

/* At the system layer, some headers are pushed onto a fragment:
 * 1. If the communication layer does piggy-backing of (some of) its state,
 *    it can push a header of size BCAST_HDR_SIZE or UCAST_HDR_SIZE,
 *    depending on the multicast flag of this nsap. These sizes are defined in
 *    the non-generic file pan_comm.h;
 * 2. The id of the nsap to which the fragment belongs. This id (of size
 *    NSAP_HDR_ID) is always at the end of a sent fragment.
 *
 * The following functions provide access.
 * Upon receipt of a fragment, always call pan_sys_fragment_nsap_pop()
 * immediately to obtain the nsap to which the fragment corresponds.
 * This function has side-effects on the fragment so be sure not to call it
 * twice!
 */
				/* Before sending out a fragment, push:
				 * 1. communication layer hdr (if needs be)
				 * 2. nsap id. */
extern pan_nsap_p pan_sys_fragment_nsap_pop(pan_fragment_p frag, int len);
extern void      *pan_sys_fragment_comm_hdr_pop(pan_fragment_p frag);

				/* After network receive of a fragment, pop:
				 * 1. nsap id
				 * 2. communication layer hdr (if needs be) */
extern int        pan_sys_fragment_nsap_push(pan_fragment_p frag);
extern void      *pan_sys_fragment_comm_hdr_push(pan_fragment_p frag);

				/* After reception or push: */
extern pan_nsap_p pan_sys_fragment_nsap_look(pan_fragment_p frag);
extern void      *pan_sys_fragment_comm_hdr_look(pan_fragment_p frag);

#define TOTAL_HDR_SIZE(frag) \
		(NSAP_HDR_SIZE + (frag)->nsap->comm_hdr_size + \
		    (((frag)->nsap->type == PAN_NSAP_SMALL) \
			 ? 0 : ((frag)->nsap->hdr_size) + FRAG_HDR_SIZE))

extern char *pan_sys_fragment_data(pan_fragment_p frag, int *size, int *owner,
				   int *buf_size, int *buf_index, int *flags,
				   int preserve);


#endif


