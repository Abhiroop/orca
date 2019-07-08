#include <stdlib.h>
#include <string.h>

#include "sys_mp_channel.h"
#include "sys_message.h"
#include "precondition.h"
#include "misc.h"
#include "map.h"
#include "amoeba.h"
#include "demultiplex.h"
#include "semaphore.h"
#include "assert.h"
#include "po_timer.h"

#define MODULE_NAME "MP_CHANNEL"

static int me;
static int group_size;
static int proc_debug;
static boolean initialized=FALSE;
static map_p ch_map;
static semaphore registered;
static message_p message;
static po_timer_p at_mp_send;
static po_timer_p at_mp_multicast;
static FM_mc_t current_mid; /* Indicates which mid is 
			       being currently registered. It
			       if used by the message handler to
			       register an mid. */

static int register_mid(message_p);
static int mid_registered(message_p);

/*=======================================================================*/
/* Initializes the module. Gets all flip addresses necessary to send
   and receive message. */
/*=======================================================================*/
int 
init_mp_channel(int moi, int gsize, int pdebug) {
  int r;
  
  if ((r=init_module())<=0) return r;
  if (initialized) return 0;
  init_map(me,group_size,pdebug);
  init_demultiplex(me,group_size,pdebug);
  init_po_timer(me,group_size,pdebug);
  ch_map=new_map();
  assert(ch_map!=NULL);
  at_mp_send=new_po_timer("mp_send");
  at_mp_multicast=new_po_timer("mp_multicast");
  sema_init(&registered,0);
  message=new_message(0,0);
  message_register_handler(register_mid);
  message_register_handler(mid_registered);
  initialized=TRUE;
  return 0;
}

/*=======================================================================*/
/* Finishes Flip. */
/*=======================================================================*/
int 
finish_mp_channel() {

  if (!initialized) return -1;
  initialized=FALSE;
  free_map(ch_map);
  return 0;
}


/*=======================================================================*/
/* Creates a new channel on which only members specified in "group"
   can send and receive message. For each message received on this
   channel, the corresponding handler will be called by the rawflip
   upcall. All channels must be registered in the same order on all
   processors. */
/*=======================================================================*/
mp_channel_p 
new_mp_channel(set_p group) {
  int *members;
  mp_channel_p ch;

  precondition(initialized);
  ch=(mp_channel_p )malloc(sizeof(mp_channel_t));
  assert(ch!=NULL);
  /* This prevents messages to be processed before the channel is
     entirely initialized. */
  ch->ch_number=insert_item(ch_map,ch);
  assert(ch->ch_number>=0);
  if (group==NULL) {
    /* The default for the processor set is the entire set of
       processors. */
    ch->group=new_set(group_size);
    full_set(ch->group);
  }
  else
    ch->group=duplicate_set(group);
  members=list_of_members(group);
  if (me==members[0]) {
    /* Only one member of the group must register the group. */
    ch->mid = FM_mc_create(members,group->num_members);
    assert(ch->mid>=0);
    /* Send mid to all members. */
    message->message_header->protocol=UPCALL;
    message_set_handler(message,register_mid);
    message_set_data(message,&(ch->mid),sizeof(int));
    FM_broadcast(receive_buffer,message->message,
		 sizeof(message_header_t)+sizeof(int));
  }
  free(members);
  /* Wait for all members to ackowledge. */
  sema_mdown(&registered,group_size-1);
  sema_mup(&registered,group_size-1);
  if (me!=0)
    ch->mid=current_mid;
  return ch;
}
  

/*=======================================================================*/
/* Unregisters FLIP addresses corresponding to the channel and cleans
   up all data structures allocated for the channel. */
/*=======================================================================*/
int 
free_mp_channel(mp_channel_p ch) {
  precondition(initialized);
  precondition(ch!=NULL);
  
  free_set(ch->group);
  if (me==0) FM_mc_delete(ch->mid);
  free(ch);
  return 0;
}
 
/*=======================================================================*/
/* Sends a message to a member of a channel. */
/*=======================================================================*/
int 
mp_channel_unicast(mp_channel_p ch, int receiver, message_p message) {
  int buffer_size;

  precondition(initialized);
  precondition(ch!=NULL);
  precondition(is_member(ch->group,me));
  precondition(is_member(ch->group,receiver));
  precondition(message!=NULL);
  precondition(message->message_header!=NULL);

  if (!(message->message_header->protocol_set))
      message->message_header->protocol=UPCALL;
  buffer_size=message->message_header->user_data_size+
    message->message_header->user_header_size+sizeof(message_header_t);
  FM_send_buf(receiver, receive_buffer, message->message, buffer_size);
  message->message_header->protocol_set=0;
  return 0;
}
 
 
/*=======================================================================*/
/* Mulitcasts a message to all nodes. */
/*=======================================================================*/
int 
mp_channel_multicast(mp_channel_p ch, message_p message) {
  int buffer_size;
 
  precondition(initialized);
  precondition(ch!=NULL);
  precondition(is_member(ch->group,me));
  precondition(message!=NULL);
  precondition(message->message_header!=NULL);

  if (!(message->message_header->protocol_set))
      message->message_header->protocol=UPCALL;
  buffer_size=message->message_header->user_data_size+
    message->message_header->user_header_size+sizeof(message_header_t);
  FM_mc_send(ch->mid, receive_buffer, message->message, buffer_size);
  message->message_header->protocol_set=0;
  return 0;
}
 
/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/
 
/*=======================================================================*/
/* Signals that a multicast id has been registered. */
/*=======================================================================*/
int 
mid_registered(message_p message) {
  assert(initialized);
  assert(message!=NULL);
  sema_up(&registered);
  return 0;
}

/*=======================================================================*/
/* Registers a multicast id. */
/*=======================================================================*/
int 
register_mid(message_p message) {
  void *buffer;
  message_p m;

  assert(initialized);
  assert(message!=NULL);
  buffer=message_get_data(message);
  current_mid=*((int *)buffer);
  m=new_message(0,0);
  m->message_header->protocol=UPCALL;
  message_set_handler(m,mid_registered);
  FM_broadcast(receive_buffer,m->message,sizeof(message_header_t));
  sema_up(&registered);
  return 0;
}
