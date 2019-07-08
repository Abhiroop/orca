#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "collection.h"
#include "coll_channel.h"
#include "map.h"
#include "forest.h"
#include "set.h"
#include "lock.h"
#include "timeout.h"
#include "mp_channel.h"
#include "message.h"
#include "message_handlers.h"
#include "assert.h"
#include "sleep.h"

#define MIN_TIMEOUT 5000
#ifdef RELIABLE_COMM
#define INTERV_TIMEOUT 500000
#define MAX_TIMEOUT 10000000
#else
#define INTERV_TIMEOUT 5000
#define MAX_TIMEOUT 40000
#endif

/* Static variables */
static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static map_p coll_channel_map;

static coll_buffers_p new_coll_buffers(void);
static int free_coll_buffers(coll_buffers_p );
static coll_opdescr_p new_coll_opdescr(void);
static int free_coll_opdescr(coll_opdescr_p );
static coll_timers_p new_coll_timers(forest_p forest);
static int free_coll_timers(coll_timers_p );
static int reset_opdescr(coll_channel_p ch, int caller, int ticket, 
			 reduction_function_p rf, coll_operation_p opcode);
static int reset_buffers(coll_channel_p ch, int caller, char *sbuffer, int ssize, char *rbuffer, int rsize);
static int check_channel(coll_channel_p ch);

/**********************************************************************/
/************************ Global functions ****************************/
/**********************************************************************/
int 
init_coll_channel(int moi, int gsize, int pdebug) {
  
  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_mp_channel(moi,gsize,pdebug);
  init_lock(moi,gsize,pdebug);
  init_message(moi,gsize,pdebug);
  init_map(moi,gsize,pdebug);
  coll_channel_map=new_map();
  assert(coll_channel_map!=NULL);
  return 0;
}

int 
finish_coll_channel(void) {
  
  if (--initialized) return 0;
  /* free coll_channel_map ??? */
  finish_map();
  finish_message();
  finish_lock();
  finish_mp_channel();
  return 0;
}

coll_channel_p
new_coll_channel(collection_p coll) {
  coll_channel_p ch;

  ch=(coll_channel_p )malloc(sizeof(coll_channel_t));
  ch->coll=coll;
  ch->ticket=-1;
  ch->lock=new_lock();
  ch->fch=new_mp_channel(coll->members);
  ch->op=new_coll_opdescr();
  assert(ch->op!=NULL);
  ch->buffers=new_coll_buffers();
  assert(ch->buffers!=NULL);
  ch->timers=new_coll_timers(coll->forest);
  ch->channel_number=insert_item(coll_channel_map,(item_p)ch);
  assert(ch->channel_number>=0);
  return ch; 
}

int
free_coll_channel(coll_channel_p ch) {
  free_mp_channel(ch->fch);
  free_coll_opdescr(ch->op);
  free_coll_buffers(ch->buffers);
  free_coll_timers(ch->timers);
  remove_item_p(coll_channel_map,ch);
  free_lock(ch->lock);
  free(ch);
  return 0;
}

static coll_opdescr_p
new_coll_opdescr(void) {
  coll_opdescr_p op;

  op=(coll_opdescr_p )malloc(sizeof(coll_opdescr_t));
  op->caller=-1;
  op->received=new_set(group_size);
  op->num_received_from_children=0;
  op->num_received_from_roots=0;
  op->coll_operation=NULL;
  op->rf=NULL;
  op->ticket=-1;
  op->rbuffer_set=0;
  op->smessage_set=0;
  return op;
}

static int
free_coll_opdescr(coll_opdescr_p op) {
  free_set(op->received);
  free(op);
  return 0;
}

static coll_buffers_p
new_coll_buffers(void) {
  coll_buffers_p buf;
  int i;

  buf=(coll_buffers_p )malloc(sizeof(coll_buffers_t));
  buf->sbuffer=NULL;
  buf->ssize=0;
  buf->rbuffer=NULL;
  buf->rsize=0;
  buf->smessage=new_message(sizeof(coll_header_t),0);
  assert(buf->smessage!=NULL);
  buf->rmessage=(message_p *)malloc(sizeof(message_p )*group_size);
  for (i=0; i<group_size; i++) {
    buf->rmessage[i]=new_message(sizeof(coll_header_t),0);
    assert(buf->rmessage[i]!=NULL);
  }
  buf->ticket=-1;
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  return buf;
}

static int
free_coll_buffers(coll_buffers_p coll_buf) {
  int i;

  for (i=0; i<group_size; i++) free_message(coll_buf->rmessage[i]);
  free(coll_buf->rmessage);
  free_message(coll_buf->smessage);
  free(coll_buf);
  return 0;
}

static coll_timers_p 
new_coll_timers(forest_p forest) {
  coll_timers_p timers;
  int i;
  int wght;

  if (! forest) return 0;
  wght = forest->weight;
  if (wght == 0) wght = 1;
  timers=(coll_timers_p)malloc(sizeof(coll_timers_t));
  timers->coll_timer
    =new_timeout(MIN_TIMEOUT,wght*INTERV_TIMEOUT,MAX_TIMEOUT);
  timers->proc_timer=(timeout_p *)malloc(sizeof(timeout_p)*group_size);
  for (i=0; i<group_size; i++) {
    timers->proc_timer[i] 
      = new_timeout(MIN_TIMEOUT,wght*INTERV_TIMEOUT,MAX_TIMEOUT);
    assert(timers->proc_timer[i] !=NULL);
  }
  return timers;
}

static int
free_coll_timers(coll_timers_p coll_timers) {
  int i;

  if (! coll_timers) return 0;
  free_timeout(coll_timers->coll_timer);
  for (i=0; i<group_size; i++)
    free_timeout(coll_timers->proc_timer[i]);
  free(coll_timers->proc_timer);
  free(coll_timers);
  return 0;
}

coll_channel_p 
channel_pointer(int ch) {
  return (coll_channel_p )get_item(coll_channel_map,ch);
}

/**********************************************************************/
/************************ Collective Operations ***********************/
/**********************************************************************/

int 
coll_operation(coll_channel_p ch, int caller, coll_operation_p opcode,
	       void *sbuffer, int ssize, char *rbuffer, int rsize, 
	       reduction_function_p rf) {
  
  if (ch->coll->members->num_members==1) return 0;

  coll_channel_wait(ch);
  lock(ch->lock);
  ch->ticket++;
  if (ch->ticket!=ch->op->ticket) {
    reset_opdescr(ch,caller,ch->ticket,rf,opcode);
  }
  reset_buffers(ch, caller, sbuffer,ssize,rbuffer,rsize);
  process_children_data(me,ch,ch->buffers->smessage);
  unlock(ch->lock);
  return 0;
}

int
coll_channel_wait(coll_channel_p ch) {
  int sum_timeout;
  
  if (ch->coll->members->num_members==1) return 0;

  do {
    sum_timeout=do_timeout(ch->timers->coll_timer);
    if (sum_timeout>0) check_channel(ch);
  } while ((sum_timeout!=0)&&(sum_timeout<getmaxt(ch->timers->coll_timer)));
  if (sum_timeout>0) {
    fprintf(stderr, "%d:\tOperation timed out -- Sending exit exception\n", me);
    send_exit_exception(ch);
    rts_sleep(1);
    exit(1);
  }
  reset_timeout(ch->timers->coll_timer);
  return 0;
}

int 
coll_channel_wait_proc(coll_channel_p ch, int procnum) { 
  int sum_timeout;

  if (ch->coll->members->num_members==1) return 0;

  do {
    sum_timeout=do_timeout(ch->timers->proc_timer[procnum]);
    if (sum_timeout>0) check_channel(ch);
  } while ((sum_timeout!=0)&&(sum_timeout<MAX_TIMEOUT));
  if (sum_timeout>0) {
    fprintf(stderr, "%d:\tOperation timed out -- Sending exit exception\n", me);
    send_exit_exception(ch);
    rts_sleep(1);
    exit(1);
  }
  reset_timeout(ch->timers->proc_timer[procnum]);
  return 0;
}


int 
process_children_data(int sender, coll_channel_p ch, message_p message) {
  coll_header_p cheader;
  coll_operation_p opcode;

#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  
  cheader=message_get_header(message);
  opcode=&operation_table[cheader->opcode];
  if (cheader->ticket!=ch->op->ticket) {
    if (cheader->ticket < ch->op->ticket) {
      /* apparently an old message that got resent somehow. ignore. */
      return 0;
    }
    reset_opdescr(ch,cheader->caller,cheader->ticket, function_pointer(cheader->rf), 
		  opcode);
  }
  if (sender!=me) {
    /* The message may have been received more than once, because "me" may
       have timed-out and requested the data again, while the sender was just
       sending it. If so, just return.
    */
    if (is_member(ch->op->received, sender)) return 0;
    (*opcode->cf)(sender,ch,message);
    add_member(ch->op->received,sender);
  }
  ch->op->num_received_from_children++;
  if (ch->op->num_received_from_children == (ch->coll->forest->fanout+1)) {
    /* All children have sent their data. 'me' can also send it's data */
    do_send_data(cheader->caller,ch);
    /* if 'me' is root, also process data locally */
    if (member(ch->coll->forest->isroot,me))
      process_roots_data(me, ch, ch->buffers->smessage);
#ifdef RELIABLE_COMM
    else if ((opcode == reduceop || opcode == gatherop) &&
	     ch->op->num_received_from_roots == ch->coll->forest->num_roots-1) {
      cancel_timeout(ch->timers->coll_timer);
    }
#endif
  }
#ifdef __MAL_DEBUG
  mal_check_all();
#endif

  return 0;
}

int 
process_roots_data(int sender, coll_channel_p ch, message_p message) {
  coll_header_p cheader;
  coll_operation_p opcode;
  
#ifdef __MAL_DEBUG
  mal_check_all();
#endif

  cheader=message_get_header(message);
  opcode=&operation_table[cheader->opcode];
  if (cheader->ticket!=ch->op->ticket) {
    if (cheader->ticket < ch->op->ticket) {
      /* apparently an old message that got resent somehow. ignore. */
      return 0;
    }
    reset_opdescr(ch,cheader->caller, cheader->ticket, function_pointer(cheader->rf), 
		  opcode);
  }

  /* The message may have been received more than once, because "me" may
     have timed-out and requested the data again, while the sender was just
     sending it. If it was received already, just return.
  */
  if (is_member(ch->op->received, sender)) return 0;

  if (cheader->ticket==ch->buffers->ticket) {
    /* 'me' has already set the buffers. Message can be unmarshalled to receive buffer */
    (*opcode->uf)(sender,ch,message);
    ch->op->num_received_from_roots++;
  }
  else if (sender!=me) {
    /* store message temporarily. It will be unmarshalled later
       by 'me' in 'reset_buffers'. */
    message_duplicate(ch->buffers->rmessage[sender],message);
  }
  add_member(ch->op->received,sender);
  if (ch->op->num_received_from_roots==ch->coll->forest->num_roots) { 
    /* signal that data is available */
    assert(ch->op->num_received_from_children == (ch->coll->forest->fanout+1));
    cancel_timeout(ch->timers->coll_timer);
  }
#ifdef RELIABLE_COMM
  else if ((opcode == reduceop || opcode == gatherop) &&
	   me != ch->op->caller &&
	   ch->op->num_received_from_children == ch->coll->forest->fanout+1 &&
	   ch->op->num_received_from_roots == ch->coll->forest->num_roots-1) {
    /* On a reduce or gather, only one processor is interested in the result,
       and this processor will not multicast its result.
    */
    cancel_timeout(ch->timers->coll_timer);
  }
#endif
#ifdef __MAL_DEBUG
  mal_check_all();
#endif

  return 0;
}

/**********************************************************************/
/************************ Static functions ****************************/
/**********************************************************************/

/* Resets the operation by removing all parameters from the previous operation. 
   This function is called by the first process that sends its data. */
static int
reset_opdescr(coll_channel_p ch, int caller,
	      int ticket, reduction_function_p rf, coll_operation_p opcode) {

#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  
  assert((ticket - ch->op->ticket) == 1);
  ch->op->caller=caller;
  empty_set(ch->op->received);
  ch->op->num_received_from_children=0;
  ch->op->num_received_from_roots=0;
  ch->op->coll_operation = opcode;
  ch->op->rf = rf;
  ch->op->ticket = ticket;
  ch->op->rbuffer_set=0;
  ch->op->smessage_set=0;
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  
  return 0;
}

/* Sets the receive buffer and other paramemters of the operation.
   This function is called only by 'me' */
static int
reset_buffers(coll_channel_p ch, int caller, char *sbuffer, int ssize, char *rbuffer, int rsize) {
  int i;
  coll_header_t op;
  
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  
  assert(ch->ticket==ch->op->ticket);
  ch->buffers->ticket=ch->ticket;
  ch->buffers->rbuffer=rbuffer;
  ch->buffers->rsize=rsize;
  /* marshall sbuffer into smessage */
  (*ch->op->coll_operation->mf)(me, ch, sbuffer, ssize);
  op.sender=me;
  op.caller=caller;
  op.channel_number=ch->channel_number;
  op.rf=function_index(ch->op->rf);
  op.opcode = ch->op->coll_operation->opcode;
  op.ticket=ch->op->ticket;
  message_set_header(ch->buffers->smessage,&op,sizeof(coll_header_t));
  /* Unmarshall any data that has already been received from a root */
  for (i=0; i<group_size; i++) {
#ifdef __MAL_DEBUG
    mal_check_all();
#endif
    if ((i!=me) && member(ch->coll->forest->isroot,i) && member(ch->op->received,i)) {
      ch->op->num_received_from_roots++;
      (*ch->op->coll_operation->uf)(me, ch, ch->buffers->rmessage[i]); 
    }
  }
  for (i=0; i<group_size; i++) {
    cancel_timeout(ch->timers->proc_timer[i]);
    start_timeout(ch->timers->proc_timer[i]);
  }
  start_timeout(ch->timers->coll_timer);
#ifdef __MAL_DEBUG
  mal_check_all();
#endif
  
  return 0;
}
 
static int 
check_channel(coll_channel_p ch) {
  int i;
  
#ifndef RELIABLE_COMM
  if (ch->op->num_received_from_children!=(ch->coll->forest->fanout+1)) {
    for (i=0; i<ch->coll->forest->fanout; i++) 
      if (!member(ch->op->received,ch->coll->forest->children[i])) {
	send_request(ch,ch->coll->forest->children[i]);
      }
  }
  else {
    for (i=0; i<ch->coll->forest->num_roots; i++)
      if ((!(member(ch->op->received,ch->coll->forest->roots[i])))
	  && (ch->coll->forest->roots[i]!=me)) {
	send_request(ch,ch->coll->forest->roots[i]);
      }
  }
#endif
  return 0;
}

