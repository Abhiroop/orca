#ifndef __RTS_COMM_H__
#define __RTS_COMM_H__

#include "orca_types.h"

extern void rc_start(void);

extern void rc_end(void);

extern int rc_export(char *name, int no_ops, operation_p *ops);

extern void rc_sync_platforms(void);

extern void rc_rpc(int dest, int service, int op,
		   message_p request, message_p *reply);

extern void rc_tagged_reply(pan_upcall_t upcall, message_p reply);

extern void rc_untagged_reply(pan_upcall_t upcall, message_p reply);

extern void rc_mcast(int service, int op, message_p msg);

extern void rc_mcast_done(void);

#endif
