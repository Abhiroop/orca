/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Initialisation/termination of individual groups.
 */

#ifndef _GROUP_PAN_GROUP_H
#define _GROUP_PAN_GROUP_H

#include "pan_sys.h"

#include "pan_group.h"


pan_group_p pan_group_create(char *name, void (*rcve)(pan_msg_p msg), int gid,
			     int sequencer);

void        pan_group_clear(pan_group_p g);

void        pan_group_clear_data(pan_group_p g);

#endif
