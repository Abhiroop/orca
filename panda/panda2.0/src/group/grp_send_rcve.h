/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#ifndef __PAN_GRP_SEND_RCVE_H__
#define __PAN_GRP_SEND_RCVE_H__

#include "pan_sys.h"

#include "pan_group.h"



void pan_grp_init_send_rcve(pan_group_p g, void (*rcve)(pan_msg_p msg));
void pan_grp_clear_send_rcve(pan_group_p g);

void pan_grp_send_rcve_start(void);
void pan_grp_send_rcve_end(void);


#endif
