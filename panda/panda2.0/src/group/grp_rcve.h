/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 *  This module implements the receiving daemons of group communication,
 *  as far as receiving functionality is common between members and sequencer.
 */


#ifndef _GROUP_GRP_RCVE_H_
#define _GROUP_GRP_RCVE_H_


#include "pan_sys.h"

#include "pan_group.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

void           pan_grp_init_rcve(pan_group_p g, void (*rcve)(pan_msg_p msg));
void           pan_grp_clear_rcve(pan_group_p g);

void           pan_grp_rcve_start(void);
void           pan_grp_rcve_end(void);

#endif

#endif
