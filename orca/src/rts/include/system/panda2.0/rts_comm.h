/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_COMM_H__
#define __RTS_COMM_H__

#include "orca_types.h"

extern void rc_start(void);

extern void rc_end(void);

extern int rc_export(oper_p handler);

extern void rc_sync_platforms(void);

extern void rc_rpc(int dest, int handle, pan_msg_p request, pan_msg_p *reply);

extern void rc_tagged_reply(pan_upcall_p upcall, pan_msg_p reply);

extern void rc_untagged_reply(pan_upcall_p upcall, pan_msg_p reply);

extern void rc_mcast(int handle, pan_msg_p msg);

extern void rc_mcast_done(void);

#endif
