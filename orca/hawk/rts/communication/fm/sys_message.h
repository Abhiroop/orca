#ifndef __sys_message__
#define __sys_message__

#include "message.h"

/* System dependent interface. */

typedef struct message_header_s message_header_t, *message_header_p;

struct message_header_s {
  int message_handler;               /* Number of the handler that
					must be executed upon
					reception of the message. */
  int user_header_size;              
  int user_data_size;
  int protocol;                      /* Used only for fast messages. */
  int seqno;                         /* Used only for fast messages. */
  int protocol_set;
};

struct message_s {
  void *message;                     /* Points to the complete message. 
					It's a consecutive chunck of 
					bytes. */
  message_header_p message_header;   /* Points to the message header
					within the message. */
  void *user_header;                 /* Points to the user header
					within the message. */
  void *user_data;                   /* Points to the user data
					within the message. */
  int max_user_data_size;
};

int message_unmarshall(message_p, void *buffer);
#endif
