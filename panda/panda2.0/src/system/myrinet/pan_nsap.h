/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_NSAP_
#define _SYS_GENERIC_NSAP_

#include "pan_comm.h"
#include "pan_message.h"

typedef enum{
    /* PAN_NSAP_UNICAST		= 0x1 << 0, */
    /* PAN_NSAP_MULTICAST	= 0x1 << 1, */
    PAN_NSAP_FRAGMENT		= 0x1 << 2,
    PAN_NSAP_SMALL		= 0x1 << 3
}pan_nsap_addr_t;


struct pan_nsap{
    pan_nsap_addr_t  type;		/* type of nsap */
    short            nsap_id;		/* nsap id used to identify handler */
    int              flags;		/* usage flags (unicast, multicast) */

    /* For type == PAN_NSAP_FRAGMENT */
    void           (*rec_msg)(pan_msg_p msg);
					/* receive handler */

    /* For type == PAN_NSAP_SMALL */
    void           (*rec_small)(void *data);
					/* receive handler */
    int              data_len;		/* data size */
    int              comm_hdr_size;	/* size of communication hdr */
    char	     pad[4];		/* too speed up address calculations */
};



extern void       pan_sys_nsap_start(void);
extern void       pan_sys_nsap_end(void);
/*
extern pan_nsap_p pan_sys_nsap_id(short id);
extern short int  pan_sys_nsap_index(pan_nsap_p nsap);
*/

#define MAX_NSAPS 32

extern struct pan_nsap pan_sys_nsaps[MAX_NSAPS];
extern int             pan_sys_nsap_nr;

#define pan_sys_nsap_id(id) \
	(assert((id) >= 0 && (id) < (pan_sys_nsap_nr)), &pan_sys_nsaps[id])
 

#define pan_sys_nsap_index(nsap)  ((nsap) - pan_sys_nsaps)

#endif

