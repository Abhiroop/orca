/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_ORDER_LIST_H__
#define __BIGGRP_ORDER_LIST_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

extern void order_start(void);
extern void order_end(void);
extern int  order_add(seqno_t seqno, pan_fragment_p frag);
extern int  order_get(seqno_t seqno, pan_fragment_p *frag);
extern int  order_size(void);

#endif
