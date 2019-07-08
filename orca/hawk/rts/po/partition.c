#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "instance.h"
#include "state.h"
#include "partition.h"
#include "misc.h"
#include "condition.h"
#include "precondition.h"

#define max_dim 2

typedef int (* save_partition_function)(instance_p instance, void *buf,
					void *elts, int partno);
typedef int (* commit_partition_function)(instance_p instance,void *buf,
					  void *elts,  int partno);

static int save_partition_2(instance_p instance, void *buf, void *elts,
			    int partno);
static int commit_partition_2(instance_p instance, void *buf, void *elts,
			      int partno);
static int save_partition_1(instance_p instance, void *buf, void *elts, 
			    int partno);
static int commit_partition_1(instance_p instance, void *buf, void *elts,
			      int partno);

static save_partition_function save_function_table[max_dim+1]={
  NULL,
  save_partition_1,
  save_partition_2,
};

static commit_partition_function commit_function_table[max_dim+1]={
  NULL,
  commit_partition_1,
  commit_partition_2,
};

/*=======================================================================*/
/* Creates a new partition structure. Does not allocate memory for
   data. That is done whenever the value of data is initialized. */
/*=======================================================================*/

partition_p 
new_partition(int part_num, int *part_indices,
	      int element_size, int num_dim, 
	      int *start, int *end, int *total_length) {
  int i;

  partition_p partition;

  precondition(element_size>0);
  precondition(num_dim>0);
  precondition(start!=NULL);
  precondition(end!=NULL);

  partition=(partition_p )malloc(sizeof(partition_t));
  assert(partition!=NULL);
  partition->start=(int *)malloc(sizeof(int)*num_dim);
  memcpy(partition->start,start,sizeof(int)*num_dim);
  partition->end=(int *)malloc(sizeof(int)*num_dim);
  memcpy(partition->end,end,sizeof(int)*num_dim);
  partition->length=(int *)malloc(sizeof(int)*num_dim);
  for (i=0; i<num_dim; i++)
    partition->length[i]=end[i]-start[i]+1;
  partition->elements=NULL;
  partition->num_elements=1;
  for (i=0; i<num_dim; i++)
    partition->num_elements=partition->num_elements*(end[i]-start[i]+1);
  assert(partition->num_elements>0);
  partition->elements=NULL;
  partition->part_num=part_num;
  partition->part_indices=(int *)malloc(sizeof(int)*num_dim);
  memcpy(partition->part_indices,part_indices,num_dim*sizeof(int));
  partition->received=0;
  partition->requested=0;
  return partition;
}

/*=======================================================================*/
/*=======================================================================*/
partition_p
duplicate_partition(instance_p instance, partition_p p) {
  partition_p newp;

  precondition(instance!=NULL);
  precondition(p!=NULL);
  precondition(p->elements!=NULL);

  newp=(partition_p )malloc(sizeof(partition_t));
  assert(newp!=NULL);
  newp->start=(int *)malloc(sizeof(int)*instance->po->num_dims);
  assert(newp->start!=NULL);
  newp->end=(int *)malloc(sizeof(int)*instance->po->num_dims);
  assert(newp->end!=NULL);
  newp->length=(int *)malloc(sizeof(int)*instance->po->num_dims);
  assert(newp->length!=NULL);
  memcpy(newp->start,p->start,sizeof(int)*instance->po->num_dims);
  memcpy(newp->end,p->end,sizeof(int)*instance->po->num_dims);
  memcpy(newp->length,p->length,sizeof(int)*instance->po->num_dims);
  newp->num_elements=p->num_elements;
  newp->elements=(void *)malloc(p->num_elements*instance->po->element_size);
  memcpy(newp->elements, p->elements, p->num_elements*instance->po->element_size);
  newp->part_num=p->part_num;
  newp->part_indices=(int *)malloc(sizeof(int)*instance->po->num_dims);
  memcpy(newp->part_indices,p->part_indices,
	 instance->po->num_dims*sizeof(int));
  newp->received=0;
  newp->requested=0;
  return newp;
}

/*=======================================================================*/
/* Frees a partition structure. */
/*=======================================================================*/
int 
free_partition(partition_p partition)
{
  precondition(partition!=NULL);
  
  if (partition->start!=NULL) free(partition->start);
  if (partition->end!=NULL) free(partition->end);
  if (partition->length!=NULL) free(partition->length);
  if (partition->part_indices!=NULL) free(partition->part_indices);
  free(partition);
  return 0;
}

/*=======================================================================*/
/* Prints a partition structure. Data is not printed. */
/*=======================================================================*/

int 
print_partition(partition_p partition, int num_dim)
{
  int i;

  precondition(partition!=NULL);
  
  printf("First Element Indices: ");
  for (i=0; i<num_dim; i++)
    printf("\t%d",partition->start[i]);
  printf("\nLast Element Indices: ");
  for (i=0; i<num_dim; i++)
    printf("\t%d",partition->end[i]);
  printf("\n");
  return 0;
}


/*=======================================================================*/
/* Buffer is a memory area that contains the entire state of a
   partitioned object. This procedure marshalls the partition's state
   from buffer into the partition structure (elements). It's mainly
   used to send a partition to another processor. This procedure works
   for 2 dimensional partitioned objects. */
/*=======================================================================*/
int 
save_partition_2(instance_p instance, void *buf, void *elts,
		 int partno) {
  register int i;
  void *source_buffer;
  void *dest_buffer;
  state_p state = instance->state;
  partition_p partition = state->partition[partno];
  int element_size = instance->po->element_size;
  
  precondition(instance!=NULL);
  
  i=0;
  source_buffer=(void *)((char *)buf
    + (partition->start[0]-state->start[0])*state->length[1]*element_size
    + (partition->start[1]-state->start[1])*element_size);
  dest_buffer=elts;
  if (partition->length[1] == state->length[1]) {
	memcpy(dest_buffer, source_buffer, 
	       partition->length[0]*partition->length[1]*element_size);
  }
  else for (i=0; i<partition->length[0]; i++)
      { 
	memcpy(dest_buffer, source_buffer, 
	       partition->length[1]*element_size);
	dest_buffer=(void *)((char *)dest_buffer+partition->length[1]*element_size);
	source_buffer=(void *)((char *)source_buffer+state->length[1]*element_size);
      }
  return 0;
}

/*=======================================================================*/
/* This performs the reverse procedure: it unmarshalls data from
   elements into the buffer, which contains the entire state of the
   partitioned object. This function works for 2 dimensional
   partitioned objects. */
/*=======================================================================*/
int 
commit_partition_2(instance_p instance, void *buf, void *elts,
		   int partno) {
  register int i;
  void *source_buffer;
  void *dest_buffer;
  state_p state = instance->state;
  partition_p partition = state->partition[partno];
  int element_size = instance->po->element_size;
  
  precondition(instance!=NULL);
  
  dest_buffer=(void *)((char *)buf
    + (partition->start[0]-state->start[0])*state->length[1]*element_size
    + (partition->start[1]-state->start[1])*element_size);
  source_buffer=elts;
  if (state->length[1] == partition->length[1]) {
	memcpy(dest_buffer, source_buffer, 
	       partition->length[0]*partition->length[1]*element_size);
  }
  else for (i=0; i<partition->length[0]; i++)
      { 
	memcpy(dest_buffer, source_buffer, 
	       partition->length[1]*element_size);
	source_buffer=(void *)((char *)source_buffer+partition->length[1]*element_size);
	dest_buffer=(void *)((char *)dest_buffer+state->length[1]*element_size);
      }
  return 0;
}


/*=======================================================================*/
/* This procedure saves a partition from 'buffer' into 'elements'. */
/*=======================================================================*/
int 
save_partition_1(instance_p instance, void *buf, void *elts, 
		 int partno) {
  partition_p partition = instance->state->partition[partno];

  precondition(partition!=NULL);
  
  memcpy(elts,
	 (void *)((char *)buf
	   + (partition->start[0]-instance->state->start[0])
	     * instance->po->element_size), 
	 partition->length[0]*instance->po->element_size);
  return 0;
}


/*=======================================================================*/
/* Same procedure as above, but for 1 dimensional partitioned
   objects. */
/*=======================================================================*/
int 
commit_partition_1(instance_p instance, void *buf, void *elts, int partno) {
  partition_p partition = instance->state->partition[partno];
  
  precondition(partition!=NULL);

  memcpy(
	 (void *)((char *)buf
	   + (partition->start[0]-instance->state->start[0])
	     * instance->po->element_size), 
	 elts,
	 partition->length[0]*instance->po->element_size);
  
  return 0;
}

/*=======================================================================*/
/* Selects the proper saving function according to the number of
   dimensions in the object. */
/*=======================================================================*/
int 
save_partition(instance_p instance, void *buf, void *elts, int partno) {
  precondition(instance!=NULL);

  (*(save_function_table[instance->po->num_dims]))(instance, buf, elts, partno);
  return 0;
}


/*=======================================================================*/
/* Same procedure as above, but for commiting. */
/*=======================================================================*/
int 
commit_partition(instance_p instance, void *buf, void *elts, int partno) {

  precondition(instance!=NULL);

  (*(commit_function_table[instance->po->num_dims]))(instance, buf, elts,
						     partno);
  return 0;
}
