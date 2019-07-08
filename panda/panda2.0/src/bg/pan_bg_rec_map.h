/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_REC_MAP_H__
#define __BIGGRP_REC_MAP_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

extern void rec_start(void);
extern void rec_end(void);
extern void rec_handle(pan_fragment_p fragment, pan_bg_hdr_p header,
		       int flags);
extern void rec_register(void (*func)(pan_msg_p msg));

#endif
