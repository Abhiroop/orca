#ifndef _PAN_MESSAGE_
#define _PAN_MESSAGE_

#include "pan_sys_pool.h"

typedef struct pan_msg {
    POOL_HEAD;
    char *block;		/* a message is a single block */
    char *top;			/* a pointer just beyond the end of this msg */
    char *data;			/* pointer to the current data on top */
    int data_len;		/* length of (valid) "pushed" data */
} pan_msg_t;

extern void pan_sys_msg_start(void);
extern void pan_sys_msg_end(void);

#endif
