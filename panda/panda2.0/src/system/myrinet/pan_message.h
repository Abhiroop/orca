/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_MESSAGE_
#define _SYS_GENERIC_MESSAGE_


#include "pan_sys_msg.h"

#include "pan_nsap.h"


#define MAX_RELEASE_FUNCS   0	/* max. no of pushed release functions */


				/* Flags to indicate who created a msg: */
typedef enum PAN_MSG_TYPE_T {
    NORMAL_MSG		= 0,
    FM_MSG		= 0x1 << 0,	/* Created by the FM layer */
    EMBEDDED_DATA	= 0x1 << 1	/* Data in same malloc'ed buffer */
} pan_msg_type_t, *pan_msg_type_p;


				/* Panda message */
struct pan_msg{

    char                  *pm_buf;		/* The data buffer */
    int                    pm_offset;	/* buffer filled to here */
    int                    pm_size;	/* buffer size */

    pan_msg_type_t         pm_type;	/* Who created the msg? */

    pan_nsap_p             pm_nsap;	/* The nsap to which the msg belongs */

#if MAX_RELEASE_FUNCS > 0
					/* Release functions and arguments */
    pan_msg_release_f      pm_func[MAX_RELEASE_FUNCS];
    void                  *pm_arg [MAX_RELEASE_FUNCS];
    int                    pm_nr_rel;
#endif

    pan_msg_p              next;	/* Support for linked lists */
};


/* Some macros to access the buf field of the message */

#define msg_type(msg)		((msg)->pm_type)
#define msg_data(msg)		((msg)->pm_buf)
#define msg_size(msg)		((msg)->pm_size)
#define msg_offset(msg)		((msg)->pm_offset)
#define msg_nsap(msg)		((msg)->pm_nsap)

#define msg_comm_hdr(msg)	((void *)(msg_data(msg) + msg_offset(msg)))
#define msg_comm_hdr_size(msg)	XCAST_HDR_SIZE(msg_nsap(msg))
#define msg_data_len(msg)	(msg_offset(msg) + COMM_HDR_SIZE(msg_nsap(msg)))




void pan_sys_msg_start(void);
void pan_sys_msg_end(void);


#endif
