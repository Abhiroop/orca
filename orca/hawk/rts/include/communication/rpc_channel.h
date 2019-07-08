#ifndef __rpc_channel__
#define __rpc_channel__

#include "set.h"
#include "po_timer.h"
#include "message.h"

typedef struct rpc_channel_s *rpc_channel_p, rpc_channel_t;

int init_rpc_channel(int me, int group_size, int proc_debug);
int finish_rpc_channel(void);

rpc_channel_p new_rpc_channel(set_p processors);
int free_rpc_channel(rpc_channel_p rch);

int rpc_set_timeout(rpc_channel_p rch, po_time_t time, unit_t unit);
int rpc_request(rpc_channel_p rch, int to, message_p message);
int rpc_reply(rpc_channel_p rch, message_p smessage, message_p rmessage);
int rpc_wait_reply(rpc_channel_p rch);

int rpc_register_handler(message_handler_p handler);
int rpc_unregister_handler(message_handler_p handler);

#endif
