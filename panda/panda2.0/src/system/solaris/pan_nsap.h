/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_NSAP_
#define _SYS_GENERIC_NSAP_

#include "pan_fragment.h"

typedef enum{
    PAN_NSAP_FRAGMENT,
    PAN_NSAP_SMALL
}pan_nsap_addr_t;


struct pan_nsap{
    pan_nsap_addr_t  type;	/* type of nsap */
    int              nsap_id;	/* nsap id used to identify handler */
    int              flags;	/* usage flags (unicast, multicast) */

    /* For type == PAN_NSAP_FRAGMENT */
    void           (*rec_frag)(pan_fragment_p frag);
				/* receive handler */
    int              hdr_size;	/* header size  */

    /* For type == PAN_NSAP_SMALL */
    void           (*rec_small)(void *data);
				/* receive handler */
    int              data_len;	/* data size */
    int              comm_hdr_size;	/* size of communication hdr */
};



#define NSAP_HDR_SIZE sizeof(int)

extern void       pan_sys_nsap_start(void);
extern void       pan_sys_nsap_end(void);
extern pan_nsap_p pan_sys_nsap_id(int id);
extern void       pan_sys_nsap_comm_hdr(pan_nsap_p nsap, int comm_hdr_size);

#define pan_sys_nsap_index(nsap)  ((nsap)->nsap_id)

#endif

