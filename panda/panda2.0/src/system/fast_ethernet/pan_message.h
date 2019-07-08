/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_MESSAGE_
#define _SYS_GENERIC_MESSAGE_

#define MAX_RELEASE_FUNCS   0	/* max. no of pushed release functions */

#include "pan_sys.h"
#include "pan_buffer.h"
#include "pan_fragment.h"


typedef enum MSG_T {
    MSG_NORMAL		= 0,
    MSG_EMBEDDED	= (0x1) << 0
} msg_t, *msg_p;


/* Panda message */
struct pan_msg{

    msg_t	           pm_type;
    pan_sys_buffer_p       pm_buffer;		/* data buffer */

    /* Fragmentation */
    struct pan_fragment    pm_fragment;
    char                   pm_backup[MAX_FRAG_HDR_SIZE];
    int                    pm_hdr_size;

#if MAX_RELEASE_FUNCS > 0
    /* Release functions and arguments */
    pan_msg_release_f      pm_func[MAX_RELEASE_FUNCS];
    void                  *pm_arg [MAX_RELEASE_FUNCS];
    int                    pm_nr_rel;
#endif
};



extern void pan_sys_msg_start(void);
extern void pan_sys_msg_end(void);


#endif
