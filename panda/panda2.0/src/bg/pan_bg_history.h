/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_HISTORY_H__
#define __BIGGRP_HISTORY_H__

#include "pan_bg_group.h"

extern void history_start(void);
extern void history_end(void);
extern void pan_bg_history_tick(int finish);

#endif
