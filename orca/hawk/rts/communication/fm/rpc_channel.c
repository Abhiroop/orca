#include <stdlib.h>
#include <stdio.h>
#include "rpc_channel.h"
#include "map.h"
#include "mp_channel.h"
#include "precondition.h"
#include "condition.h"
#include "amoeba.h"
#include "thread.h"
#include "assert.h"

#define MODULE_NAME "RPC_CHANNEL"
#define STACKSIZE  32000

static int me;
static int group_size;
static int proc_debug;
static boolean initialized=FALSE;
static map_p rpc_channel_map;
static map_p rpc_handlers_map;

typedef struct trailer_s trailer_t, *trailer_p;

struct rpc_channel_s {
  int ch_number;
  int ticket;             /* Tickets used to make sure messages are
		             not processed more than once. */
  int server_ticket;
  int client_ticket;
  message_p request_message;
  message_p reply_message;
  condition_p channel_free;
  po_time_t timeout; /* Timeout value in milliseconds. */
  set_p processors; /* Members connected by the channel. */
  mp_channel_p mpch; /* Message passing channel used to connect members. */
  message_p message; /* Used by server thread to process incoming messages. */
  condition_p new_request; 
};

struct trailer_s {
  int rch_id;
  int to;
  int from;
  int ticket;
  int handler;
};

static void __rpc_server_handler(message_p);
static void rpc_server_thread(char *c, int size);
static void rpc_server_handler(message_p);
static void rpc_client_handler(message_p);

/*==============================================================*/
/* Initializes the module. */
/*==============================================================*/
int 
init_rpc_channel(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  init_mp_channel(me, group_size, proc_debug);
  init_message(me, group_size, proc_debug);
  init_map(me, group_size, proc_debug);
  init_condition(me, group_size, proc_debug);
  rpc_channel_map=new_map();
  message_register_handler(rpc_server_handler);
  message_register_handler(rpc_client_handler);
  rpc_handlers_map=new_map();
  initialized=TRUE;
  return 0;
}

/*==============================================================*/
/* Ends the module. */
/*==============================================================*/
int 
finish_rpc_channel() {

  precondition(initialized);

  free_map(rpc_channel_map);
  free_map(rpc_handlers_map);
  initialized=FALSE;
  return 0;
}

/*==============================================================*/
/* Creates a new channel between a set of processors. */
/*==============================================================*/
rpc_channel_p 
new_rpc_channel(set_p processors) {
  rpc_channel_p rch;
  int r;

  precondition(initialized);

  rch=(rpc_channel_p )malloc(sizeof(rpc_channel_t));
  assert(rch!=NULL);
  if (processors==NULL) {
    rch->processors=new_set(group_size);
    full_set(rch->processors);
  }
  else 
    rch->processors=duplicate_set(processors);
  assert(rch->processors!=NULL);
  rch->ticket=0;
  rch->server_ticket=1;
  rch->client_ticket=1;
  rch->request_message=new_message(0,0);
  rch->reply_message=new_message(0,0);
  rch->channel_free=new_condition();
  rch->mpch=new_mp_channel(rch->processors);
  assert(rch->mpch!=NULL);
  rch->timeout=1000000; /* Default timeout is 1000000 microseconds. */
  rch->message=new_message(0,0);
  rch->new_request=new_condition();
  await_condition(rch->new_request);
  rch->ch_number=insert_item(rpc_channel_map, rch);
  assert(rch->ch_number>=0);
  r=thread_newthread(rpc_server_thread,STACKSIZE,
		     (char *)rch, sizeof(rpc_channel_p));
  assert(r>=0);
  return rch;
}

/*==============================================================*/
/* Frees an rpc channel. */
/*==============================================================*/
int 
free_rpc_channel(rpc_channel_p rch) {
  
  precondition(initialized);
  precondition(rch!=NULL);
  
  remove_item(rpc_channel_map,rch->ch_number);
  free_set(rch->processors);
  free_message(rch->request_message);
  free_message(rch->reply_message);
  free_condition(rch->channel_free);
  free_mp_channel(rch->mpch);
  free_message(rch->message);
  signal_condition(rch->new_request);
  return 0;
}

/*==============================================================*/
/* Sets the timeout for the next rpc issued by this processor. */
/*==============================================================*/
int 
set_timeout(rpc_channel_p rch, po_time_t time, unit_t unit) {

  precondition(initialized);
  precondition(rch!=NULL);

  switch (unit) {
  case PO_MICROSEC: 
    rch->timeout=time;
    break;
  case PO_MILLISEC:
    rch->timeout=time*1000.0;
    break;
  case PO_SEC:
    rch->timeout=time*1000000.0;
    break;
  default:
    return -1;
  }
  return 0;
}

/*==============================================================*/
/* Sends a message to a processor and returns a ticket. Upon arrival
   at the server's side, the message is processed using the handler
   specified in the header of the message. This handler must be
   registered and set before calling rpc_request. The server must
   reply to the message using an rpc_reply call. */
/*==============================================================*/
int
rpc_request(rpc_channel_p rch, int to, message_p message) {  
  trailer_t trailer;

  precondition(initialized);
  precondition(rch!=NULL);
  precondition((to>=0) && (to<group_size));
  precondition(message!=NULL);
  
  await_condition(rch->channel_free);
  /* Set rpc specific information in a trailer and replace handler by
     rpc_server_hanlder pointer. */
  rch->ticket++;
  trailer.rch_id=rch->ch_number;
  trailer.ticket=rch->ticket;
  trailer.from=me;
  trailer.to=to;
  trailer.handler=item_index(rpc_handlers_map,
			     (item_p )message_get_handler(message));
  assert(trailer.handler>=0);
  message_append_data(message,&trailer,sizeof(trailer_t));
  message_set_handler(message,rpc_server_handler);
  
  /* Save request message. */
  if (message_get_header_size(rch->request_message)!=
      message_get_header_size(message)) {
    free_message(rch->request_message);
    rch->request_message=new_message(message_get_header_size(message),
				     message_get_data_size(message));
  }
  message_set_header(rch->request_message,message_get_header(message),
		     message_get_header_size(message));
  message_set_data(rch->request_message,message_get_data(message),
		   message_get_data_size(message));
  message_set_handler(rch->request_message, rpc_server_handler);

  /* Send message. */
  mp_channel_unicast(rch->mpch,to,message);
  message_set_handler(message, get_item(rpc_handlers_map,trailer.handler));
  
  return rch->ticket;
}

/*==============================================================*/
/* Replies to an rpc request. The server must provide the message sent
   by the client in smessage. The rmessage is the reply message and
   will be processed at the client's side by the handler specified in
   the message. The handler must be set before calling rpc_reply. */
/*==============================================================*/
int 
rpc_reply(rpc_channel_p rch, message_p smessage, message_p rmessage) {
  trailer_p strailer;
  trailer_t trailer;

  /* Set trailer in the message. */
  strailer=(trailer_p )(message_get_data(smessage)+
			message_get_data_size(smessage)-
			sizeof(trailer_t));
  trailer.from=me;
  trailer.to=strailer->from;
  trailer.rch_id=strailer->rch_id;
  trailer.ticket=strailer->ticket;
  trailer.handler=item_index(rpc_handlers_map,
			     (item_p)message_get_handler(rmessage));
  assert(trailer.handler>=0);
  message_append_data(rmessage,&trailer,sizeof(trailer_t));
  message_set_handler(rmessage,rpc_client_handler);

  /* Save reply message. */
  if (message_get_header_size(rch->reply_message)!=
      message_get_header_size(rmessage)) {
    free_message(rch->reply_message);
    rch->reply_message=new_message(message_get_header_size(rmessage),
				   message_get_data_size(rmessage));
  }
  message_set_header(rch->reply_message,message_get_header(rmessage),
		     message_get_header_size(rmessage));
  message_set_data(rch->reply_message,message_get_data(rmessage),
		   message_get_data_size(rmessage));
  message_set_handler(rch->reply_message, rpc_client_handler);

  /* Send message. */
  mp_channel_unicast(rch->mpch,trailer.to,rmessage);
  return 0;
}

/*==============================================================*/
/* Waits for a reply. If the reply does not arrive within 'timeout'
   time, sends the request again. Does the timeout twice only. */
/*==============================================================*/
int
rpc_wait_reply(rpc_channel_p rch) {
  trailer_p trailer;

  /* Timeout reply. If it does not arrive within the required time,
     resend the request again. */
  if (tawait_condition(rch->channel_free,rch->timeout,PO_MICROSEC)<0) {
    trailer=(trailer_p )(message_get_data(rch->request_message)+
			 message_get_data_size(rch->request_message)-
			 sizeof(trailer_t));
    mp_channel_unicast(rch->mpch,trailer->to,rch->request_message);
    /* Timeout this rpc again. */
    if (tawait_condition(rch->channel_free,rch->timeout,PO_MICROSEC)<0)
      return -1;
  }
  signal_condition(rch->channel_free);
  return 0;
}

/*==============================================================*/
/* Registers a handler. */
/*==============================================================*/
int
rpc_register_handler(message_handler_p handler) {
  precondition(initialized);
  precondition(handler!=NULL);
  
  return insert_item(rpc_handlers_map, (item_p)handler);
}

/*==============================================================*/
/* Unregisters a handler. */
/*==============================================================*/
int 
rpc_unregister_handler(message_handler_p handler) {
  precondition(initialized);
  precondition(handler!=NULL);
  
  return remove_item_p(rpc_handlers_map, (item_p)handler);
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

void 
rpc_server_handler(message_p message) {
  trailer_p trailer;
  rpc_channel_p rch;

  trailer=(trailer_p )(message_get_data(message)+
		       message_get_data_size(message)-
		       sizeof(trailer_t));
  rch=get_item(rpc_channel_map,trailer->rch_id);
  assert(rch!=NULL);

  /* Wake up server thread. */
  if (message_get_header_size(message)!=
      message_get_header_size(rch->message)) {
    free_message(rch->message);
    rch->message=new_message(message_get_header_size(message),
				   message_get_data_size(message));
  }
  message_set_header(rch->message,message_get_header(message),
		     message_get_header_size(message));
  message_set_data(rch->message,message_get_data(message),
		   message_get_data_size(message));
  message_set_handler(rch->message, rpc_server_handler);

  signal_condition(rch->new_request);
  return;
}

void
rpc_server_thread(char *c, int size) {
  rpc_channel_p rch;

  rch=(rpc_channel_p)c;
  for (;;) {
    await_condition(rch->new_request);
    if (rch->message==NULL) {
      free_condition(rch->new_request);
      free(rch);
      thread_exit();
    }
    __rpc_server_handler(rch->message);
  }
}

void 
__rpc_server_handler(message_p message) {
  trailer_p trailer;
  rpc_channel_p rch;
  message_handler_p handler;

  /* Make sure request has not yet been processed. */
  trailer=(trailer_p )(message_get_data(message)+
		       message_get_data_size(message)-
		       sizeof(trailer_t));
  rch=get_item(rpc_channel_map,trailer->rch_id);
  assert(rch!=NULL);

  /* If already processed, send reply again without calling user
     server handler. */
  if (trailer->ticket<rch->server_ticket) {
    trailer=(trailer_p )(message_get_data(rch->reply_message)+
			 message_get_data_size(rch->reply_message)-
			 sizeof(trailer_t));
    mp_channel_unicast(rch->mpch, trailer->to, rch->reply_message);
    return;
  }

  /* If not already processed, call the user server handler. */
  /*  assert(rch->server_ticket==trailer->ticket); */
  rch->server_ticket++;
  handler=get_item(rpc_handlers_map,trailer->handler);
  (*handler)(message);
  return;
}

void 
rpc_client_handler(message_p message) {
  trailer_p trailer;
  rpc_channel_p rch;
  message_handler_p handler;

  /* Make sure reply has not yet been processed. */
  trailer=(trailer_p )(message_get_data(message)+
		       message_get_data_size(message)-
		       sizeof(trailer_t));
  rch=get_item(rpc_channel_map,trailer->rch_id);
  assert(rch!=NULL);

  /* If already processed, do not call user client handler. */
  if (trailer->ticket<rch->client_ticket) return;

  /* If not already processed, call the user client handler. */
  assert(rch->client_ticket==trailer->ticket);
  rch->client_ticket++;
  handler=get_item(rpc_handlers_map,trailer->handler);
  (*handler)(message);

  /* Signal the end of RPC. */
  signal_condition(rch->channel_free);
  return;
}


