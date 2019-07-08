#ifndef __instance__
#define __instance__

#include "collection.h"
#include "consistency.h"
#include "sys_po.h"
#include "state.h"
#include "pdg.h"
#include "communication.h"
#include "condition.h"
#include "util.h"

typedef enum
{
  CYCLIC,
  BLOCK
} dist_t;

typedef enum 
{ 
  ROWWISE, 
  COLUMNWISE, 
  ANY
} partitioning_t;

struct instance_s {
  po_p po;
  int id;
  int sum_optim_counter; /* If this one is zero, the RTS applies some optimizations
			    (Removes the barrier at the end of each operation. */
  int *optim_counter; 
  grp_channel_p gch;   /* Used to send an invocation to processors
			  holding part of the state of the
			  object. Only those will receive the
			  invocation. */
  mp_channel_p fch;
  state_p state;
  handlers_p handlers; /* One set of handlers for each operation. */
  consistency_p consistency;
  map_p pdg_table;
  set_p processors;
  collection_p synch_ch, return_ch;
  partitioning_t partitioning;
  message_p invocation_message;
  condition_p no_local_invocation;
  boolean parallel_update; /* Specifies whether there was any parallel
			      update operation executed on the
			      instance. If not, the entire state of
			      the object is consistent on all
			      processors. */
  boolean needs_sync;
};

int init_instance_module(int me, int group_size, int pdebug, int *argc, char **argv);
int finish_instance_module(void);

instance_p new_instance(po_p po, int *length, int *base);
int operation_attributes(instance_p poi, 
			 handlers_t handlers, po_opcode opcode);
int change_attribute(instance_p poi, po_opcode opcode, 
		     attribute_t attribute, handler_p handler);

int free_instance(instance_p instance);
void compute_optim_counter(instance_p poi);

int rowwise_partitioning(instance_p instance);
int columnwise_partitioning(instance_p instance);
int free_partitioning(instance_p instance);
int my_partitioning(instance_p instance, int *num_parts);

int block_distribution(instance_p instance, set_p processors);
int cyclic_distribution(instance_p instance, set_p processors);
int partition(instance_p instance, int *indices);
int my_distribution(instance_p instance, int *owner);

int distribute_on_n(instance_p instance, int **dist);
int distribute_on_list(instance_p instance, int num_proc, 
		       int *proc_list, int **dist);
 
int change_color(instance_p instance);

instance_p get_instance(int i_id);
int set_pdg(instance_p instance, po_opcode opcode, pdg_p pdg);
pdg_p get_pdg(instance_p instance, po_opcode opcode);
#endif

