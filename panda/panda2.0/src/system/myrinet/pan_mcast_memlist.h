/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* A package for lists of mcast group members.
 * Implemented as array of int
 * Used by the purge watchdog to maintain its member admin
 */


#ifndef _GROUP_MEMLIST_H
#define _GROUP_MEMLIST_H


#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI static
#endif



#ifndef STATIC_CI

int  pan_mcast_mem_lst_n_behind(int *seen_seqno, int min_sq);

int  pan_mcast_mem_lst_min_seen(int *seen_seqno);

int *pan_mcast_mem_lst_create(void);

void pan_mcast_mem_lst_clear(int *seen_seqno);

#endif


#endif
