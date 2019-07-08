#ifndef __message__
#define __message__

/*********************************************************************/
/*********************************************************************/
/* Message structure management. */
/*********************************************************************/
/*********************************************************************/

typedef struct message_s *message_p, message_t;
typedef int (*message_handler_p)(message_p );

/* Combines data parts of two messages. The result is set in 'dbuffer'
   and the primitive returns the size of the result. */
typedef void (*message_combine_function)(void *dbuffer, void *buffer);

int init_message(int moi, int gsize, int pdebug);
int finish_message(void);

/* =================================================================
   Primitives for message structure handling.
   ================================================================= */
message_p new_message(int header_size, int max_data_size);
int free_message(message_p message);

/* =================================================================
   Primitives for setting message contents.
   ================================================================= */
int message_set_header(message_p message, void *hdr, int size);
int message_set_data(message_p message, void *buffer, int size);
int message_set_handler(message_p, message_handler_p);
int message_clear_data(message_p message);
int message_append_data(message_p message, void *buffer, int size);
int message_duplicate(message_p destmessage, message_p message);
int message_combine_data(message_p message, void *buffer, int size,
			 message_combine_function rf);

/* =================================================================
   Primitives for getting message contents and message handlers.
   ================================================================= */
int message_get_header_size(message_p);
void *message_get_header(message_p);
int message_get_data_size(message_p);
void *message_get_data(message_p);
int message_copy_data(message_p, void *buffer, int buffer_size);
message_handler_p message_get_handler(message_p);

/* =================================================================
   Primitives for registering message handlers. All handlers must be
   register in the same order on all processors. 'get_message_handler'
   is used by the lower-level protocols to process incoming messages.
   ================================================================= */
int message_register_handler(message_handler_p mhandler);
int message_unregister_handler(message_handler_p mhandler);

#endif
