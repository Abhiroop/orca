/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_MESSAGE_
#define _SYS_GENERIC_MESSAGE_

#define MAX_RELEASE_FUNCS   2	/* max. no of pushed release functions */

#include "pan_sys.h"
#include "pan_buffer.h"
#include "pan_fragment.h"
#include "pan_sys_pool.h"

/* Panda message */
struct pan_msg{
    POOL_HEAD;			/* Pool management */

    /* Buffer management */
    pan_sys_buffer_p      *pm_buffer;/* Table of buffers */
    int                    pm_index; /* Index of current buffer */
    int                    pm_size; /* Size of table */
    int                    pm_used; /* Number of used buffers */

    /* Fragmentation */
    struct pan_fragment    pm_fragment;
    char                   pm_backup[MAX_TOTAL_HDR_SIZE];
    int                    pm_hdr_size;

    /* Release functions and arguments */
    pan_msg_release_f      pm_func[MAX_RELEASE_FUNCS];
    void                  *pm_arg [MAX_RELEASE_FUNCS];
    int                    pm_nr_rel;
};



extern void pan_sys_msg_start(void);
extern void pan_sys_msg_end(void);

extern void pan_sys_msg_send(pan_msg_p msg, pan_nsap_p nsap,
			     void *header, int len,
			     void (*frag_handler)(pan_fragment_p frag, 
						  void *header,
						  int data, int flags),
			     int data);
extern void pan_sys_msg_append(pan_msg_p msg, pan_sys_buffer_p buf, 
			       int index, int size);


#endif
