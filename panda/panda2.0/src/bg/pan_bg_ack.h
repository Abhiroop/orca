/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_ACK_H__
#define __BIGGRP_ACK_H__

#include "pan_bg_group.h"

extern void    ack_start(void);
extern void    ack_end(void);
extern seqno_t ack_check(int pid, seqno_t ackno);
extern void    ack_explicit(seqno_t seqno);
extern void    ack_flush(void);
extern int     ack_reply(int pid, seqno_t ackno);



#endif
