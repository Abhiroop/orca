/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_COMM_H__
#define __RTS_COMM_H__

#include "orca_types.h"

extern int rc_trailer;

void *rc_msg_create(int *size, int *len);
void *rc_msg_push(void **m, int *size, int *len, int s);
void *rc_msg_pop(void *m, int *len, int s);

extern void rc_start(void);

extern void rc_end(void);

extern int rc_export(oper_p handler);

extern void rc_sync_platforms(void);

extern void rc_rpc(int dest, int handle, 
		   void *request, int req_len,
		   void **reply, int *rep_size, int *rep_len);

extern void rc_tagged_reply(int upcall, void *reply, int len);

extern void rc_untagged_reply(int upcall, void *reply, int len);

extern void rc_mcast(int handle, void *msg, int len);

extern void rc_mcast_done(void);


#define RC_REQ_SIZE	ps_align(sizeof(unsigned long) + sizeof(int))
#define RC_REPL_SIZE	ps_align(sizeof(int))
#define RC_MCAST_SIZE	ps_align(sizeof(int))

#endif
