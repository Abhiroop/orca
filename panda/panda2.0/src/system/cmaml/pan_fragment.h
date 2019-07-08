#ifndef _SYS_CMAML_FRAGMENT_
#define _SYS_CMAML_FRAGMENT_

#include "pan_sys.h"		/* for fragment flags */
#include "pan_buffer.h"
#include "pan_sys_pool.h"

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
    int flags;			/* Flags FIRST, LAST*/
} frag_hdr_t, *frag_hdr_p;


/*
 * Reserve some space for assembly information.
 */
#define FRAG_UA          (UNIVERSAL_ALIGNMENT - 1)
#define FRAG_HDR_SIZE    ((sizeof(frag_hdr_t) + FRAG_UA) & ~FRAG_UA)
#define FREE_SIZE        (MAX_HEADER_SIZE + FRAG_HDR_SIZE)

typedef struct pan_fragment{
    POOL_HEAD;			/* Pool managment */
    char               *data;
    int                 size;
    int                 owner;
    frag_hdr_p          header;
    pan_nsap_p          nsap;
    int                 f_bufsize;/* size of attached data buffer */

    /* Active messages part of fragment header. */
    struct pan_receive *f_bufent; /* buffer entry that holds this fragment */
    int                 f_src;    /* sender of this fragment */
    int                 f_root;   /* original sender */
    int                 f_size;   /* total xmit size */
    int                 f_seqno;  /* sequence number */
    pan_fragment_p      f_next;
} pan_fragment_t;

extern void       pan_sys_fragment_start(void);
extern void       pan_sys_fragment_end(void);

#define TOTAL_HDR_SIZE(frag) \
                ((((frag)->nsap->type == PAN_NSAP_SMALL) ? 0 : \
                                  ((frag)->nsap->hdr_size) + FRAG_HDR_SIZE))
 
#endif /* _SYS_CMAML_FRAGMENT_ */
