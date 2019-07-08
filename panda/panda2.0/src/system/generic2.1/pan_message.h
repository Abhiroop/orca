/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_MESSAGE_
#define _SYS_GENERIC_MESSAGE_

#include "pan_sys.h"
#include "pan_fragment.h"
#include "pan_sys_pool.h"

/* Panda message */
struct pan_msg{
    POOL_HEAD;			/* Pool management */

    /* Data buffer */
    char *pm_buffer;
    int   pm_size;
    int   pm_pos;

    /* Fragmentation */
    struct pan_fragment pm_fragment;
#ifndef SINGLE_FRAGMENT
    char                pm_backup[MAX_TOTAL_HDR_SIZE];
#endif
    int                 pm_hdr_size;
};



extern void pan_sys_msg_start(void);
extern void pan_sys_msg_end(void);

/* New extensions to the system interface */
extern void *pan_msg_append(pan_msg_p msg, int length);
extern void *pan_msg_resize(pan_msg_p msg, int old_length, int new_length);
extern void * pan_msg_data(pan_msg_p msg, int *offset, int length, int align);

#endif
