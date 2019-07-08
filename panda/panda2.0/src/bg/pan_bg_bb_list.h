/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_BB_LIST_H__
#define __BIGGRP_BB_LIST_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

extern void bb_start(void);
extern void bb_end(void);
extern void bb_add(pan_fragment_p fragment);
extern int  bb_find(int pid, seqno_t index, pan_fragment_p *frag);
extern void bb_remove(int pid, seqno_t index);


#endif
