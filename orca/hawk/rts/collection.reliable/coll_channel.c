#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include "collection.h"
#include "coll_channel.h"
#include "map.h"
#include "set.h"
#include "lock.h"
#include "condition.h"
#include "mp_channel.h"
#include "message.h"
#include "message_handlers.h"
#include "assert.h"
#include "sleep.h"

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
static int reset_opdescr(coll_channel_p ch, int caller, int ticket, 
			 reduction_function_p rf, coll_operation_p opcode);
static int reset_buffers(coll_channel_p ch, int caller, char *sbuffer, int ssize, char *rbuffer, int rsize);

/**********************************************************************/
/************************ Global functions ****************************/
/**********************************************************************/

int 
init_coll_channel(int moi, int gsize, int pdebug) {
  
  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_condition(moi,gsize,pdebug);
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
  finish_condition();
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
  ch->cond=new_condition();
  ch->channel_number=insert_item(coll_channel_map,(item_p)ch);
  assert(ch->channel_number>=0);
  return ch; 
}

int
free_coll_channel(coll_channel_p ch) {
  free_mp_channel(ch->fch);
  free_coll_opdescr(ch->op);
  free_coll_buffers(ch->buffers);
  free_condition(ch->cond);
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
  op->sendphase = 1;
  op->receivephase = 0;
  op->coll_operation=NULL;
  op->rf=NULL;
  op->ticket=-1;
  op->rbuffer_set=0;
  op->smessage_set=0;
  return op;
}

static int
free_coll_opdescr(coll_opdescr_p op) {
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
    buf->rmessage[i]=0;
  }
  buf->ticket=-1;
  return buf;
}

static int
free_coll_buffers(coll_buffers_p coll_buf) {
  free(coll_buf->rmessage);
  free_message(coll_buf->smessage);
  free(coll_buf);
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

  if (ch->coll->members->num_members==1) return 0;

  await_condition(ch->cond);
  signal_condition(ch->cond);
  return 0;
}

int 
process_children_data(int sender, coll_channel_p ch, message_p message) {
  coll_header_p cheader;
  coll_operation_p opcode;
  int rv;
  int retval = 0;

  cheader=message_get_header(message);
  opcode=&operation_table[cheader->opcode];
  if (cheader->ticket!=ch->op->ticket) {
    assert(cheader->ticket > ch->op->ticket);
    reset_opdescr(ch,cheader->caller,cheader->ticket, function_pointer(cheader->rf), 
		  opcode);
  }
  rv = ch->coll->rcvphase[sender];
  assert(rv > 0);
  ch->op->receivephase |= rv;
  while (ch->op->sendphase & ch->op->receivephase) {
    /* Process messages up until the current receive phase. */
    int sf = ch->op->sendphase;
    int r = ch->coll->sender[sf];
    ch->op->sendphase <<= 1;
    if (opcode->cf && r != me) {
      /* If there is a combine function and the sender is not me, then we
         can process the message.
      */
      message_p m = sf== rv ? message : ch->buffers->rmessage[r];
      if (opcode->uf && ch->op->sendphase > ch->coll->finishrcv) {
	/* Don't bother combining for the last message received. We won't send
	   out this part anyway, so we might just as well unmarshall it.
	*/
	(*opcode->uf)(sender, ch, m);
      }
      else (*opcode->cf)(sender,ch, m);
      if (m != message) {
	free_message(m);
	ch->buffers->rmessage[r] = 0;
      }
    }
    if (ch->op->sendphase <= ch->coll->finishrcv) {
      do_send_data(cheader->caller, ch->coll->receiver[ch->op->sendphase], ch);
    }
  }
  if (ch->op->sendphase < rv && opcode->cf) {
    /* If we receive messages out of order, or receiver is not here yet, save
       the message by putting it aside.
    */
    retval = 1;
    assert(ch->buffers->rmessage[sender] == 0);
    ch->buffers->rmessage[sender] = message;
  }
  else if (ch->op->receivephase == (ch->coll->finishrcv << 1)-1) {
    /* We received all messages. Unmarshall the combined message (note that the
       other half is already unmarshalled earlier).
    */
    assert(ch->op->sendphase > ch->op->receivephase);
    if (opcode->uf) {
      (*opcode->uf)(sender, ch, ch->buffers->smessage);
    }
    signal_condition(ch->cond);
  }
  return retval;
}

/**********************************************************************/
/************************ Static functions ****************************/
/**********************************************************************/

/* Resets the operation by removing all parameters from the previous operation. 
   This function is called by the first process that sends its data. */
static int
reset_opdescr(coll_channel_p ch, int caller,
	      int ticket, reduction_function_p rf, coll_operation_p opcode) {

  assert((ticket - ch->op->ticket) == 1);
  ch->op->caller=caller;
  ch->op->receivephase=0;
  ch->op->sendphase=1;
  ch->op->coll_operation = opcode;
  ch->op->rf = rf;
  ch->op->ticket = ticket;
  ch->op->rbuffer_set=0;
  ch->op->smessage_set=0;
  
  return 0;
}

/* Sets the receive buffer and other parameters of the operation.
   This function is called only by 'me' */
static int
reset_buffers(coll_channel_p ch, int caller, char *sbuffer, int ssize, char *rbuffer, int rsize) {
  coll_header_t op;
  
  assert(ch->ticket==ch->op->ticket);
  ch->buffers->ticket=ch->ticket;
  ch->buffers->rbuffer=rbuffer;
  ch->buffers->rsize=rsize;
  /* marshall sbuffer into smessage */
  if (ch->op->coll_operation->mf) {
    (*ch->op->coll_operation->mf)(me, ch, sbuffer, ssize);
  }
  op.sender=me;
  op.caller=caller;
  op.channel_number=ch->channel_number;
  op.rf=function_index(ch->op->rf);
  op.opcode = ch->op->coll_operation->opcode;
  op.ticket=ch->op->ticket;
  message_set_header(ch->buffers->smessage,&op,sizeof(coll_header_t));
  await_condition(ch->cond);
  
  return 0;
}
