/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_SEND_MAP_H__
#define __BIGGRP_SEND_MAP_H__

extern void send_start(void);
extern void send_end(void);
extern void send_group(pan_msg_p message);
extern void send_signal(int src, int dest);

#endif
