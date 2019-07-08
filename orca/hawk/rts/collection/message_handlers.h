#ifndef __message_handler__
#define __message_handler__

#include "coll_channel.h"
#include "message.h"

int init_message_handler(int moi, int gsize, int pdebug);
int finish_message_handler(void);

int send_request(coll_channel_p ch, int to);
int send_alive_message(coll_channel_p ch, int to);
int send_exit_exception(coll_channel_p ch);
int do_send_data(int, coll_channel_p ch);

#endif
