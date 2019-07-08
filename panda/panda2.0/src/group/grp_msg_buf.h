/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Circular message buffer implementation.
 *
 * The implementation increases the size to a power of two for cheap MOD
 * operations.
 */


#ifndef _GROUP_PAN_MSG_BUF_H
#define _GROUP_PAN_MSG_BUF_H

#include "pan_sys.h"

#include "grp_types.h"



typedef struct MSG_BUF_T msg_buf_t, *msg_buf_p;

struct MSG_BUF_T {
    pan_msg_p      *buf;			/* the buffer */
    int             last;			/* next to be read */
    int             next;			/* next to be written */
    int             size;			/* size */
};


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#define BUFMOD(msg_buf, idx)	((idx) & ((msg_buf)->size - 1))

#define BUF(msg_buf, idx)	((msg_buf)->buf[BUFMOD(msg_buf, idx)])


#ifndef STATIC_CI

pan_msg_p  buf_get_no(msg_buf_p buf, int idx);
boolean    buf_put_no(msg_buf_p buf, pan_msg_p msg, int idx);

boolean    buf_no_acceptable(msg_buf_p buf, int idx);

void       pan_msg_buf_init(msg_buf_p msg_buf, int size);
int        pan_msg_buf_clear(msg_buf_p msg_buf);

#endif


#endif
