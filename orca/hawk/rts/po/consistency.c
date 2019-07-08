#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "consistency.h"
#include "communication.h"
#include "atomic_int.h"
#include "set.h"
#include "collection.h"
#include "timeout.h"
#include "po_timer.h"
#include "errno.h"
#include "map.h"
#include "misc.h"
#include "po_marshall.h"
#include "precondition.h"
#include "assert.h"
#include "sleep.h"

#define MODULE_NAME "CONSISTENCY"

struct request_s {
  int sender;
  int oid;
  int pid;
  boolean color;
};

typedef struct request_s *request_p, request_t;

static collection_p barrier_channel;

static int handle_partition(message_p message);
static int handle_request_partition(message_p message);
int send_request_partition(instance_p instance, int part_num);
static po_lock_p message_lock, short_message_lock;
static message_p message, short_message;
static po_timer_p wait_part_timer, send_part_timer;
static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static int num_parts_sent=0;
static int num_parts_req=0;

#ifdef __MAL_DEBUG
extern void mal_check_all();
#endif

/*=======================================================================*/
/* Initialized the module. */
/*=======================================================================*/
int 
init_consistency_module(int moi, int gsize, int pdebug, int *argc, char **argv) 
{

  if (initialized++) return 0;
  precondition((moi>=0)&&(moi<gsize));
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_po_timer(moi,gsize,pdebug);
  init_lock(me,group_size,proc_debug);
  init_atomic_int(me,group_size,proc_debug);
  init_message(me,group_size,proc_debug);
  init_pdg(me,group_size,proc_debug);
  init_collection(moi,gsize,pdebug,argc,argv);
  init_timeout(moi,gsize,pdebug,argc,argv);
  barrier_channel=new_collection(NULL);
  message=new_message(sizeof(request_t),0);
  message_lock=new_lock();
  short_message=new_message(sizeof(request_t),0);
  short_message_lock=new_lock();
  message_register_handler(handle_partition);
  message_register_handler(handle_request_partition);
  wait_part_timer=new_po_timer("wait_part");
  send_part_timer=new_po_timer("send_part");
  return 0;
}

/*=======================================================================*/
/* Finishes the module. */
/*=======================================================================*/
int
finish_consistency_module(void) 
{

  if (--initialized) return 0;

  free_po_timer(send_part_timer);
  free_po_timer(wait_part_timer);
  free_lock(short_message_lock);
  free_message(short_message);
  free_message(message);
  free_lock(message_lock);
  free_collection(barrier_channel);
/*
  fprintf(stderr,"%d:\t Sent %d partitions\n", me, num_parts_sent);
  fprintf(stderr,"%d:\t Received %d requests\n", me, num_parts_req);
*/
  finish_timeout();
  finish_collection();
  finish_pdg();
  finish_message();
  finish_atomic_int();
  finish_lock();
  finish_po_timer();
  return 0;
}


/*=======================================================================*/
/* Creates a new consistency structure for an object. */
/*=======================================================================*/

consistency_p 
new_consistency(instance_p instance) 
{
  consistency_p consistency;

  precondition(instance!=NULL);

  consistency=(consistency_p)malloc(sizeof(consistency_t));
  assert(consistency!=NULL);
  consistency->instance=instance;
  consistency->color=FALSE;
  consistency->status[FALSE]=new_atomic_int(INVALID);
  consistency->status[TRUE]=new_atomic_int(VALID);
  consistency->locked[FALSE] = new_condition();
  tawait_condition(consistency->locked[FALSE], 0, PO_SEC);
  consistency->locked[TRUE] = new_condition();
  consistency->part_status[0]=NULL;
  consistency->part_status[1]=NULL;
  instance->consistency=consistency;
  return consistency;
}

  
/*=======================================================================*/
/* Frees a consistency structure. */
/*=======================================================================*/

int 
free_consistency(instance_p instance) 
{
  consistency_p consistency;
  int i;

  consistency=instance->consistency;
  for (i=0; i<instance->state->num_parts; i++) {
    free_timeout(consistency->timeouts[0][i]);
    free_timeout(consistency->timeouts[1][i]);
    free_atomic_int(consistency->part_status[0][i]);
    free_atomic_int(consistency->part_status[1][i]);
  }
  if (consistency->part_status[0]) {
    free(consistency->part_status[0]);
    free(consistency->part_status[1]);
  }
  if (consistency->timeouts[0]) {
    free(consistency->timeouts[0]);
    free(consistency->timeouts[1]);
  }
  free_condition(consistency->locked[FALSE]);
  free_condition(consistency->locked[TRUE]);
  free_atomic_int(consistency->status[FALSE]);
  free_atomic_int(consistency->status[TRUE]);
  free(consistency);
  instance->consistency=NULL;
  return 0;
}


/*=======================================================================*/
/* Changes the color of an object. */
/*=======================================================================*/

int 
change_color(instance_p instance) 
{
  precondition(instance!=NULL);
  precondition(instance->consistency!=NULL);

  await_condition(instance->consistency->locked[!(instance->consistency->color)]);
  instance->consistency->color=!(instance->consistency->color);
  signal_condition(instance->consistency->locked[!(instance->consistency->color)]);
  return 0;
}

/*=======================================================================*/
/* If the given partition is invalid, the procedure fetches it from
   the owner. If all partitions are already being fetched or this
   partition is already being fetched, nothing needs to be done. */
/*=======================================================================*/
int 
fetch_part(instance_p instance, int part_num) 
{
  int old_val;
  boolean color;

  precondition(instance!=NULL);

  if (group_size==1) return 0;

  color=!(instance->consistency->color);
  if (instance->state->owner[part_num]==me) return 0;
 
  if ((value(instance->consistency->status[color])==VALID)||
      (value(instance->consistency->status[color])==INSYNC)) 
    return 0;
  old_val=compare_and_swap
    (instance->consistency->part_status[color][part_num],INVALID,INSYNC);
  if ((old_val==INSYNC)||(old_val==VALID)) return 0;
  send_request_partition(instance,part_num);
  return 0;
}


/*=======================================================================*/
/* COLLECTIVE OPERATION: must be called by all processes. If all
   partitions are invalid, the procedure fetches them using a gather
   operation on the object's channel. */
/*=======================================================================*/
int 
fetch_all(instance_p instance) {
  int old_val, i;
  boolean color;
 
  precondition(instance!=NULL);
  
  if (group_size==1) return 0;

  color=!(instance->consistency->color);
  if ((value(instance->consistency->status[color])==VALID)||
      (value(instance->consistency->status[color])==INSYNC)) 
    return 0;
  old_val=compare_and_swap(instance->consistency->status[color],
			   INVALID,INSYNC);
  if ((old_val==INSYNC)||(old_val==VALID)) return 0;
  assert(old_val==INVALID);
  /* It does not really matter if another thread does this
     concurrently, the function is idempotent. So there is no
     concurrency control for setting each partition INSYNC in this
     case. */
  for (i=0; i<instance->state->num_parts; i++)
    if (instance->state->owner[i]!=me)
      set_value(instance->consistency->part_status[color][i],INSYNC);
  gather_po(instance->synch_ch,instance->state->data);
  return 0;
}


/*=======================================================================*/
/* Blocks until all partitions are validated. */
/*=======================================================================*/
int 
access_all(instance_p instance) {
  int i;
  int old_val;
  boolean color;
 
  precondition(instance!=NULL);
  
  if (group_size==1) return 0;
 
  color=!(instance->consistency->color);
  old_val=compare_and_swap(instance->consistency->status[color],
			   INVALID,INSYNC);
  if (old_val==INVALID) {
    fetch_all(instance);
  }
  collection_wait(instance->synch_ch);
  set_value(instance->consistency->status[color],VALID);
  for (i=0; i<instance->state->num_parts; i++)
    {
      set_value(instance->consistency->part_status[color][i],VALID);  
    }
  return 0;
}
 
/*=======================================================================*/
/* Invalidate all nonowned partitions. */
/*=======================================================================*/
int 
invalidate_all(instance_p instance) 
{
  register int i;
  register boolean color;

  precondition(instance!=NULL);

  color=!(instance->consistency->color);
  set_value(instance->consistency->status[color],INVALID);

  for (i=0; i<instance->state->num_parts; i++)
    if (instance->state->owner[i]!=me)
      {
	set_value(instance->consistency->part_status[color][i],INVALID);
      }
  return 0;
}

/* ======================================================= */
/* Sends a partition using a sender-initiated strategy. */
/* ======================================================= */

int 
send_part(instance_p instance, int dest, int part_num, boolean color) 
{
  request_t req;

  precondition(part_num>=0);
  precondition(part_num<instance->state->num_parts);

  if (dest==me) return 0;
  start_po_timer(send_part_timer);
  lock(message_lock);
  req.sender=me;
  req.oid=instance->id;
  req.pid=part_num;
  req.color=color;
  message_clear_data(message);
  message_set_header(message,&req,sizeof(request_t));
  marshall_partition(message, instance, part_num);
  message_set_handler(message,handle_partition);
  mp_channel_unicast(instance->fch,dest,message);
  unlock(message_lock);
  num_parts_sent++;
  end_po_timer(send_part_timer);
  add_po_timer(send_part_timer);
  return 0;
}

/* ======================================================= */
/* Sends a partition using a sender-initiated startegy. */
/* ======================================================= */

int 
multicast_part(instance_p instance, int part_num) 
{
  request_t req;

  precondition(part_num>=0);
  precondition(part_num<instance->state->num_parts);

  lock(message_lock);
  req.sender=me;
  req.oid=instance->id;
  req.pid=part_num;
  req.color=!(instance->consistency->color);
  message_clear_data(message);
  message_set_header(message,&req,sizeof(request_t));
  marshall_partition(message, instance, part_num);
  message_set_handler(message,handle_partition);
  mp_channel_multicast(instance->fch,message);
  unlock(message_lock);
  return 0;
}


/*=======================================================================*/
/* Processes the reception of a partition. Writes the contents in the
   consistent state of the instance given in the reply, and awakes any
   thread blocked waiting for the partition to become consistent. */
/*=======================================================================*/

static int 
handle_partition(message_p message) 
{
  instance_p instance;
  request_p req; 
  void *data;

  req=message_get_header(message);
  data=message_get_data(message);

  instance=get_instance(req->oid);

  if (! instance) {
	/* Can this happen? Maybe with a resent message? */
	return 0;
  }
  assert(instance->state!=NULL);
  assert(instance->state->partition!=NULL);
  assert(instance->state->partition[req->pid]!=NULL);

  if (!is_member(instance->processors,me)) return 0;

  instance->state->partition[req->pid]->received++;
  instance->state->partition[req->pid]->last_color=req->color;
  unmarshall_partition(data, instance, req->pid);
  set_value(instance->consistency->part_status[req->color][req->pid],VALID);
  cancel_timeout(instance->consistency->timeouts[req->color][req->pid]);
  return 0;
}


/*=======================================================================*/
/*=======================================================================*/
static int
handle_request_partition(message_p message) 
{
  instance_p instance;
  request_p req; 

  req=message_get_header(message);
  instance=get_instance(req->oid);
  assert(instance!=NULL);
  assert(instance->state!=NULL);
  assert(instance->state->partition!=NULL);
  assert(instance->state->partition[req->pid]!=NULL);
  send_part(instance, req->sender, req->pid, req->color);
  num_parts_req++;
  return 0;
}

/* ======================================================= */
/* ======================================================= */

int 
pdg_fetch(instance_p instance, po_opcode opcode) 
{
  int source;
  int i;

  for (i=0; i<instance->state->num_owned_parts; i++) {
      source=instance->state->owned_partitions[i]->part_num;
      pdg_fetch_part(instance,opcode,source);
    }
  return 0;
}

/* ======================================================= */
/* Uses the given pdg to fetch partitions dependent on owned
   partitions. Partitioning and distribution must be defined before
   calling this procedure. */
/* ======================================================= */

int 
pdg_fetch_part(instance_p instance, po_opcode opcode, int source) 
{
  pdg_p pdg;
  int dst;

  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  
  edge_loop(pdg->graph, source, dst, 
	{	if (instance->state->owner[dst] != me) {
			fetch_part(instance, dst);
		}
	}
	    );
  return 0;
}
	   
/* ======================================================= */
/* Uses the given pdg to send partitions to other
   processors. Partitioning and distribution must be defined before
   calling this procedure. */
/* ======================================================= */

int
pdg_send_part(instance_p instance, po_opcode opcode, int s) 
{
  int dst, dstcpu;
  pdg_p pdg;
  char *done = malloc(group_size);

  memset(done, '\0', group_size);
  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);

  rev_edge_loop(pdg->graph, s, dst, {	
    dstcpu = instance->state->owner[dst];
    if (dstcpu != me && ! done[dstcpu]) {
      /* dstcpu could have more than one partition depend on s. Make sure
	 it gets sent only once.
      */
      done[dstcpu] = 1;
      send_part(instance, dstcpu, s, !(instance->consistency->color));
    }
  }
  );
  free(done);
  return 0;
}
	   
/* ======================================================= */
/* ======================================================= */

int 
pdg_send(instance_p instance, po_opcode opcode) 
{
  int i;
  pdg_p pdg = get_pdg(instance, opcode);
  char *done = malloc(group_size);

  assert(pdg!=NULL);

  for (i=0; i<instance->state->num_owned_parts; i++) {
    int s = instance->state->owned_partitions[i]->part_num;
    int dst;

    for (dst = group_size-1; dst >= 0; dst--) {
	done[dst] = 0;
    }
    rev_edge_loop(pdg->graph, s, dst, {
      int dstcpu = instance->state->owner[dst];
      if (dstcpu != me && ! done[dstcpu]) {
        /* dstcpu could have more than one partition depend on s. Make sure
           it gets sent only once.
        */
        done[dstcpu] = 1;
        send_part(instance, dstcpu, s, !(instance->consistency->color));
      }
    }
    );

  }
  free(done);
  return 0;
}

/*=======================================================================*/
/* Waits until the specified partition is available. If after a
   certain amount of time (determined by partition->timeouts[part_num]),
   if the partition is still not consistent, it requests it. */
/*=======================================================================*/
int 
wait_part(instance_p instance, int part_num) { 
  int sum_timeout;
  boolean color;
  int v;

  if (instance->state->owner[part_num]==me) return 0;
  color=!(instance->consistency->color);
  if (value(instance->consistency->part_status[color][part_num]) == VALID) {
    return 0;
  }
  start_timeout(instance->consistency->timeouts[color][part_num]);
  v=value(instance->consistency->part_status[color][part_num]);
  if (v==VALID) {
	cancel_timeout(instance->consistency->timeouts[color][part_num]);
	return 0;
  }
  start_po_timer(wait_part_timer);
  do {
    sum_timeout=do_timeout
      (instance->consistency->timeouts[color][part_num]);
    v=value(instance->consistency->part_status[color][part_num]);
    if ((sum_timeout>0) && (v!=VALID)) {
      send_request_partition(instance,part_num);
    }
  }
  while ((sum_timeout!=0)&&
	 (sum_timeout<getmaxt(instance->consistency->timeouts[color][part_num])));
  if (sum_timeout>0) {
      send_exit(barrier_channel);
      fprintf(stderr,"%d:\tSending exit exception\n", me);
      rts_sleep(2);
    }
  cancel_timeout(instance->consistency->timeouts[color][part_num]);
  assert(value(instance->consistency->part_status[color][part_num])==VALID);
  end_po_timer(wait_part_timer);
  add_po_timer(wait_part_timer);
  return 0;
  
}

/* ======================================================= */
/* Sends a request for partition when using a sender-initiated
   strategy. */
/* ======================================================= */

int 
send_request_partition(instance_p instance, int part_num) 
{
  request_t req;
  int receiver;

  precondition(part_num>=0);
  precondition(part_num<instance->state->num_parts);

  receiver=instance->state->owner[part_num];
  lock(short_message_lock);
  instance->state->partition[part_num]->requested++;
  req.sender=me;
  req.oid=instance->id;
  req.pid=part_num;
  req.color=!(instance->consistency->color);
  message_set_header(short_message,&req,sizeof(request_t));
  message_set_handler(short_message,handle_request_partition);
  mp_channel_unicast(instance->fch,receiver,short_message);
  unlock(short_message_lock);
  return 0;
}

int 
check_element(instance_p instance, int *element) {
  int partnum, i;
  boolean color;
  precondition(instance!=NULL);

  color=!instance->consistency->color;
  partnum=partition(instance,element);
  if (group_size==1) return 0;
  if (value(instance->consistency->status[color])==VALID)
    return 0;
  if (value(instance->consistency->part_status[color][partnum])==VALID) return 0;
  send_exit(barrier_channel);
  fprintf(stderr,"%d: element in partition %d is invalid\n", me, partnum);
  fprintf(stderr,"Index of element accessed: ");
  for (i=0; i<instance->po->num_dims; i++) fprintf(stderr,"%d ", element[i]);
  fprintf(stderr,"\n");
  assert(0);
  exit(1);
  return -1;
}

/* ======================================================= */
/* ======================================================= */

int 
pdg_wait_part(instance_p instance, po_opcode opcode, int source) 
{
  boolean color;
  pdg_p pdg;
  int dst;

  precondition(instance!=NULL);

  if (group_size==1) return 0;

  color=!(instance->consistency->color);
  if (value(instance->consistency->status[color])==VALID) return 0;
  if (value(instance->consistency->status[color])==INSYNC) 
    {
      collection_wait(instance->synch_ch);
      set_value(instance->consistency->status[color],VALID);
      return 0;
    }

  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  
  edge_loop(pdg->graph, source, dst, {	
    if (instance->state->owner[dst] != me) {
      wait_part(instance, dst);
    }
  }
  );
  return 0;
}


/* ======================================================= */
/* ======================================================= */

int 
pdg_wait_all(instance_p instance, po_opcode opcode) 
{
  boolean color;
  pdg_p pdg;
  int p, source;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);

  if (group_size==1)  return 0;
  if (instance->id==0) return 0;
  color=!(instance->consistency->color);
  if (value(instance->consistency->status[color])==VALID) return 0;
  if (value(instance->consistency->status[color])==INSYNC) {
    collection_wait(instance->synch_ch);
    set_value(instance->consistency->status[color],VALID);
    return 0;
  }

  pdg=get_pdg(instance,opcode);
  if (pdg==NULL) return 0;
 
  for (p=0; p<instance->state->num_owned_parts; p++) {
    int dst;
    source=instance->state->owned_partitions[p]->part_num;
    edge_loop(pdg->graph, source, dst, {	
      if (instance->state->owner[dst] != me) {
	wait_part(instance, dst);
      }
    }
    );
  }
  return 0;
}


boolean
isconsistent(instance_p instance, int part_num) {
  boolean color;

  if (group_size==1) return TRUE;
  if (instance->state->owner[part_num]==me) return TRUE;
  color=!(instance->consistency->color);
  if (value(instance->consistency->status[color])==VALID) return TRUE;
  if (value(instance->consistency->part_status[color][part_num])==VALID) return TRUE;
  return FALSE;
}
