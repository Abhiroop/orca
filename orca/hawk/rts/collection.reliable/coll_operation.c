#include "coll_operation.h"
#include "po.h"
#include "assert.h"
#include "po_marshall.h"
#include "mp_channel.h"

#define MAXOPS 100

coll_operation_t operation_table[MAXOPS];
int opindex=0;

coll_operation_p gatherop;
coll_operation_p gatherallop;
coll_operation_p reduceallop;
coll_operation_p reduceop;
coll_operation_p barrierop;
coll_operation_p gather_poop;

static int initialized=0;

static int unicast_data(int dest, coll_channel_p ch);
static int unicast_header(int dest, coll_channel_p ch);

static void gather_pomf(int sender, coll_channel_p ch, void *ignored, int signored);
static void gather_pocf(int sender, coll_channel_p ch, message_p message2);
static void gather_pouf(int sender, coll_channel_p ch, message_p message);
static void gather_posf(int dest, coll_channel_p ch);

static void reduceallmf(int sender, coll_channel_p ch, void *sbuffer, int ssize);
static void reduceallcf(int sender, coll_channel_p ch, message_p message2);
static void reducealluf(int sender, coll_channel_p ch, message_p message);
static void reduceallsf(int dest, coll_channel_p ch);

static void gatherallmf(int sender, coll_channel_p ch, void *sbuffer, int ssize);
static void gatherallcf(int sender, coll_channel_p ch, message_p message2);
static void gatheralluf(int sender, coll_channel_p ch, message_p message);
static void gatherallsf(int dest, coll_channel_p ch);

static void barriersf(int dest, coll_channel_p ch);

coll_operation_p
new_coll_operation(marshall_function_p mf, combine_function_p cf,
		   unmarshall_function_p uf, send_function_p sf) {
  if (opindex==MAXOPS) return NULL;
  operation_table[opindex].mf = mf;
  operation_table[opindex].cf = cf;
  operation_table[opindex].uf = uf;
  operation_table[opindex].sf = sf;
  operation_table[opindex].opcode = opindex++;
  return &(operation_table[opindex-1]);
}

/* ======================= specific PO Gather functions ================= */
static void
gather_pomf(int sender, coll_channel_p ch, void *ignored, int signored) {
  instance_p instance;
  int i, j;

  instance=(instance_p )ch->op->scratch;
  assert(instance!=NULL);
  message_clear_data(ch->buffers->smessage);
  for (i=0; i<instance->state->num_owned_parts; i++) {
    message_append_data(ch->buffers->smessage, 
			&(instance->state->owned_partitions[i]->part_num), 
			sizeof(int));
    j = instance->state->owned_partitions[i]->num_elements*
      instance->po->element_size;
    message_append_data(ch->buffers->smessage, &j, sizeof(int));
    marshall_partition(ch->buffers->smessage,instance,
		       instance->state->owned_partitions[i]->part_num);
  }
}

static void
gather_pocf(int sender, coll_channel_p ch, message_p message2) {
  instance_p instance;

  instance=(instance_p )ch->op->scratch;
  assert(instance!=NULL);
  message_append_data(ch->buffers->smessage,
			message_get_data(message2),message_get_data_size(message2)); 
}

static void
gather_pouf(int sender, coll_channel_p ch, message_p message) {
  char *buffer,*p;
  int bsize;
  instance_p instance;

  instance=(instance_p )ch->op->scratch;
  buffer=p=message_get_data(message);
  bsize=message_get_data_size(message);
  while (p<(buffer+bsize)) {
    unmarshall_partition((void *)(p+2*sizeof(int)),instance,*(int *)p);
    p += (((int *)p)[1] + 2*sizeof(int));
  }
}

static void
gather_posf(int dest, coll_channel_p ch) {
    unicast_data(dest, ch);
}


/* ======================= specific Gather functions ================= */
static void
gatherallmf(int sender, coll_channel_p ch, void *sbuffer, int ssize) {
  instance_p instance;
  int i;

  instance=(instance_p )ch->op->scratch;
  assert(instance!=NULL);
  message_clear_data(ch->buffers->smessage);
  for (i=0; i<instance->state->num_owned_parts; i++) {
    int partno = instance->state->owned_partitions[i]->part_num;
    message_append_data(ch->buffers->smessage, 
			&partno,
			sizeof(int));
    gather_marshall(ch->buffers->smessage, instance, partno, sbuffer, ssize);
  }
}

static void
gatherallcf(int sender, coll_channel_p ch, message_p message2) {
  message_append_data(ch->buffers->smessage,
			message_get_data(message2),message_get_data_size(message2)); 
}

static void
gatheralluf(int sender, coll_channel_p ch, message_p message) {
  char *buffer,*p;
  int bsize;
  instance_p instance;

  instance=(instance_p )ch->op->scratch;
  buffer=p=message_get_data(message);
  bsize=message_get_data_size(message);
  while (p<(buffer+bsize)) {
    p = gather_unmarshall((void *)(p+sizeof(int)),instance,*(int *)p, ch->buffers->rbuffer, ch->buffers->rsize);
  }
}

static void
gatherallsf(int dest, coll_channel_p ch) {
    unicast_data(dest, ch);
}


/* ======================= Reduce all functions ================= */
static void
reduceallmf(int sender, coll_channel_p ch, void *sbuffer, int ssize) {
    message_set_data(ch->buffers->smessage,sbuffer,ssize);
}

static void
reduceallcf(int sender, coll_channel_p ch, message_p message2) {
    message_combine_data(ch->buffers->smessage,message_get_data(message2),
			 message_get_data_size(message2),ch->op->rf);
}

static void
reducealluf(int sender, coll_channel_p ch, message_p message) {

  if (ch->op->rbuffer_set==0) {
    message_copy_data(message,ch->buffers->rbuffer,ch->buffers->rsize);
    ch->op->rbuffer_set=1;
  }
  else
    (*ch->op->rf)(ch->buffers->rbuffer,message_get_data(message));
}

static void
reduceallsf(int dest, coll_channel_p ch) {
    unicast_data(dest, ch);
}

/* ======================= Barrier functions ================= */

static void
barriersf(int dest, coll_channel_p ch) {
  unicast_header(dest, ch);
}

/*=======================================================================*/
/*======================= Static Functions ==============================*/
/*=======================================================================*/

static int 
unicast_data(int dest, coll_channel_p ch) {
  mp_channel_unicast(ch->fch,dest,ch->buffers->smessage);
  return 0;
}
 
static int 
unicast_header(int dest, coll_channel_p ch) {
  message_clear_data(ch->buffers->smessage);
  mp_channel_unicast(ch->fch,dest,ch->buffers->smessage);
  return 0;
}
 
int
init_coll_operation(int moi, int gsize, int pdebug) {
  
  if (initialized++) return 0;
  gather_poop=new_coll_operation(gather_pomf,gather_pocf,gather_pouf,gather_posf);
  reduceallop=new_coll_operation(reduceallmf,reduceallcf,reducealluf,reduceallsf);
  reduceop=new_coll_operation(reduceallmf,reduceallcf,reducealluf,reduceallsf);
  barrierop=new_coll_operation(0,0,0,barriersf);
  gatherop=new_coll_operation(gatherallmf,gatherallcf,gatheralluf,gatherallsf);
  return 0;
}
  
int
finish_coll_operation(void) {
  if (--initialized) return 0;
  return 0;
}
