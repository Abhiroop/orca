#include <stdlib.h>
#include <string.h>

#include "instance.h"
#include "po_invocation.h"
#include "misc.h"
#include "space.h"
#include "precondition.h"
#include "timeout.h"
#include "grp_channel.h"
#include "assert.h"

#define MODULE_NAME "INSTANCE"
#define MIN_TIMEOUT  2000
#ifdef RELIABLE_COMM
#define INT_TIMEOUT  500000
#define MAX_TIMEOUT  10000000
#else
#define INT_TIMEOUT  2000
#define MAX_TIMEOUT  10000
#endif

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static map_p instance_table;

static int distribute_one_processor_case(instance_p instance);
static int partition_one_processor_case(instance_p instance);
static state_p new_state(instance_p instance, int *length, int *base);
static int free_state(instance_p instance);
static int new_elements(instance_p instance, set_p processors);
static int create_pdgs(instance_p instance);
static int free_elements(instance_p instance);

static int proc_num(int part, int NumParts, int NumProcs, int dist);
static int block_cpu(int part, int NumLargerParts, int Partitionsz);
static int cyclic_cpu(int part, int NumProcs);
static int cpuindex_to_cpu(int *cpu_index, int *dist, int num_dims);

int 
block_cpu(int part, int NumLargerParts, int Partitionsz) {
  int startsmallparts;

  if (NumLargerParts == 0) {
	/* Probably most common case. */
	return part/Partitionsz;
  }
  /* Here, we start with NumLargerParts of size Partitionsz+1 and then have
     a couple of smaller partitions (of size Partitionsz).
  */
  startsmallparts = NumLargerParts * (Partitionsz+1);

  if (part < startsmallparts) {
	/* Still within the first NumLargerParts partitions. */
	return part/(Partitionsz+1);
  }
  return (part - startsmallparts) / Partitionsz + NumLargerParts;
}

int 
cyclic_cpu(int part, int NumProcs) {
  return part % NumProcs;
}

int
proc_num(int part, int NumParts, int NumProcs, int dist) {
  if (dist==BLOCK) {
    return block_cpu(part,NumParts%NumProcs,NumParts/NumProcs);
  }
  else {
    assert(dist==CYCLIC);
    return cyclic_cpu(part,NumProcs);
  }
}

int 
cpuindex_to_cpu(int *cpu_index, int *dist, int num_dims) {
  int cpu, i;
  
  /* Some loop unrolling for the most common cases */
  switch(num_dims) {
  case 1:
	return cpu_index[0];
  case 2:
	return cpu_index[1] * dist[0] + cpu_index[0];
  default:
	cpu=cpu_index[num_dims-1];
  	for (i=num_dims-2; i>=0; i--)
	    cpu = cpu*dist[i] + cpu_index[i];
  	return cpu;
  }
  /* NOTREACHED */
}

/*=======================================================================*/
/* Given the index of an element, this procedure returns the partition
   the element belongs to. The state must have been partitioned before
   this function can be called. */
/*=======================================================================*/
int
partition(instance_p instance, int *indices) {
  int i;
  state_p state = instance->state;

  precondition(instance!=NULL);
  precondition(indices!=NULL);
  
  /* if (group_size==1) return 0; */
  switch(instance->partitioning) {
  case ROWWISE:
    indices[0] -= state->start[0];
    if (indices[0] >= 0 && indices[0] < state->length[0]) return indices[0]; 
    return -1;
  case COLUMNWISE:
    indices[1] -= state->start[1];
    if (indices[1] >= 0 && indices[1] < state->length[1]) return indices[1];
    return -1;
  default:
    /* Some loop unrolling for the most common cases. */
    switch(instance->po->num_dims) {
    case 1:
	i = indices[0] - state->start[0];
        if (i <  0 || i >= state->length[0]) return -1;
	return block_cpu(i, state->num_larger_parts[0],
		state->part_size[0]);

    case 2:
	indices[0] -= state->start[0];
	indices[1] -= state->start[1];
        if (indices[0] <  0 || indices[0] >= state->length[0]) return -1;
        if (indices[1] <  0 || indices[1] >= state->length[1]) return -1;
	indices[0] = block_cpu(indices[0], state->num_larger_parts[0],
		state->part_size[0]);
	indices[1] = block_cpu(indices[1], state->num_larger_parts[1],
		state->part_size[1]);
	return indices[1] * state->num_part_indices[0] + indices[0];

    default:
      for (i=0; i<instance->po->num_dims; i++) {
	indices[i] -= state->start[i];
        if (indices[i] < 0 || indices[i] >= state->length[i]) return -1;
        indices[i]=block_cpu(indices[i], state->num_larger_parts[i],
			state->part_size[i]);
      }
      i = cpuindex_to_cpu(indices,state->num_part_indices,
			    instance->po->num_dims);
      return i;
    }
  }
  /* NOTREACHED */
}

/*=======================================================================*/
/* Initialized the module. */
/*=======================================================================*/

int 
init_instance_module(int moi, int gsize, int pdebug, int *argc, char **argv) {
  precondition((moi>=0)&&(moi<gsize));

  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_map(me,group_size,proc_debug);
  instance_table=new_map();
  sys_error(instance_table==NULL);
  init_condition(me,group_size,proc_debug);
  init_space_module(me,group_size,proc_debug, argc, argv);
  return 0;
}


/*=======================================================================*/
/* Finishes the module. */
/*=======================================================================*/

int 
finish_instance_module(void) {
  if (--initialized) return 0;
  finish_space_module();
  finish_condition();
  free_map(instance_table);
  finish_map();
  return 0;
}

/*=======================================================================*/
/* Creates a new instance of an object. */
/*=======================================================================*/

instance_p 
new_instance(po_p po, int *length, int *base)
{
  instance_p poi;
  int i;
  po_operation_p po_op;

  precondition(po!=NULL);
  precondition(length!=NULL);

  poi=(instance_p)malloc(sizeof(instance_t));
  sys_error(poi==NULL);
  poi->po=po;
  poi->state=new_state(poi,length,base);
  poi->pdg_table=new_map();
  poi->processors=NULL;
  poi->handlers
    =(handlers_p )malloc(sizeof(handlers_t)*num_items(po->op_table));
  poi->optim_counter=(int *)malloc(sizeof(int)*num_items(po->op_table));
  for (i=0; i<num_items(po->op_table); i++) {
    po_op=(po_operation_p)get_item(po->op_table,i);
    memcpy(&(poi->handlers[i]),&(po_op->handlers),sizeof(handlers_t));
    poi->optim_counter[i] = po_op->optim_counter;
  }
  poi->id=insert_item(instance_table,(item_p)poi);
  assert(poi->id>=0);
  poi->partitioning=ANY;
  poi->invocation_message=new_message(marshalled_invocation_size(),0);
  poi->no_local_invocation=new_condition();
  poi->parallel_update=FALSE;

  poi->gch = NULL;
  poi->fch = NULL;
  poi->needs_sync = FALSE;
  compute_optim_counter(poi);
  return poi;
}

/*=======================================================================*/
/*=======================================================================*/
void
compute_optim_counter(instance_p poi) {
  int i;

  poi->sum_optim_counter = 0;
  for (i=0; i<num_items(poi->po->op_table); i++)
    poi->sum_optim_counter += poi->optim_counter[i];
}

/*=======================================================================*/
/* Changes the operation attributes for an operation. */
/*=======================================================================*/

int 
operation_attributes(instance_p poi, handlers_t handlers, po_opcode opcode)
{
  int opid;

  opid=item_index(poi->po->opcode_table,(item_p)opcode);
  assert(opid>=0);
  memcpy(&(poi->handlers[opid]),&handlers,sizeof(handlers_t));
  return 0;
}

/*=======================================================================*/
/* Changes one attribute for an operation. */
/*=======================================================================*/

int 
change_attribute(instance_p poi, po_opcode opcode, attribute_t attribute, 
		 handler_p handler)
{
  int opid;

  opid=item_index(poi->po->opcode_table,(item_p)opcode);
  assert(opid>=0);
  switch(attribute)
    {
    case INVOCATION:
      poi->handlers[opid].i_handler=handler;
      break;
    case TRANSFER:
      poi->handlers[opid].t_handler=handler;
      if ((handler==noop) || (handler==t_collective_all))
	poi->optim_counter[opid]=0;
      break;
    case EXECUTION:
      poi->handlers[opid].e_handler=handler;
      break;
    case RETURN:
      poi->handlers[opid].r_handler=handler;
      break;
    case COMMIT:
      poi->handlers[opid].c_handler=handler;
      break;
    default: return -1;
    }
  return 0;
}

/*=======================================================================*/
/* Deletes an instance of an object. */
/*=======================================================================*/

int 
free_instance(instance_p instance) {
  int r;

  precondition(instance!=NULL);

  r=remove_item(instance_table,instance->id);
  assert(r>=0);
  free_elements(instance);
  if (instance->state!=NULL) free_state(instance);
  if (instance->fch!=NULL) free_mp_channel(instance->fch);
  free(instance->handlers);
  free_map(instance->pdg_table);
  free_message(instance->invocation_message);
  free_condition(instance->no_local_invocation);

  if (instance->processors) free_set(instance->processors);
  if (instance->gch) { free_grp_channel(instance->gch); instance->gch=NULL; }
  free(instance->optim_counter);
  free(instance);
  return 0;
}


/*=======================================================================*/
/* Creates a state structure for an object. */
/*=======================================================================*/

state_p 
new_state(instance_p instance, int *length, int *base)
{
  state_p state;
  int i;

  precondition(instance!=NULL);
  precondition(length!=NULL);

  state=(state_p )malloc(sizeof(state_t));
  assert(state!=NULL);
  state->instance=instance;
  state->length=(int *)malloc(sizeof(int)*instance->po->num_dims);
  state->start=(int *)malloc(sizeof(int)*instance->po->num_dims);
  state->end=(int *)malloc(sizeof(int)*instance->po->num_dims);
  memcpy(state->length, length, sizeof(int)*instance->po->num_dims);
  if (base) {
    for (i=0; i<instance->po->num_dims; i++) {
      state->start[i] = base[i];
      state->end[i] = base[i] + length[i] - 1;
    }
  }
  else {
    for (i=0; i<instance->po->num_dims; i++) {
      state->start[i] = 0;
      state->end[i] = length[i] - 1;
    }
  }
  state->total_size=1;
  for (i=0; i<instance->po->num_dims; i++) 
    state->total_size=state->total_size*length[i];
  state->total_size=state->total_size*instance->po->element_size;
  state->num_parts=-1;
  state->num_part_indices=(int *)malloc(sizeof(int)*instance->po->num_dims);
  state->part_size = (int *)malloc(sizeof(int)*instance->po->num_dims);
  state->num_larger_parts = (int *)malloc(sizeof(int)*instance->po->num_dims);
  state->num_owned_parts=-1;
  state->owner=NULL;
  state->order=NULL;
  state->owned_elements=NULL;
  state->partition=NULL;
  state->owned_partitions=NULL;
  instance->state=state;
  return state;
}


/*=======================================================================*/
/* Frees a state structure for an object. */
/*=======================================================================*/
 
int 
free_state(instance_p instance)
{
  int i;
  state_p state;

  precondition(instance!=NULL);
 
  state=instance->state;
  free(state->length);
  free(state->start);
  free(state->end);
  if (state->owner!=NULL) free(state->owner);
  if (state->order!=NULL) free(state->order);
  if (state->num_part_indices) free(state->num_part_indices);
  if (state->part_size) free(state->part_size);
  if (state->num_larger_parts) free(state->num_larger_parts);
  if (state->owned_partitions) free(state->owned_partitions);
  if (state->partition!=NULL)
    {
      for (i=0; i<state->num_parts; i++)
	{
	  free_partition(state->partition[i]);
	}
      free(state->partition);
    }
  free(state);
  return 0;
}

int 
distribute_one_processor_case(instance_p instance) {
  state_p state;

  precondition(instance!=NULL);
  precondition(group_size==1);
  
  state=instance->state;
  state->owner=(int *)malloc(sizeof(int));
  state->owner[0]=me;
  instance->processors=new_set(group_size);
  full_set(instance->processors);
  assert(instance->processors->max_members==1);
  state->num_owned_parts=1;
  new_elements(instance, instance->processors);

  /* Prepare pointers to owned partitions. */
  state->owned_partitions=(partition_p *)malloc(sizeof(partition_p));
  state->owned_partitions[0]=state->partition[0];
  instance->synch_ch = new_collection(instance->processors);
  coll_add_instance(instance->synch_ch, instance);
  instance->return_ch = new_collection(instance->processors);  
  coll_add_instance(instance->return_ch, instance);
  create_pdgs(instance);
  return 0;  
}

int
partition_one_processor_case(instance_p instance) {
  int *start;
  int *end;
  state_p state;
  int *part_indices;
  int i;

  precondition(instance!=NULL);
  precondition(instance->state!=NULL);
  precondition(group_size==1);
  
  state=instance->state;
  part_indices=(int *)malloc(sizeof(int)*instance->po->num_dims);
  for (i=0; i<instance->po->num_dims; i++) state->num_part_indices[i]=1;
  for (i=0; i<instance->po->num_dims; i++) state->part_size[i]=state->length[i];
  for (i=0; i<instance->po->num_dims; i++) state->num_larger_parts[i]=0;
  memset(part_indices,0,sizeof(int)*instance->po->num_dims);
  state=instance->state;
  state->partition=(partition_p *)malloc(sizeof(partition_p));
  start=(int *)malloc(sizeof(int)*instance->po->num_dims);
  end=(int *)malloc(sizeof(int)*instance->po->num_dims);
  memcpy(start,state->start,sizeof(int)*instance->po->num_dims);
  memcpy(end,state->end,sizeof(int)*instance->po->num_dims);
  state->partition[0]=new_partition(0,part_indices,
				    instance->po->element_size,
				    instance->po->num_dims,
				    start, end, state->length);
  state->num_parts=1;
  instance->partitioning=ANY;
  free(part_indices);
  return 0;
}

/*=======================================================================*/
/* Partitions the state of an object rowwise. This procedure assumes a
   2 dimensional state. */
/*=======================================================================*/
int 
rowwise_partitioning(instance_p instance)
{
  int start[2];
  int end[2];
  int i;
  state_p state;
  int *part_indices;

  precondition(instance!=NULL);
  precondition(instance->state!=NULL);
  precondition(instance->po->num_dims==2);
 
  if (group_size==1) 
    return partition_one_processor_case(instance);

  part_indices=(int *)malloc(sizeof(int)*instance->po->num_dims);
  state=instance->state;
  state->num_part_indices[0] = state->length[0];
  state->part_size[0] = 1;
  state->num_larger_parts[0] = 0;
  state->num_part_indices[1] = 1;
  state->part_size[1] = state->length[1];
  state->num_larger_parts[1] = 0;
  part_indices[1]=0;
  state=instance->state;
  state->partition=(partition_p *)malloc(sizeof(partition_p)*state->length[0]);
  start[1]=state->start[1];
  end[1]=state->end[1];
  for (i=0; i<state->length[0]; i++) {
    part_indices[0]=i;
    start[0]=state->start[0]+i;
    end[0]=state->start[0]+i;
    state->partition[i]=new_partition(i,part_indices,
				      instance->po->element_size,
				      instance->po->num_dims,
				      start, end, state->length);
  }
  state->num_parts=state->length[0];
  instance->partitioning=ROWWISE;
  free(part_indices);
  return 0;
}
 
/*=======================================================================*/
/* Partitions the state of an object columnwise. This procedure assumes a
   2 dimensional state. */
/*=======================================================================*/
int 
columnwise_partitioning(instance_p instance)
{
  int start[2];
  int end[2];
  int i;
  state_p state;
  int *part_indices;

  precondition(instance!=NULL);
  precondition(instance->state!=NULL);
  precondition(instance->po->num_dims==2);
 
  if (group_size==1) 
    return partition_one_processor_case(instance);
  
  state=instance->state;
  part_indices=(int *)malloc(sizeof(int)*instance->po->num_dims);
  part_indices[0]=0;
  state->num_part_indices[1] = state->length[1];
  state->part_size[1] = 1;
  state->num_larger_parts[1] = 0;
  state->num_part_indices[0] = 1;
  state->part_size[0] = state->length[0];
  state->num_larger_parts[0] = 0;
  state=instance->state;
  state->partition=(partition_p *)malloc(sizeof(partition_p)*state->length[1]);
  start[0]=state->start[0];
  end[0]=state->end[0];
  for (i=0; i<state->length[1]; i++) 
    {
      part_indices[1]=i;
      start[1]=state->start[1]+i;
      end[1]=state->start[1]+i;
      state->partition[i]=new_partition(i,part_indices,
					instance->po->element_size,
					instance->po->num_dims,
					start, end, state->length);
    }
  state->num_parts=state->length[1];
  instance->partitioning=COLUMNWISE;
  free(part_indices);
  return 0;
}
 
/* Partitions the state of an object according to the user's
   specifications. The user gives the number of partitions along each 
   dimension. */
/*=======================================================================*/
int 
my_partitioning(instance_p instance, int *parts)
{
  state_p state;
  int **en, **st, **part_indices;
  int step, i, j, s, e, p, dim;
  int modop, divop;
  
  precondition(instance!=NULL);
  precondition(instance->state!=NULL);
  
  if (group_size==1) 
    return partition_one_processor_case(instance);
        
  state=instance->state;

  if (instance->po->num_dims == 2) {
    if (parts[0] == state->length[0] && parts[1] == 1) {
	return rowwise_partitioning(instance);
    }
    if (parts[1] == state->length[1] && parts[0] == 1) {
	return columnwise_partitioning(instance);
    }
  }
  memcpy(state->num_part_indices, parts, sizeof(int)*instance->po->num_dims);
  state->num_parts=1;
  for (i=0; i<instance->po->num_dims; i++) {
    state->num_parts *= parts[i];
    state->part_size[i] = state->length[i] / parts[i];
    state->num_larger_parts[i] = state->length[i] - state->part_size[i] * parts[i];
  }
  state->partition=(partition_p *)malloc(sizeof(partition_p)*state->num_parts);
  en=(int **)malloc(sizeof(int *)*state->num_parts);
  st=(int **)malloc(sizeof(int *)*state->num_parts);
  part_indices=(int **)malloc(sizeof(int *)*state->num_parts);
  for (i=0; i<state->num_parts; i++) {
    st[i]=(int *)malloc(sizeof(int)*instance->po->num_dims);
    en[i]=(int *)malloc(sizeof(int)*instance->po->num_dims);
    part_indices[i]=(int *)malloc(sizeof(int)*instance->po->num_dims);
  }

  /* Determine start and end elements of each partition. */
  step =1;
  for (dim=0; dim<instance->po->num_dims; dim++) {
    modop=state->length[dim] % parts[dim];
    divop=state->length[dim] / parts[dim];
    i=0; p=0; 
    while (i<state->num_parts) {
      s=(((p)<modop) ? (p)*divop +p : (p)*divop + modop);
      e=(((p+1)<modop) ? (p+1)*divop +p+1 : (p+1)*divop + modop) -1;
      for (j=0; j<step; j++) {
      st[i+j][dim] = s+state->start[dim];
      en[i+j][dim] = e+state->start[dim];
      part_indices[i+j][dim] = p;
      }
      i += step;
      p = (p+1) % parts[dim];
    }
    step *= parts[dim];
  }

  /* Create the partitions. */
  for (i=0; i<state->num_parts; i++) 
    state->partition[i]=new_partition(i,part_indices[i],
                                    instance->po->element_size,
                                    instance->po->num_dims,
                                    st[i], en[i], state->length);
  for (i=0; i<state->num_parts; i++) {
    free(st[i]);
    free(en[i]);
    free(part_indices[i]);
  }
  free(part_indices);
  free(st);
  free(en);
  return 0;
}
 

/*=======================================================================*/
/* Frees space allocated for partitions description. */
/*=======================================================================*/

int 
free_partitioning(instance_p instance)
{
  state_p state;
  int i;

  precondition(instance!=NULL);
  precondition(instance->state!=NULL);

  state=instance->state;
  if (state->partition!=NULL)
    {
      for (i=0; i<state->num_parts; i++)
	free_partition(state->partition[i]);
      free(state->partition);
      if (state->owner!=NULL) free(state->owner);
    }
  return 0;
}


/*=======================================================================*/
/* Even partitioning: the elements are distributed evenly among the
   processors, in block. */
/*=======================================================================*/
int 
my_distribution(instance_p instance, int *owner)
{
  int i, j;
  state_p state;

  precondition(instance!=NULL);
  precondition(owner!=NULL);

  if (group_size==1) 
    return distribute_one_processor_case(instance);

  state=instance->state;
  state->owner=(int *)malloc(sizeof(int)*state->num_parts);
  memcpy(state->owner, owner, sizeof(int)*state->num_parts);
  instance->processors=new_set(group_size);
  state->num_owned_parts=0;
  for (i=0; i<state->num_parts; i++)
    {
      add_member(instance->processors,owner[i]);
      if (owner[i]==me)
	state->num_owned_parts++;
    }

  new_elements(instance, instance->processors);

  /* Prepare pointers to owned partitions. */
  state->owned_partitions=
    (partition_p *)malloc(sizeof(partition_p)*state->num_owned_parts);
  for (i=0, j=0; i<state->num_parts; i++)
    if (state->owner[i]==me) {
      state->owned_partitions[j]=state->partition[i];
      j++;
    }
  instance->synch_ch = new_collection(instance->processors);
  coll_add_instance(instance->synch_ch, instance);
  instance->return_ch = new_collection(instance->processors);  
  coll_add_instance(instance->return_ch, instance);
  create_pdgs(instance);
  return 0;  
}

int
distribute_on_n(instance_p instance, int **dist) {
  int *owner;
  int *cpuindex;
  int d, p;

  owner=(int *)malloc(sizeof(int)*instance->state->num_parts);
  cpuindex=(int *)malloc(sizeof(int)*instance->po->num_dims);
  /* Determine distribution. */
  for (p=0; p<instance->state->num_parts; p++) {
    for (d=0; d<instance->po->num_dims; d++)
      cpuindex[d]=proc_num(instance->state->partition[p]->part_indices[d],
			   instance->state->num_part_indices[d],
			   dist[0][d], dist[1][d]);
    owner[p] = cpuindex_to_cpu(cpuindex, dist[0], instance->po->num_dims);
  }

  /* Call my_distribution(). */
  my_distribution(instance,owner);
  free(cpuindex);
  free(owner);
  return 0;
}

int
distribute_on_list(instance_p instance, int num_proc, int *proc_list, int **dist) {
  int *owner;
  int *cpuindex;
  int d, p;

  precondition(num_proc<=group_size);

  for (d=0; d<instance->po->num_dims; d++)
    assert(dist[0][d] <= instance->state->num_part_indices[d]);
  owner=(int *)malloc(sizeof(int)*instance->state->num_parts);
  cpuindex=(int *)malloc(sizeof(int)*instance->po->num_dims);
  /* Determine distribution. */
  for (p=0; p<instance->state->num_parts; p++) {
    for (d=0; d<instance->po->num_dims; d++)
      cpuindex[d]=proc_num(instance->state->partition[p]->part_indices[d],
			   instance->state->num_part_indices[d],
			   dist[0][d], dist[1][d]);
    owner[p] = proc_list[cpuindex_to_cpu(cpuindex, dist[0], instance->po->num_dims)];
  }

  /* Call my_distribution(). */
  my_distribution(instance,owner);
  free(cpuindex);
  free(owner);
  return 0;
}

/*=======================================================================*/
/* Distributes a 1 dimensional partitioning in blocks. The procedure
   allocates an approximately equal number of partitions to each
   processor. */
/*=======================================================================*/
 
int 
block_distribution(instance_p instance, set_p processors)
{
  int modop, divop;
  int proc, i, j;
  int num_part, first_part;
  state_p state;
  int *processor_list;

  precondition(instance!=NULL);
  precondition(instance->state!=NULL);

  if (group_size==1) 
    return distribute_one_processor_case(instance);

  state=instance->state;
  state->owner=(int *)malloc(sizeof(int)*state->num_parts);
  modop=state->num_parts % processors->num_members;
  divop=state->num_parts / processors->num_members;
  processor_list=list_of_members(processors);
  state->num_owned_parts=0;

  for (i=0; i<processors->num_members; i++)
    {
      proc=processor_list[i];
      num_part=((i < modop) ? divop+1 : divop);
      first_part=((i < modop) ? i*divop + i : i*divop + modop);
      for (j=first_part; j<(first_part + num_part); j++)
	{
	  state->owner[j]=proc;
	  if (proc==me) state->num_owned_parts++;
	}
    }

  instance->processors=duplicate_set(processors);
  new_elements(instance, instance->processors);
  
  /* Prepare pointers to owned partitions. */
  state->owned_partitions=
    (partition_p *)malloc(sizeof(partition_p)*state->num_owned_parts);
  for (i=0, j=0; i<state->num_parts; i++)
    if (state->owner[i]==me)
      {
	state->owned_partitions[j]=state->partition[i];
	j++;
      }

  free(processor_list);
  instance->synch_ch = new_collection(instance->processors);
  coll_add_instance(instance->synch_ch, instance);
  instance->return_ch = new_collection(instance->processors);  
  coll_add_instance(instance->return_ch, instance);
  create_pdgs(instance);
  return 0;
}


/*=======================================================================*/
/* Build dependency graphs for all operations of an instance. Must be
   called after all operations on the object have been registered and
   after instance has been partitioned and distributed. */
/*=======================================================================*/

int 
create_pdgs(instance_p instance) 
{
  int i;
  po_operation_p method;
  pdg_p pdg;

  precondition(instance!=NULL);
  
  for (i=0; i<max_items(instance->po->op_table); i++) {
    method=get_item(instance->po->op_table,i);
    if (method != 0 &&
	instance->state->num_parts == instance->state->num_owned_parts) {
      change_attribute(instance, method->opcode, EXECUTION, e_parallel_consistent);
      change_attribute(instance, method->opcode, TRANSFER, noop);
      change_attribute(instance, method->opcode, INVOCATION, i_local);
      change_attribute(instance, method->opcode, RETURN, r_local);
    }
    else if ((method!=NULL) && (method->build_pdg!=NULL)) {
      pdg=(*(method->build_pdg))(instance);
      if (analyze_pdg(pdg) == 0) {
        set_pdg(instance,method->opcode,pdg);
        build_deps(pdg, instance->state->owner);
        if (pdg->with_rdeps==0) {
	  change_attribute(instance, method->opcode, EXECUTION, e_parallel_consistent);
	  change_attribute(instance, method->opcode, TRANSFER, noop);
	  instance->optim_counter[i] = 0;
        }
        else {
	  change_attribute(instance, method->opcode, EXECUTION, e_parallel_nonblocking);
	  change_attribute(instance, method->opcode, TRANSFER, t_sender);
	  instance->optim_counter[i] = 1;
        }  
      }
      else {
	change_attribute(instance, method->opcode, EXECUTION, e_parallel_consistent);
	change_attribute(instance, method->opcode, TRANSFER, t_collective_all);
	free_pdg(pdg);
      }
    }
  }
  compute_optim_counter(instance);
  return 0;
}

/*=======================================================================*/
/* Creates buffer space for owned elements. All updates are performed
   in this buffer space. When an operation commits, the contents of
   the buffer space is written over the state of the object. */
/*=======================================================================*/

int 
new_elements(instance_p instance, set_p processors)
{
  int length;
  int i;
  void *part;
  state_p state;
  consistency_p consistency;
  
  precondition(instance!=NULL);
  
  state=instance->state;
  if (is_member(instance->processors,me)) {
    state->data=get_space(state->total_size);
    assert(state->data!=NULL);
  }
  instance->fch=new_mp_channel(processors);
  instance->consistency=new_consistency(instance);
  instance->gch=new_grp_channel(processors);
  consistency=instance->consistency;

  length=0;
  for (i=0; i<state->num_parts; i++)
    if (state->owner[i]==me)
      length=length+state->partition[i]->num_elements;
  state->owned_elements=(void *)malloc(length*instance->po->element_size);
  consistency->part_status[0]=
    (atomic_int_p *)malloc(sizeof(atomic_int_p)*state->num_parts);
  consistency->part_status[1]=
    (atomic_int_p *)malloc(sizeof(atomic_int_p)*state->num_parts);
  consistency->timeouts[0]=(timeout_p *)malloc(sizeof(timeout_p)*state->num_parts);
  consistency->timeouts[1]=(timeout_p *)malloc(sizeof(timeout_p)*state->num_parts);
  for (i=0; i<state->num_parts; i++) {
    consistency->timeouts[0][i]=new_timeout(MIN_TIMEOUT,INT_TIMEOUT,MAX_TIMEOUT);
    consistency->timeouts[1][i]=new_timeout(MIN_TIMEOUT,INT_TIMEOUT,MAX_TIMEOUT);
    consistency->part_status[0][i]=new_atomic_int(INVALID);
    consistency->part_status[1][i]=new_atomic_int(VALID);
  }
  
  part=state->owned_elements;
  for (i=0; i<state->num_parts; i++) {
    if (state->owner[i]==me) {
      state->partition[i]->elements=part;
      part=(void *)((char *)part+state->partition[i]->num_elements*
		    instance->po->element_size);
    }
  }
  synch_handlers();
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
int 
free_elements(instance_p instance) {
  if (is_member(instance->processors,me))
    free_space(instance->state->data);
  free_consistency(instance);
  free(instance->state->owned_elements);
  free_collection(instance->synch_ch);
  free_collection(instance->return_ch);
  return 0;
}

/*=======================================================================*/
/* Returns the pointer to an instance given its id. */
/*=======================================================================*/

instance_p 
get_instance(int i_id) {
  precondition(i_id>=0);
  
  return (get_item(instance_table,i_id));
}



/*=======================================================================*/
/* Links a pdg with an operation of an object. */
/*=======================================================================*/

int 
set_pdg(instance_p instance, po_opcode opcode, pdg_p pdg)
{
  int i, j;
  pdg_p oldpdg;
  
  precondition(instance!=NULL);
  precondition(opcode!=NULL);
  
  i=operation_index(instance->po,opcode);
  assert(i>=0);
  oldpdg = get_item(instance->pdg_table, i);
  j=insert_index(instance->pdg_table,(item_p)pdg,i);
  if (oldpdg) free_pdg(oldpdg);
  assert(j>=0);
  return 0;
}

/*=======================================================================*/
/* Returns the pdg associated with an operation. */
/*=======================================================================*/

pdg_p 
get_pdg(instance_p instance, po_opcode opcode)
{
  int op_i;
  
  precondition(instance!=NULL);
  precondition(opcode!=NULL);

  op_i=operation_index(instance->po,opcode);
  assert(op_i>=0);
  return (pdg_p )(get_item(instance->pdg_table,op_i));
}
