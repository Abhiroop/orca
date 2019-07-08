/*************************************************************************/
/* Author: Saniya Ben Hassen. */
/* This module implements messages. */
/*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "sys_message.h"
#include "precondition.h"
#include "map.h"
#include "misc.h"
#include "assert.h"

#define MODULE_NAME "MESSAGE"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static map_p handler_map;

static int expand_message(message_p message, int max_data_size);

/*=======================================================================*/
int 
init_message(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  handler_map=new_map();
  assert(handler_map!=NULL);
  return 0;
}  

/*=======================================================================*/
int
finish_message() {

  if (--initialized) return 0;
  free_map(handler_map);
  return 0;
}

/*=======================================================================*/
/* Creates a new message structure. */
/*=======================================================================*/
message_p 
new_message(int header_size, int max_data_size) {
  message_p message;
  
  precondition_p(initialized);
  precondition_p(header_size>=0);
  precondition_p(max_data_size>=0);

  message=(message_p)malloc(sizeof(message_t));
  assert(message!=NULL);
  assert((header_size % 8)==0);
  assert((sizeof(message_header_t) % 8)==0);
  message->message=(void *)malloc(header_size+max_data_size+
			      sizeof(message_header_t));
  assert(message->message!=NULL);
  message->message_header=(message_header_p )message->message;
  message->user_header=message->message+sizeof(message_header_t);
  message->user_data=message->message+header_size+sizeof(message_header_t);
  message->message_header->user_header_size=header_size;
  message->message_header->user_data_size=0;
  message->max_user_data_size=max_data_size;
  return message;
}

/*=======================================================================*/
/* Frees a message structure. */
/*=======================================================================*/
int 
free_message(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  free(message->message);
  free(message);
  return 0;
}



/*=======================================================================*/
/* Print the RTS readable contents of a message. */
/*=======================================================================*/
int
print_message(message_p message) {
  precondition(message!=NULL);
  precondition(message->message!=NULL);
  
  fprintf(stderr, "MESSAGE:\n");
  fprintf(stderr, "\theader size: \t%d\n\t data size: \t%d\n\tmaxsize: \t%d\n",
	  message->message_header->user_header_size,
	  message->message_header->user_data_size,
	  message->max_user_data_size);
  return 0;
}

/*=======================================================================*/
/* Clears the data parts of a message structure. */
/*=======================================================================*/
int 
message_clear_data(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  message->message_header->user_data_size=0;
  return 0;
}

/*=======================================================================*/
/* Sets the header to the given values. */
/*=======================================================================*/
int 
message_set_header(message_p message, void *header, int size) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(size==message->message_header->user_header_size);
  
  memcpy(message->user_header,header,size);
  return 0;
}

/*=======================================================================*/
/* Sets the data part of a message to the given value. If the message
   cannot hold the entire message, it reallocates enough space for
   it. */
/*=======================================================================*/
int 
message_set_data(message_p message, void *buffer, int size) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(size>=0);
  
  if (message->max_user_data_size<size) expand_message(message,size);
  memcpy(message->user_data,buffer,size);
  message->message_header->user_data_size=size;
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
int
message_set_handler(message_p message, message_handler_p message_handler) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(message_handler!=NULL);
  
  message->message_header->message_handler=
    item_index(handler_map,(item_p)message_handler);
  return message->message_header->message_handler;
}
  
/*=======================================================================*/
/* Appends data to a message. Expands the message buffer if necessary. */
/*=======================================================================*/
int 
message_append_data(message_p message, void *buffer, int size) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(size>=0);
  precondition((buffer!=NULL)||(size==0));

  if (size==0) return 0;
  if (message->max_user_data_size<
      (message->message_header->user_data_size+size))
    expand_message(message,message->message_header->user_data_size+size);
  memcpy(message->user_data+message->message_header->user_data_size,
	 buffer,size);
  message->message_header->user_data_size+=size;
  return 0;
}


/*=======================================================================*/
/* Sets the data part of a message to the given value. Requires that
   the buffer size be the same as the user data size. */
/*=======================================================================*/
int 
message_combine_data(message_p message, void *buffer, int size, 
		     message_combine_function cf) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(size==message->message_header->user_data_size);
  
  (*cf)(message->user_data,buffer);
  return 0;
}

/*=======================================================================*/
/* Returns the user header size of a message. */
int
message_get_header_size(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  return message->message_header->user_header_size;
}

/*=======================================================================*/
/* Copies 'message' into 'destmessage'. */
int
message_duplicate(message_p destmessage, message_p message) {
  message_set_header(destmessage,message_get_header(message),message_get_header_size(message));
  message_set_data(destmessage,message_get_data(message),message_get_data_size(message));
  message_set_handler(destmessage,message_get_handler(message));
  return 0;
}
  
/*=======================================================================*/
/* Returns a pointer to the header of a message. */
/*=======================================================================*/
void *
message_get_header(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  return message->user_header;
}

/*=======================================================================*/
/* Returns the user data size of a message. */
/*=======================================================================*/
int
message_get_data_size(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  return message->message_header->user_data_size;
}

/*=======================================================================*/
/* Returns a pointer to the data part of a message. */
/*=======================================================================*/
void *
message_get_data(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  return message->user_data;
}

/*=======================================================================*/
/* Copies the data part of a message into a given buffer. The size of
   the buffer must be at least the size of the data part of the
   message. */
/*=======================================================================*/
int
message_copy_data(message_p message, void *buffer, int buffer_size) {
  precondition(initialized);
  precondition(message!=NULL);
  precondition(message->message_header->user_data_size<=buffer_size);
  
  memcpy(buffer,message->user_data,message->message_header->user_data_size);
  return 0;
}

/*=======================================================================*/
/* Returns a pointer to the handler of a message. */
/*=======================================================================*/
message_handler_p
message_get_handler(message_p message) {
  precondition(initialized);
  precondition(message!=NULL);

  return (message_handler_p)
    get_item(handler_map,message->message_header->message_handler);
}

/*=======================================================================*/
/* Registers a message handler. All handlers must be register in the
   same order on all processors. */
/*=======================================================================*/
int
message_register_handler(message_handler_p handler) {
  precondition(initialized);
  precondition(handler!=NULL);

  return insert_item(handler_map, (item_p)handler);
}

/*=======================================================================*/
/*=======================================================================*/
int
message_unregister_handler(message_handler_p handler) {
  precondition(initialized);
  precondition(handler!=NULL);

  return remove_item_p(handler_map, (item_p)handler);
}


/*************************************************************************/
/********************* System dependent functions ************************/
/*************************************************************************/

/*=======================================================================*/
/*=======================================================================*/
int 
message_unmarshall(message_p message, void *buffer) {
  precondition(initialized);
  precondition(message!=NULL);

  message->message=buffer;
  message->message_header=(message_header_p )buffer;
  message->user_header=message->message+sizeof(message_header_t);
  message->user_data=message->user_header
    +message->message_header->user_header_size;
  message->max_user_data_size=((int *)buffer)[2];
  return 0;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

/*=======================================================================*/
/* Expands a message to data part of a message. */
/*=======================================================================*/
static int 
expand_message(message_p message, int max_data_size) {
  void *new_message;

  precondition(initialized);
  precondition(message!=NULL);
  precondition(message->message_header!=NULL);
  precondition(max_data_size>=0);

#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  if (message->max_user_data_size>=max_data_size) return 0;
  new_message=malloc(message->message_header->user_header_size+
		     max_data_size+sizeof(message_header_t));
  assert(new_message!=NULL);
  memcpy(new_message,message->message,
	 sizeof(message_header_t)+
	 message->message_header->user_data_size+
	 message->message_header->user_header_size);
  free(message->message);
  message->message=new_message;
  message->message_header=(message_header_p)message->message;
  message->user_header=message->message+sizeof(message_header_t);
  message->user_data=message->user_header
    +message->message_header->user_header_size;
  message->max_user_data_size=max_data_size;
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  return 0;
}
