/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_COMM_H__
#define __RTS_COMM_H__

#include "orca_types.h"


typedef struct PANDA_MSG rc_msg_t, *rc_msg_p;

struct PANDA_MSG {
    pan_iovec_p	iov;
    int		iov_size;
    void       *proto;
    int		proto_size;
    void	(*clearfunc)(rc_msg_p);
/* For answers to RPCs, the clearfunc must cleanup the arguments as well,
   because the answer is sent asynchronous.
*/
    void	**argv;		/* argument vector */
    op_dscr	*op;		/* operation descriptor */

    rc_msg_p	next;
};

extern int     rts_mcast_proto_start;
extern int     rts_rpc_proto_start;
extern int     rts_reply_proto_start;
extern int     rts_mcast_proto_top;
extern int     rts_rpc_proto_top;
extern int     rts_reply_proto_top;


rc_msg_p rc_msg_create(void);
void  rc_msg_clear(rc_msg_p msg);

void *rc_proto_create(void);
void rc_proto_clear(void *proto);

extern void rc_start(void);

extern void rc_end(void);

extern int rc_export(oper_p handler);

extern void rc_sync_platforms(void);

extern void rc_rpc(int dest, int handle, 
		   pan_iovec_p iov, int iov_size,
		   void *proto, int proto_size,
		   pan_msg_p *reply, void **reply_proto);

extern void rc_tagged_reply(int upcall, rc_msg_p msg);

extern void rc_untagged_reply(int upcall, rc_msg_p msg);

extern void rc_mcast(int handle, pan_iovec_p iov, int iov_size, void *proto,
		     int proto_size);

extern void rc_mcast_done(void);

#endif
