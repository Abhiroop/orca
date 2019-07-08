#include <stdlib.h>
#include <unistd.h>

#include "collection.h"
#include "message_handlers.h"
#include "coll_channel.h"
#include "synchronization.h"
#include "mp_channel.h"
#include "message.h"
#include "util.h"
#include "sleep.h"
#include "assert.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

static int request_for_data_handler(message_p message);
static int exit_exception_handler(message_p message);
static int data_handler(message_p message);
static int alive_handler(message_p message);

po_lock_p short_message_lock;
message_p short_message;

/*=======================================================================*/
/* Registers all message handlers. */
/*=======================================================================*/
int 
init_message_handler(int moi, int gsize, int pdebug) {
  if (initialized++) return 0;
  
  me=moi;
  proc_debug=pdebug;
  group_size=gsize;
  init_message(moi,gsize,pdebug);
  init_mp_channel(moi,gsize,pdebug);
  init_lock(moi,gsize,pdebug);
  short_message=new_message(sizeof(coll_header_t),0);
  message_register_handler(alive_handler);
  message_register_handler(data_handler);
  message_register_handler(exit_exception_handler);
  message_register_handler(request_for_data_handler);
  short_message_lock=new_lock();
  return 0;
}

int 
finish_message_handler(void) {
  if (--initialized) return 0;
  
  free_lock(short_message_lock);
  free_message(short_message);
  finish_lock();
  finish_mp_channel();
  finish_message();
  return 0;
}

/*=======================================================================*/
/* Here are defined several message handlers. They should be called
   when a message arrives, according to the type of message. The first
   one handles requests for data.  */
/*=======================================================================*/
static int 
request_for_data_handler(message_p message) {
  coll_channel_p ch;
  int sender;
  coll_header_p op;
  void *data;
  int size;

  if (!initialized) return 0;
  op=(coll_header_p )message_get_header(message);
  sender=op->sender;
  data=message_get_data(message);
  size=message_get_data_size(message);
  ch=channel_pointer(op->channel_number);
  if (ch==NULL) return 0;
  if (!(member(ch->coll->members,me))) return 0;
  if ((ch->op->num_received_from_children==(ch->coll->forest->fanout+1))) {
    lock(ch->lock);
    do_send_data(op->caller,ch);
    unlock(ch->lock);
  }
  else
    send_alive_message(ch,sender);
  return 0;
}

/*=======================================================================*/
/* When an exit exception is received, just exit. */
/*=======================================================================*/
static int 
exit_exception_handler(message_p message) {
  int sender;
  coll_header_p op;
  
  if (!initialized) return 0;
  op=(coll_header_p )message_get_header(message);
  sender=op->sender;
  if (sender!=me) {
    fprintf(stderr,"%d:\tReceived exit exception from %d\n", me, sender);
    exit(0);
  }
  return 0;
}
 
/*=======================================================================*/
/* Handles data received either from a child or from a root. */
/*=======================================================================*/

static int 
data_handler(message_p message) {
  coll_channel_p ch;
  int sender;
  coll_header_p op;

  if (!initialized) return 0;
  op=(coll_header_p )message_get_header(message);
  sender=op->sender;
  ch=channel_pointer(op->channel_number);
  if (ch==NULL) return 0;
  if (!(member(ch->coll->members,me))) return 0;
  lock(ch->lock);
  if (member(ch->coll->forest->ischild,sender)) {
    process_children_data(sender,ch,message);
  }
  else {
    assert(member(ch->coll->forest->isroot,sender));
    process_roots_data(sender,ch,message);
  }
  reset_timeout(ch->timers->coll_timer);
  unlock(ch->lock);
  return 0;
}
 
/*=======================================================================*/
/* Handles alive message. Just reset the timer corresponding to that
   operation to wait for a little more. */
/*=======================================================================*/
static int 
alive_handler(message_p message) {
  coll_channel_p ch;
  int sender;
  coll_header_p op;
  void *data;
  int size;

  if (!initialized) return 0;
  op=(coll_header_p )message_get_header(message);
  sender=op->sender;
  data=message_get_data(message);
  size=message_get_data_size(message);
  ch=channel_pointer(op->channel_number);
  if (ch==NULL) return 0;
  if (!member(ch->coll->members,me)) return 0;
  reset_timeout(ch->timers->coll_timer);  
  return 0;
}


/*=======================================================================*/
/* Sends a request for data, that was probably lost, to the specified
   process. */
/*=======================================================================*/
int 
send_request(coll_channel_p ch, int to) {
  coll_header_t op;

  op.sender=me;
  op.channel_number=ch->channel_number;
  lock(short_message_lock);
  message_set_header(short_message,&op,sizeof(coll_header_t));
  message_set_handler(short_message,request_for_data_handler);
  mp_channel_unicast(ch->fch,to,short_message);
  unlock(short_message_lock);
  return 0;
}

/*=======================================================================*/
/* Sends a message to a process to tell it it is alive. */
/*=======================================================================*/
int 
send_alive_message(coll_channel_p ch, int to ) {
  coll_header_t op;

  op.channel_number=ch->channel_number;
  lock(short_message_lock);
  message_set_header(short_message,&op,sizeof(coll_header_t));
  message_set_handler(short_message,alive_handler);
  mp_channel_unicast(ch->fch,to,short_message);
  unlock(short_message_lock);
  return 0;
}

/*=======================================================================*/
/* Sends an exit_exception to all processes. */

int 
send_exit_exception(coll_channel_p ch) {
  lock(short_message_lock);
  message_set_handler(short_message,exit_exception_handler);
  mp_channel_multicast(ch->fch,short_message);
  rts_sleep(1);
  unlock(short_message_lock);
  return 0;
}

/*=======================================================================*/
/* Calls the appropriate send function according to the operation
   code. */

int 
do_send_data(int caller, coll_channel_p ch) {
  coll_header_t op;

  op.sender=me;
  op.caller=caller;
  op.channel_number=ch->channel_number;
  op.rf=function_index(ch->op->rf);
  op.opcode = ch->op->coll_operation->opcode;
  op.ticket=ch->op->ticket;
  message_set_header(ch->buffers->smessage,&op,sizeof(coll_header_t));
  message_set_handler(ch->buffers->smessage,data_handler);
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  (*ch->op->coll_operation->sf)(caller,ch);
  return 0;
}

 
