/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_GLOBAL_H__
#define __BIGGRP_GLOBAL_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

#include <stdlib.h>
#include <assert.h>

extern pan_mutex_p    pan_bg_lock;
extern pan_pset_p     pan_bg_all;
extern seqno_t        pan_bg_rseqno;
extern seqno_t        pan_bg_seqno;
extern seqno_t        pan_bg_ackno;

#endif
