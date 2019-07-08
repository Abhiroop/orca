#include <stdlib.h>
#include <string.h>

#include "po_invocation.h"
#include "po.h"
#include "instance.h"
#include "grp_channel.h"
#include "condition.h"
#include "collection.h"
#include "set.h"
#include "po_timer.h"
#include "consistency.h"
#include "util.h"
#include "lock.h"
#include "precondition.h"
#include "assert.h"

#ifdef PO_TIMERS_ON
#define start_timing(t) { if (instance->id>0) start_po_timer(t); }
#define stop_timing(t) { if (instance->id>0) { 	\
end_po_timer(t); 				\
add_po_timer(t); } }
#else
#define start_timing(t)
#define stop_timing(t)
#endif

int num_inv=0;
extern po_lock_p po_rts_lock;

static void dynamic_pdg(instance_p instance, int opid, po_opcode opcode, void **args);
static int invocation_channel_handler(message_p message);

void (*opmarshall_func)(message_p mp, void **arg, void *d);
char *(*opunmarshall_func)(char *p, void ***arg, void *d, void **args, int sender, instance_p ip);
void (*opfree_in_params_func)(void **args, void *d, int remove_outs);

void *(*p_gatherinit)(instance_p i, void *a, void *d);

/* Predefined attributes for an operation. They can be overwritten by
   using the primitive change_handler. */

handlers_t ParRead = { FALSE,
  i_multicast, t_sender, e_parallel_nonblocking,
  r_collective_caller, c_no_commit };
handlers_t ParWrite = { TRUE,
  i_multicast, t_sender, e_parallel_nonblocking,
  r_collective_caller, c_commit_owned };
handlers_t SeqWrite = { FALSE,
  i_multicast, t_collective_all, e_sequential_replicated,
  r_local, c_no_commit };
handlers_t SeqRead = { FALSE,
  i_multicast, t_collective_caller, e_sequential_local,
  r_local, c_no_commit };
handlers_t SeqInit = { FALSE,
  i_multicast, t_no_transfer, e_sequential_replicated,
  r_local, c_save_all };
handlers_t RepRead = { FALSE,
  i_multicast, t_no_transfer, e_sequential_replicated,
  r_local, noop };

/* RTS operations. */
handlers_t RtsOp = { FALSE,
  i_multicast, t_no_transfer, e_sequential_replicated, 
  r_collective_caller, c_no_commit };

/* Orca Operations. */
handlers_t OrcaRepRead = { 
  FALSE,
  i_local, t_no_transfer, e_sequential_replicated,
  r_local, c_no_commit
};

handlers_t OrcaRepWrite = { 
  FALSE,
  i_multicast, t_no_transfer, e_sequential_replicated,
  r_local, c_no_commit
};

handlers_t OrcaLocalRead = { 
  FALSE,
  i_local, t_no_transfer, e_sequential_replicated,
  r_local, c_no_commit 
};

handlers_t OrcaLocalWrite = { 
  FALSE,
  i_local, t_no_transfer, e_sequential_replicated,
  r_local, c_no_commit 
};

typedef struct invocation_s *invocation_p, invocation_t;

struct invocation_s {
  int sender;
  instance_p instance;
  int operation_id;
  void **args;
};

static int commit_owned_partitions(instance_p instance);
static int save_owned_partitions(instance_p instance);
static int unmarshall_invocation(message_p message, invocation_p invocation);
static int marshall_invocation(int me, instance_p instance, int opid, 
			       void **args, message_p operation_buffer);     
static collection_p barrier_channel;
static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static int register_handlers(void);
static map_p handler_table;

/* Timers for profiling. */
static po_timer_p i_local_timer;
static po_timer_p i_collective_timer;
static po_timer_p i_multicast_timer;

static po_timer_p t_collective_caller_timer;
static po_timer_p t_collective_all_timer;
static po_timer_p t_receiver_timer;
static po_timer_p t_sender_timer;

static po_timer_p e_sequential_local_timer;
static po_timer_p e_sequential_replicated_timer;
static po_timer_p e_sequential_replicated_timer;
static po_timer_p e_parallel_blocking_timer;
static po_timer_p e_parallel_consistent_timer;
static po_timer_p e_parallel_nonblocking_timer;
static po_timer_p e_parallel_control_timer;

static po_timer_p r_collective_caller_timer;

static po_timer_p c_commit_owned_timer;
static po_timer_p c_save_all_timer;

#ifdef __MAL_DEBUG
extern void mal_check_all();
#endif

static void initialize_timers(void);
static void free_timers(void);

/*=======================================================================*/
/* Initializes the module. */
/*=======================================================================*/

int 
init_invocation_module(int moi, int gsize, int pdebug, int max_arg_size,int *argc, char **argv) {
  precondition((moi>=0)&&(moi<gsize));

  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;

  init_collection(moi,gsize,pdebug,argc,argv);
  init_po_timer(moi,gsize,pdebug);
  initialize_timers();
  init_message(moi,gsize,pdebug);
  barrier_channel=new_collection(NULL);
  init_grp_channel(moi,gsize,pdebug);
  handler_table=new_map();
  register_handlers();
  message_register_handler(invocation_channel_handler);
  return 0;
}

/*=======================================================================*/
/* Finishes the module. */
/*=======================================================================*/

int 
finish_invocation_module(void) {
  if (--initialized) return 0;  
  barrier(barrier_channel);
  collection_wait(barrier_channel);
  /* free handler_map ??? */
  finish_grp_channel();
  free_collection(barrier_channel);
  finish_message();
  free_timers();
  finish_po_timer();
  finish_collection();
  return 0;
}


/*=======================================================================*/
/* Invokes the operation on an instance. Checks the type of the
   operation first. If the operation must be executed locally, it
   calls the appropriate call handler. Otherwise, it sends a group
   message containing a description of the invocation. */
/*=======================================================================*/
int 
do_operation(instance_p instance, po_opcode opcode, void **args) {
  int opid;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
 
  opid=operation_index(instance->po,(item_p)opcode);

  assert(opid>=0);
  await_condition(instance->no_local_invocation);
  (*instance->handlers[opid].i_handler)(me,instance, opcode, args);
  return 0;
}

/*************************************************************************/
/*************************************************************************/
/* Noop handler. */
/*************************************************************************/
/*************************************************************************/

void 
noop(int sender, instance_p instance, po_opcode opcode, void **args) {
  return;
}

/*************************************************************************/
/*************************************************************************/
/* Invocation Handlers. */
/*************************************************************************/
/*************************************************************************/

/*=======================================================================*/
/* Local invocation handler. Calls the following handlers
   sequentially. */
/*=======================================================================*/
void 
i_local(int sender, instance_p instance, po_opcode opcode, void **args)
{
  int opid;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);

  start_timing(i_local_timer);
  lock(po_rts_lock);
  opid=operation_index(instance->po,(item_p)opcode);
  assert(opid>=0);

  if (instance->processors->num_members==1 && p_gatherinit) {
  	po_operation_p opd = get_item(instance->po->op_table, opid);
	int i;
  	for (i = 0; i < opd->nparams; i++) {
	    if (is_gather(&(opd->param_descrs[i]))) {
		args[i] = (*p_gatherinit)(instance, args[i], opd->param_descrs[i].par_d);
	    }
	}
  }
  if (instance->state->num_owned_parts &&
      instance->state->num_parts != instance->state->num_owned_parts) {
  	dynamic_pdg(instance, opid, opcode, args);
  }
  /* Block until all operations issued are ended and signal start of a
     new operation. */
  (*instance->handlers[opid].t_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].e_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].r_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].c_handler)(sender, instance, opcode, args);
  if (sender==me) signal_condition(instance->no_local_invocation);
  unlock(po_rts_lock);
  stop_timing(i_local_timer);
}

/*=======================================================================*/
/* Collective invocation handler. Just like the local invocation
   handler, calls the following handlers sequentially. However, there
   must be an agreement between all processors holding the state of
   the instance to call this object collectively. */
/*=======================================================================*/
void 
i_collective(int sender, instance_p instance, po_opcode opcode, void **args) {
  int opid;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(i_collective_timer);
  lock(po_rts_lock);
  opid=operation_index(instance->po,(item_p)opcode);
  assert(opid>=0);
  
  if (instance->state->num_owned_parts &&
      instance->state->num_parts != instance->state->num_owned_parts) {
  	dynamic_pdg(instance, opid, opcode, args);
  }
  (*instance->handlers[opid].t_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].e_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].r_handler)(sender, instance, opcode, args);
  (*instance->handlers[opid].c_handler)(sender, instance, opcode, args);
  if (sender==me) signal_condition(instance->no_local_invocation);
  unlock(po_rts_lock);
  stop_timing(i_collective_timer);
}

/*=======================================================================*/
/* Multicast invocation handler. Marshalls the invocation and sends it
   off to all processors using group communication. The threads that
   receive the invocation message will take care of the rest of the
   invocation. */
/*=======================================================================*/
void 
i_multicast(int sender, instance_p instance, po_opcode opcode, void **args) {
  int opid;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(i_multicast_timer);
  if (instance->processors->num_members==1) 
    i_local(sender, instance, opcode, args);
  else {
    opid=operation_index(instance->po,(item_p)opcode);
    assert(opid>=0);
    marshall_invocation(me, instance, opid, args, instance->invocation_message);
    /* send operation on the invocation channel. */
    message_set_handler(instance->invocation_message, invocation_channel_handler);
    grp_channel_send(instance->gch, instance->invocation_message);
  }
  stop_timing(i_multicast_timer);
}

/*=======================================================================*/
/* Invocation handler. Calls the different handlers associated with
   the instance and the operation. */
/*=======================================================================*/
static int 
invocation_channel_handler(message_p message)  {
  po_opcode opcode;
  int opid;
  invocation_t invocation;
  
  /* Unmarshall operation and prepare result buffers. */
  lock(po_rts_lock);
  unmarshall_invocation(message, &invocation);
  opid=invocation.operation_id;
  if (!member((invocation.instance->processors),me)) {
    /* Free parameters, using the Orca function if there is one. */
    if (invocation.args != 0) {
      if (opfree_in_params_func) {
        po_operation_p opd = 
	  get_item((invocation.instance)->po->op_table, opid);
        if (opd->opdescr) {
      	  (*opfree_in_params_func)(invocation.args, opd->opdescr, 1);
        }
      }
      free(invocation.args);
    }
    unlock(po_rts_lock);
    /* HERE: SIGNAL ORCA RTS OF END OF INVOCATION. */
    return 0;
  }
  opcode=
    (po_opcode)get_item((invocation.instance)->po->opcode_table, opid);
  /* Block until all operations issued are ended and signal start of a
     new operation. */
  assert(opcode!=NULL);
  dynamic_pdg(invocation.instance, opid, opcode, invocation.args);
  (*invocation.instance->handlers[opid].t_handler)
    (invocation.sender, invocation.instance, opcode, invocation.args);
  (*invocation.instance->handlers[opid].e_handler)
    (invocation.sender, invocation.instance, opcode, invocation.args);
  (*invocation.instance->handlers[opid].r_handler)
    (invocation.sender, invocation.instance, opcode, invocation.args);
  (*invocation.instance->handlers[opid].c_handler)
    (invocation.sender, invocation.instance, opcode, invocation.args);
  /* Free parameters, using the Orca function if there is one. */
  if (invocation.args) {
    if (opfree_in_params_func) {
      po_operation_p opd = 
        get_item((invocation.instance)->po->op_table, opid);
      if (opd->opdescr) {
        (*opfree_in_params_func)(invocation.args, opd->opdescr, invocation.sender != me);
      }
    }
    if (invocation.sender!=me) free(invocation.args);
  }
  invocation.instance->parallel_update = 
    invocation.instance->parallel_update 
    || invocation.instance->handlers[opid].parallel_update;
  if (invocation.sender==me)
    signal_condition(invocation.instance->no_local_invocation); 
  
  /* HERE: SIGNAL ORCA RTS OF END OF INVOCATION. */
  unlock(po_rts_lock);
  return 0;
}
  
/*=======================================================================*/
/* Unmarshalls the parameters of an operation. Prepares buffers for
   return values if the operation has been issued by a different
   platform. */
/*=======================================================================*/
int 
unmarshall_invocation(message_p message, invocation_p invocation) 
{
  po_operation_p opd;
  int i;
  void *p;
  void **args;
  marshalled_invocation_p marshalled_invocation;

  precondition(message!=NULL);
  precondition(invocation!=NULL);

  marshalled_invocation=(marshalled_invocation_p)message_get_header(message);
  invocation->sender=marshalled_invocation->sender;
  invocation->instance=get_instance(marshalled_invocation->instance_id);
  assert(invocation->instance!=NULL);
  invocation->operation_id=marshalled_invocation->operation_id;
  opd = get_item(invocation->instance->po->op_table, invocation->operation_id);
  assert(opd != 0);
  args = marshalled_invocation->args;
  p=message_get_data(message);
  if (opunmarshall_func && opd->opdescr) {
	p = (*opunmarshall_func)(p, &(invocation->args), opd->opdescr,
			args, invocation->sender, invocation->instance);
  }
  else {
    invocation->args = (void **) malloc((opd->nparams+1) * sizeof(void *));
    for (i = 0; i < opd->nparams; i++) {
	int sz = opd->param_descrs[i].par_size;
	if (opd->param_descrs[i].par_flags & IN_PARAM) {
		if (sz == 0) {
			memcpy(&sz, p, sizeof(int));
		}
		invocation->args[i] = p;
		p = (void *)((char *)p + ((sz + 7) & ~7));
	}
	else {
		if (invocation->sender != me) {
			invocation->args[i] = malloc(sz);
		}
		else {
			invocation->args[i] = args[i];
		}
	}
    }
  }
  return 0;
}

/*=======================================================================*/
/* Marshalls an invocation of an operation into a buffer to be sent,
   totally ordered. */
/*=======================================================================*/
int 
marshall_invocation(int sender, instance_p instance, int opid, 
		    void **args, message_p operation_buffer)
{
  int i, sz;
  po_operation_p opd;
  marshalled_invocation_t marshalled_invocation;

  precondition(instance!=NULL);
  precondition(opid>=0);
  precondition(operation_buffer!=NULL);

  marshalled_invocation.sender=sender;
  marshalled_invocation.instance_id=instance->id;
  marshalled_invocation.operation_id=opid;
  marshalled_invocation.args=args;
  message_set_header(operation_buffer,&marshalled_invocation,
		     sizeof(marshalled_invocation_t));
  message_clear_data(operation_buffer);
  opd = get_item(instance->po->op_table, opid);
  if (opmarshall_func && opd->opdescr) {
    (*opmarshall_func)(operation_buffer, args, opd->opdescr);
  }
  else for (i = 0; i < opd->nparams; i++) {
    if (opd->param_descrs[i].par_flags & IN_PARAM) {
      sz = opd->param_descrs[i].par_size;
      if (sz == 0) {
	sz = *(int *)(args[i]);
      }
      message_append_data(operation_buffer, args[i], sz);
      if (sz != ((sz + 7) & ~7)) {
	double x = 0.0;
        message_append_data(operation_buffer, &x, ((sz + 7) & ~7) - sz);
      }
    }
  }
  return 0;
}

/*************************************************************************/
/*************************************************************************/
/* Transfer Handlers. */
/*************************************************************************/
/*************************************************************************/

/*=======================================================================*/
/*=======================================================================*/
void 
t_collective_caller(int sender, instance_p instance, po_opcode opcode, 
		    void **args) {

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(t_collective_caller_timer);
  if (instance->needs_sync) {
    int opid = operation_index(instance->po, (item_p) opcode);
    if (instance->optim_counter[opid]) {
      instance->needs_sync = FALSE;
      synch_invocation(instance);
    }
  }
  fetch_all(instance);
  access_all(instance);
  stop_timing(t_collective_caller_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
t_collective_all(int sender, instance_p instance, po_opcode opcode, 
		 void **args) {
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(t_collective_all_timer);
  if (instance->needs_sync) {
    instance->needs_sync = FALSE;
    synch_invocation(instance);
  }
  fetch_all(instance);
  access_all(instance);
  stop_timing(t_collective_all_timer);
}


/*=======================================================================*/
/*=======================================================================*/
void 
t_receiver(int sender, instance_p instance, po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(t_receiver_timer);
  if (instance->needs_sync) {
    int opid = operation_index(instance->po, (item_p) opcode);
    if (instance->optim_counter[opid]) {
      instance->needs_sync = FALSE;
      synch_invocation(instance);
    }
  }
  pdg_fetch(instance,opcode);
  stop_timing(t_receiver_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
t_sender(int sender, instance_p instance, po_opcode opcode, void **args) {

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  if (!(instance->parallel_update)) return;
  start_timing(t_sender_timer);
  if (instance->needs_sync) {
    int opid = operation_index(instance->po, (item_p) opcode);
    if (instance->optim_counter[opid]) {
      instance->needs_sync = FALSE;
      synch_invocation(instance);
    }
  }
  if (instance->parallel_update) {
  	pdg_send(instance,opcode);
  }
  stop_timing(t_sender_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
t_no_transfer(int sender, instance_p instance, po_opcode opcode, void **args) {
  return;
}


/*=======================================================================*/
/*=======================================================================*/

static void
dynamic_pdg(instance_p instance, int opid, po_opcode opcode,  void **args)
{
  po_operation_p po = get_item(instance->po->op_table, opid);
  pdg_p pdg;

  if (po->dyn_pdg) {
	pdg = (*(po->dyn_pdg))(instance, args);
	insert_pdg(instance, opcode, pdg);
  }
}

/*************************************************************************/
/*************************************************************************/
/* Execution Handlers. */
/*************************************************************************/
/*************************************************************************/


/*=======================================================================*/
/*=======================================================================*/
void 
e_sequential_local(int sender, instance_p instance, 
		   po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_sequential_local_timer);
  if (sender==me) {
    pdg_wait_all(instance,opcode);
    (*opcode)(sender,instance,args);
  }
  stop_timing(e_sequential_local_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
e_sequential_replicated(int sender, instance_p instance, 
			po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_sequential_replicated_timer);
  pdg_wait_all(instance,opcode);
  (*opcode)(sender, instance, args);
  stop_timing(e_sequential_replicated_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
e_parallel_blocking(int sender, instance_p instance, po_opcode opcode, 
		    void **args) 
{
  int p;
  int op_id;
  pdg_p pdg;
  po_operation_p opd;
  int x = 1;
  int with_rdeps, with_ldeps, *remote_deps, *local_deps;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_parallel_blocking_timer);
  op_id = operation_index(instance->po,(item_p)opcode);
  opd = get_item(instance->po->op_table, op_id);
  if (args) args[opd->nparams] = &x;

  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  remote_deps=get_remote_dependencies(pdg, &with_rdeps);
  local_deps=get_local_dependencies(pdg, &with_ldeps);

  /* Execute operation on partitions with no remote dependencies. */
  for (p=0; p<with_ldeps; p++) 
    (*opcode)(local_deps[p], instance, args);
  /* Execute operation on partitions with remote dependencies. */
  for (p=0; p<with_rdeps; p++) {
    /* Fetch remote dependencies. */
    pdg_fetch_part(instance,opcode,remote_deps[p]);
    pdg_wait_part(instance,opcode,remote_deps[p]);
    (*opcode)(remote_deps[p], instance, args);
  }
  stop_timing(e_parallel_blocking_timer);
}
  
/*=======================================================================*/
/* Assumes all data is already available and does not block waiting
   for partition dependencies. */
/*=======================================================================*/
void 
e_parallel_consistent(int sender, instance_p instance, po_opcode opcode, 
		      void **args) 
{
  int p;
  int part_num;
  int x = 1;
  po_operation_p opd;
  int op_i;
  
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_parallel_consistent_timer);
  op_i = operation_index(instance->po,(item_p)opcode);
  opd = get_item(instance->po->op_table, op_i);
  if (args) args[opd->nparams] = &x;

  for (p=0; p<instance->state->num_owned_parts; p++) {
    part_num=instance->state->owned_partitions[p]->part_num;
    (*opcode)(part_num, instance, args);
  }
  stop_timing(e_parallel_consistent_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
e_parallel_nonblocking(int sender, instance_p instance, po_opcode opcode, 
		       void **args) 
{
  register int p;
  int op_i;
  po_operation_p opd;
  pdg_p pdg;
  int x = 1;
  int visited;
  int with_rdeps, with_ldeps, *remote_deps, *local_deps;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_parallel_nonblocking_timer);
  visited=0;
  op_i = operation_index(instance->po,(item_p)opcode);
  opd = get_item(instance->po->op_table, op_i);
  if (args) args[opd->nparams] = &x;

  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  remote_deps=get_remote_dependencies(pdg, &with_rdeps);
  local_deps=get_local_dependencies(pdg, &with_ldeps);

  /* Execute operation on partitions with no remote dependencies. */
  for (p=0; p<with_ldeps; p++) {
    (*opcode)(local_deps[p], instance, args);
    visited++;
  }
  /* Execute operation on partitions with remote dependencies. */
  for (p=0; p<with_rdeps; p++) {
      /* Wait for dependencies. */
      pdg_wait_part(instance,opcode,remote_deps[p]);
      (*opcode)(remote_deps[p], instance, args);
      visited++;
    }
  assert(visited==instance->state->num_owned_parts);
  stop_timing(e_parallel_nonblocking_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
e_parallel_control(int sender, instance_p instance, po_opcode opcode, 
		   void **args) 
{
  int p;
  int op_i;
  pdg_p pdg;
  po_operation_p opd;
  int x = 1;
  int with_rdeps, with_ldeps, *remote_deps, *local_deps;
  
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(e_parallel_control_timer);
  op_i = operation_index(instance->po,(item_p)opcode);
  opd = get_item(instance->po->op_table, op_i);
  if (args) args[opd->nparams] = &x;

  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  remote_deps=get_remote_dependencies(pdg, &with_rdeps);
  local_deps=get_local_dependencies(pdg, &with_ldeps);

  /* Fetch dependencies for 1st partition with remote dependencies. */
  if (with_rdeps>0) pdg_fetch_part(instance,opcode,remote_deps[0]);
  /* Execute operation on partitions without remote dependencies. */
  for (p=0; p<with_ldeps; p++)
    (*opcode)(local_deps[p], instance, args);
  /* Execute operation on partitions with remote dependencies. */
  for (p=0; p<with_rdeps; p++)
    {
      /* Fetch dependencies for the next iteration. */
      if (p<(with_rdeps+1)) pdg_fetch_part(instance,opcode,remote_deps[p+1]);
      pdg_wait_part(instance,opcode,remote_deps[p]);
      (*opcode)(remote_deps[p], instance, args);
    }
  stop_timing(e_parallel_control_timer);
}

/*************************************************************************/
/*************************************************************************/
/* Return Handlers. */
/*************************************************************************/
/*************************************************************************/

/*=======================================================================*/
/*=======================================================================*/
void 
r_collective_caller(int sender, instance_p instance, po_opcode opcode, 
		    void **args) {
  int opid;  
  po_operation_p opd;
  int i, sz;
  int op = 0;

  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  start_timing(r_collective_caller_timer);
  opid=operation_index(instance->po,(item_p)opcode);
  assert(opid>=0);
  opd = get_item(instance->po->op_table, opid);
  assert(opd != 0);

  for (i = 0; i < opd->nparams; i++) {
    if (opd->param_descrs[i].par_flags & OUT_PARAM) {
      sz = opd->param_descrs[i].par_size;
      if (sz == 0) {
	sz = *(int *)(args[i]);
      }
      /* sz = (sz + 7) & ~7;  WHY WAS THIS HERE??? */
      if (is_reduce(&(opd->param_descrs[i]))) {
	reduce(instance->return_ch,sender,args[i],sz,args[i],sz,opd->param_descrs[i].par_func);
      }
      else if (is_gather(&(opd->param_descrs[i]))) {
	gather(instance->return_ch,sender,args[i],sz,args[i],sz);
      }
      collection_wait(instance->return_ch);
      op = 1;
    }
  }
  if ((! op) && (instance->sum_optim_counter>0)) {
    instance->needs_sync = TRUE;
  }
  else if (instance->id==0) 
    synch_invocation(instance);
  stop_timing(r_collective_caller_timer);
}

/*=======================================================================*/
/*=======================================================================*/
void 
r_local(int sender, instance_p instance, po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  return;
}

/*************************************************************************/
/*************************************************************************/
/* Commit Handlers. */
/*************************************************************************/
/*************************************************************************/

/*=======================================================================*/
/*=======================================================================*/
void 
c_commit_owned(int sender, instance_p instance, po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);

  start_timing(c_commit_owned_timer);
  invalidate_all(instance); 
  commit_owned_partitions(instance);
  change_color(instance);
  stop_timing(c_commit_owned_timer);
}


/*=======================================================================*/
/*=======================================================================*/
void 
c_save_all(int sender, instance_p instance, po_opcode opcode, void **args)
{
  precondition(instance!=NULL);
  precondition(opcode!=NULL);

  start_timing(c_save_all_timer);
  save_owned_partitions(instance);
  stop_timing(c_save_all_timer);
}


/*=======================================================================*/
/*=======================================================================*/
void 
c_no_commit(int sender, instance_p instance, po_opcode opcode, void **args) {
  return;
}


/*=======================================================================*/
/* Synchronizes all invocation handlers on all platforms. */
/*=======================================================================*/

int 
synch_handlers(void) {
  barrier(barrier_channel);
  collection_wait(barrier_channel);
  return 0;
}

/*=======================================================================*/
/* Synchronizes all invocation handlers on platforms where an
   operation on instance is being executed. */
/*=======================================================================*/
int 
synch_invocation(instance_p instance) {
  barrier(instance->synch_ch);
  collection_wait(instance->synch_ch);
  return 0;
}


/*=======================================================================*/
/* Blocks until all invocations issues so far end. */
/*=======================================================================*/
int 
wait_for_end_of_invocation(instance_p instance)
{
  await_condition(instance->no_local_invocation);
  signal_condition(instance->no_local_invocation);
  return 0;
}


/*=======================================================================*/
/* Copies all owned partitions from the temporary buffer
   (instance->state->owned_elements) to the state of the instance
   (instance->state->data). */
/*=======================================================================*/
int 
commit_owned_partitions(instance_p instance)
{
  register int p;
  register int part_num;

  precondition(instance!=0);

  if (instance->id!=0)
    for (p=0; p<instance->state->num_owned_parts; p++) {
      part_num=instance->state->owned_partitions[p]->part_num;
      commit_partition(instance,
		       instance->state->data,
		       instance->state->partition[part_num]->elements,
		       part_num);
    }
  return 0;
}

/*=======================================================================*/
/* Saves all owned partitions. */
/*=======================================================================*/
int 
save_owned_partitions(instance_p instance)
{
  int p;
  int part_num;

  precondition(instance!=NULL);
  
  if (instance->id!=0)
    for (p=0; p<instance->state->num_owned_parts; p++)
      {
	part_num=instance->state->owned_partitions[p]->part_num;
	save_partition(instance, instance->state->data,
		       instance->state->partition[part_num]->elements,
		       part_num);
      }
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/

int 
marshalled_invocation_size() {
  return sizeof(marshalled_invocation_t);
}


/*=======================================================================*/
/*=======================================================================*/
int 
get_handler_id(handler_p handler) {
  return item_index(handler_table,(item_p )handler);
}

/*=======================================================================*/
/*=======================================================================*/
handler_p 
get_handler_p(int handler) {
  return (handler_p )get_item(handler_table,handler);
}

/*=======================================================================*/
/*=======================================================================*/
int 
register_handlers(void) 
{
  insert_item(handler_table,(item_p )noop);
  insert_item(handler_table,(item_p )i_local);
  insert_item(handler_table,(item_p )i_collective);
  insert_item(handler_table,(item_p )i_multicast);
  insert_item(handler_table,(item_p )t_collective_caller);
  insert_item(handler_table,(item_p )t_collective_all);
  insert_item(handler_table,(item_p )t_receiver);
  insert_item(handler_table,(item_p )t_sender);
  insert_item(handler_table,(item_p )t_no_transfer);
  insert_item(handler_table,(item_p )e_sequential_local);
  insert_item(handler_table,(item_p )e_sequential_replicated);
  insert_item(handler_table,(item_p )e_parallel_blocking);
  insert_item(handler_table,(item_p )e_parallel_consistent);
  insert_item(handler_table,(item_p )e_parallel_nonblocking);
  insert_item(handler_table,(item_p )e_parallel_control);
  insert_item(handler_table,(item_p )r_collective_caller);
  insert_item(handler_table,(item_p )r_local);
  insert_item(handler_table,(item_p )c_commit_owned);
  insert_item(handler_table,(item_p )c_save_all);
  insert_item(handler_table,(item_p )c_no_commit);
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
void 
initialize_timers(void) {
  i_local_timer=new_po_timer("LocalInvocation");
  i_collective_timer=new_po_timer("CollectiveInvocation");
  i_multicast_timer=new_po_timer("MulticastInvocation");

  t_collective_all_timer=new_po_timer("CollectiveAllTransfer");
  t_collective_caller_timer=new_po_timer("CollectiveCallerTransfer");
  t_receiver_timer=new_po_timer("ReceiverTransfer");
  t_sender_timer=new_po_timer("SenderTransfer");

  e_sequential_local_timer=new_po_timer("SequentialLocalExecution");
  e_sequential_replicated_timer=new_po_timer("SequentialReplicatedExecution");
  e_parallel_blocking_timer=new_po_timer("ParallelBlockingExecution");
  e_parallel_consistent_timer=new_po_timer("ParallelConsistentExecution");
  e_parallel_nonblocking_timer=new_po_timer("ParallelNonblockingExecution");
  e_parallel_control_timer=new_po_timer("ParallelControlExecution");

  r_collective_caller_timer=new_po_timer("CollectiveCallerReturn");

  c_commit_owned_timer=new_po_timer("CommitOwnedCommit");
  c_save_all_timer=new_po_timer("SaveAllCommit");
}

void 
free_timers(void) 
{
    free_po_timer(i_local_timer);
    free_po_timer(i_collective_timer);
    free_po_timer(i_multicast_timer);

    free_po_timer(t_collective_all_timer);
    free_po_timer(t_collective_caller_timer);
    free_po_timer(t_receiver_timer);
    free_po_timer(t_sender_timer);

    free_po_timer(e_sequential_local_timer);
    free_po_timer(e_sequential_replicated_timer);
    free_po_timer(e_parallel_blocking_timer);
    free_po_timer(e_parallel_consistent_timer);
    free_po_timer(e_parallel_nonblocking_timer);
    free_po_timer(e_parallel_control_timer);

    free_po_timer(r_collective_caller_timer);

    free_po_timer(c_commit_owned_timer);
    free_po_timer(c_save_all_timer);
}
