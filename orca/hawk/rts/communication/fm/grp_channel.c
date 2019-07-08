#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "grp_channel.h"
#include "precondition.h"
#include "sys_message.h"
#include "sys_mp_channel.h"
#include "demultiplex.h"
#include "pipe.h"
#include "map.h"
#include "misc.h"
#include "amoeba.h"
#include "group.h"
#include "thread.h"
#include "fm.h"
#include "assert.h"

#define MODULE_NAME "GRP_CHANNEL"
#define STACKSIZE 64000

struct grp_channel_s {
  int ch_number;
  int mid;
  set_p processors;
  protocol_p protocol;
  mp_channel_p mpc;
};

static  int me;
static  int group_size;
static  int proc_debug;
static  boolean initialized=FALSE;
static  map_p gch_map;

static  void grp_thread(char *c, int i);

/*=======================================================================*/
/* Initializes the module. */
/*=======================================================================*/ 
int 
init_grp_channel(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  if (initialized) return 0;
  init_mp_channel(me,group_size,proc_debug);
  init_demultiplex(me,group_size,proc_debug);
  init_map(me,group_size,proc_debug);
  init_pipe(me,group_size,proc_debug);
  gch_map=new_map();
  initialized=TRUE;
  return 0;
}
  

/*=======================================================================*/
/* Finishes the module. Sends a group message that will force all grp
   threads to return. */
/*=======================================================================*/ 
int 
finish_grp_channel() { 
  precondition(initialized);

  initialized=FALSE;
  free_map(gch_map);
  return 0;
}


/*=======================================================================*/
/* Creates a new grp_channel. It registers a receive function. All
   such functions must be register by all members in the same
   order. */
/*=======================================================================*/
grp_channel_p 
new_grp_channel(set_p processors) {
  grp_channel_p gch;
  int r;

  precondition(initialized);

  gch=(grp_channel_p )malloc(sizeof(grp_channel_t));
  sys_error(gch==NULL);
  gch->ch_number=insert_item(gch_map,(item_p)gch);
  sys_error(gch->ch_number<0);
  gch->processors=duplicate_set(processors);
  sys_error(gch->processors==NULL);
  gch->protocol=new_protocol();
  assert(gch->protocol!=NULL);
  gch->mpc=new_mp_channel(processors);
  sys_error(gch->mpc<0);
  r=thread_newthread(grp_thread,STACKSIZE,(char *)gch, sizeof(grp_channel_p));
  sys_error(r<0);
  return gch;
}

/*=======================================================================*/
/* Frees a grp channel. All channel must be freed in the same order
   by all members. */
/*=======================================================================*/
int 
free_grp_channel(grp_channel_p gch) {
  precondition(initialized);
  precondition(gch!=NULL);

  free_mp_channel(gch->mpc);
  free_set(gch->processors);
  /* some more stuff to be done here. */
  free(gch);
  return 0;
}

/*=======================================================================*/
/* Sends a message to all members of the group. Sets the h_extra field
   to the channel number (or receive function number). */
/*=======================================================================*/ 
int 
grp_channel_send(grp_channel_p gch, message_p message) { 
  struct FM_buffer *buffer;

  precondition(initialized);
  precondition(gch!=NULL);

  message->message_header->seqno=
    FM_next_seqno_and_credits(gch->mpc->mid,
			      sizeof(message_header_t)+
			      message->message_header->user_header_size+
			      message->message_header->user_data_size);
  message->message_header->protocol=gch->protocol->pid;
  message->message_header->protocol_set=1;
  mp_channel_multicast(gch->mpc,message);
  /* Create a new FM_buffer and copy the message in or the local
     grp_thread. */
  buffer=(struct FM_buffer *)pan_malloc(FM_BUF_ADMIN_SIZE+
			     sizeof(message_header_t)+
			     message->message_header->user_header_size+
			     message->message_header->user_data_size);
  assert(buffer!=NULL);
  assert(memcpy(buffer->fm_buf, message->message,
		sizeof(message_header_t)+
		message->message_header->user_header_size+
		message->message_header->user_data_size)!=NULL);
  pipe_append(gch->protocol->pipe,buffer);
  return 0;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

/*=======================================================================*/
/* Thread listening to group messages.  */
/*=======================================================================*/
void 
grp_thread(char *c, int i)
{
  grp_channel_p gch;
  message_handler_p handler;
  message_t message;
  struct FM_buffer *buffer;

  assert(initialized);
  gch=(grp_channel_p )c;
  assert(gch!=NULL);
  for (;;)
    { /* Block until message arrives. */
#ifdef __MAL_DEBUG
      mal_check_all();
#endif
      buffer=pipe_retrieve(gch->protocol->pipe);
      assert(buffer!=NULL);
      /* Call handler specified in message. */
      message_unmarshall(&message,buffer->fm_buf);
      handler=message_get_handler(&message);
      sys_error(handler==NULL) ;
      if ((*(handler))(&message)) {
	fprintf(stderr, "%d: handler returns 1, not supported by FM communication layer\n", me);
      }
#ifdef __MAL_DEBUG
      mal_check_all();
#endif
      FM_free_buf(buffer);
#ifdef __MAL_DEBUG
      mal_check_all();
#endif
    }
}
