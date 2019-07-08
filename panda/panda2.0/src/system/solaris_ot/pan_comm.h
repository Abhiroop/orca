/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_COMM_
#define _SYS_GENERIC_COMM_

#include "pan_nsap.h"

#define BCAST_COMM_HDR_SIZE	0
#define UCAST_COMM_HDR_SIZE	0

#define MAX_COMM_HDR_SIZE	(BCAST_COMM_HDR_SIZE > UCAST_COMM_HDR_SIZE ? \
				 BCAST_COMM_HDR_SIZE : UCAST_COMM_HDR_SIZE)

extern void pan_sys_comm_start(void);
extern void pan_sys_comm_wakeup(void);
extern void pan_sys_comm_end(void);
extern void pan_sys_comm_send(pan_nsap_p nsap, char *data, int size);

extern int sys_sprint_comm_stat_size(void);
extern void sys_sprint_comm_stats(char *buf);

#endif

