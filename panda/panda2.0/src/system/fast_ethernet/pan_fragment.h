/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_FRAGMENT_
#define _SYS_GENERIC_FRAGMENT_

#include "pan_sys.h"

#include "pan_buffer.h"
#include "pan_comm.h"
#include "pan_nsap.h"


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
/* The header is compressed. It functionally contains three fields:
 * 1. unsigned	h_buf_flags
 * 2. int	h_buf_size
 * 3. int	h_buf_offset
 *
 * If this is a first fragment, h_buf_size contains the total size of the
 * source buffer. h_buf_offset is always 0.
 * If this is a later fragment, h_buf_size is meaningless, and h_buf_offset
 * indicates where in the receiving buffer to put this fragment.
 * The fragment size is delivered by the upcall from the system layer.
 * Conclusion: h_buf_size and h_buf_offset can be shared.
 * Moreover, both are aligned with UNIVERSAL_ALIGNMENT, which is generally >= 4.
 * The 2 least significant bits are always zero. THese can be used to contain
 * the flags PAN_FRAGMENT_FIRST and PAN_FRAGMENT_LAST.
 */

typedef struct {
					/* Lower 2 bits contain flags */
    					/* Start address of data in buffer */
    int			h_buf_size;	/* Remaining size of buffer, incl this*/
} frag_hdr_t, *frag_hdr_p;


#define frag_hdr_size(n)		((n) & ~0x3)
#define frag_hdr_flags(n)		((n) & 0x3)
#define frag_hdr_clr_size(n)		((n) &= 0x3)
#define frag_hdr_clr_flags(n)		((n) &= ~0x3)

#define frag_hdr_set_size(h, n)		(assert(! frag_hdr_flags(n)), \
					 frag_hdr_clr_size((h)->h_buf_size), \
					 (h)->h_buf_size |= frag_hdr_size(n))
#define frag_hdr_set_flags(h, n)	(assert(! frag_hdr_size(n)), \
					 frag_hdr_clr_flags((h)->h_buf_size), \
					 (h)->h_buf_size |= frag_hdr_flags(n))
#define frag_hdr_set_offset(h, n)	frag_hdr_set_size(h, n)

#define frag_hdr_get_size(hdr)		(frag_hdr_size((hdr)->h_buf_size))
#define frag_hdr_get_flags(hdr)		(frag_hdr_flags((hdr)->h_buf_size))
#define frag_hdr_get_offset(hdr)	frag_hdr_get_size(hdr)



typedef struct pan_fragment{
    char	       *f_start;	/* Start of fragment data in buffer */
    int			f_size;		/* Size of fragment data */
    int			f_embedded;	/* Is this a frag embedded in a msg */
    pan_nsap_p		f_nsap;		/* Frag's nsap */
    pan_sys_buffer_p	f_buffer;	/* Frag's buffer */
    pan_fragment_p	next;		/* linked list support */
}pan_fragment_t;


			/* Macros to access the fragment fields */

#define fragment_data(f)	((f)->f_start)
#define fragment_size(f)	((f)->f_size)
#define fragment_buffer(f)	((f)->f_buffer)
#define fragment_nsap(f)	((f)->f_nsap)
#define fragment_embedded(f)	((f)->f_embedded)
#define fragment_common_hdr(f)	(fragment_data(f) + fragment_size(f))
#define fragment_frag_hdr(f)	((frag_hdr_p)(fragment_common_hdr(f) + \
					      COMMON_HDR_SIZE(f->f_nsap)))
#define fragment_nsap_hdr(f)	((short *)(fragment_data(f) + \
					   fragment_size(f) + \
					   FRAG_HDR_SIZE(f->f_nsap) + \
					   XCAST_HDR_SIZE(f->f_nsap)))


extern int        pan_sys_fragment_data_len(pan_fragment_p frag);

extern void       pan_sys_fragment_start(void);
extern void       pan_sys_fragment_end(void);



#endif


