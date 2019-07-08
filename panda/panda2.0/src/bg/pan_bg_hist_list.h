/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_HIST_LIST_H__
#define __BIGGRP_HIST_LIST_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

extern void hist_start(void);
extern void hist_end(void);
extern int  hist_add(seqno_t seqno, pan_fragment_p fragment);
extern int  hist_find(seqno_t seqno, pan_fragment_p *fragment);
extern int  hist_seqno(int pid, seqno_t index, pan_fragment_p *fragment);
extern void hist_confirm(seqno_t seqno);
extern void hist_clear(void);

#endif
