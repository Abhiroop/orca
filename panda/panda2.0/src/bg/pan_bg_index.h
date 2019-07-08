/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_INDEX_H__
#define __BIGGRP_INDEX_H__

#include "pan_bg_group.h"

extern void    index_start(void);
extern void    index_end(void);
extern seqno_t index_get(void);
extern int     index_check(seqno_t index, int pid);
extern void    index_set(seqno_t index, int pid);

#define INDEX_OVERRUN   1
#define INDEX_DUPLICATE 2
#define INDEX_OK        3


#endif
