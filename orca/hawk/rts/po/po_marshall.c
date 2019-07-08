#include <stdlib.h>
#include <string.h>

#include "po_marshall.h"
#include "instance.h"
#include "misc.h"
#include "condition.h"
#include "po_timer.h"
#include "message.h"
#include "precondition.h"
#include "assert.h"

#define MODULE_NAME "PO_MARSHALL"
#define max_dim 2

typedef void (*marshall_partition_function)(message_p m, instance_p i, int p);
typedef void *(*unmarshall_partition_function)(void *m, instance_p i, int p);

static void marshall_partition_1(message_p m, instance_p i, int p);
static void marshall_partition_2(message_p m, instance_p i, int p);
static void *unmarshall_partition_1(void *m, instance_p i, int p);
static void *unmarshall_partition_2(void *m, instance_p i, int p);

static marshall_partition_function marshall_function_table[max_dim+1]={
  NULL,
  marshall_partition_1,
  marshall_partition_2,
};

static unmarshall_partition_function unmarshall_function_table[max_dim+1]={
  NULL,
  unmarshall_partition_1,
  unmarshall_partition_2,
};

/*=======================================================================*/
/* Marshall a partition from the "data" buffer of the instance into the
   message "message". This version is for 2-dimensional partitioned objects.
*/
/*=======================================================================*/
void 
marshall_partition_2(message_p message, instance_p instance, int partnum)
{
  register int i;
  void *source_buffer;
  partition_p partition = instance->state->partition[partnum];
  int element_size = instance->po->element_size;
  int pl1 = partition->length[1]*element_size;
  int psl1 = instance->state->length[1]*element_size;
  int pst0 = partition->start[0] - instance->state->start[0];
  int pst1 = partition->start[1] - instance->state->start[1];
  precondition(message!=NULL);
  precondition(instance!=NULL);
  
  source_buffer = (void *)((char *)instance->state->data + pst0 * psl1 + pst1 * element_size);
  if (psl1 == pl1) {
	message_append_data(message, source_buffer, partition->length[0]*pl1);
  }
  else for (i=0; i<partition->length[0]; i++) { 
    message_append_data(message, source_buffer, pl1);
    source_buffer=(void *)((char *)source_buffer+psl1);
  }
}

/*=======================================================================*/
/* This unmarshalls data from 'buf' into the state of the partitioned
   object. This function works for 2 dimensional partitioned
   objects. */
/*=======================================================================*/
void 
*unmarshall_partition_2(void *buf, instance_p instance, int partnum)
{
  register int i;
  void *source_buffer;
  void *dest_buffer;
  partition_p partition = instance->state->partition[partnum];
  int element_size = instance->po->element_size;
  int pl1 = partition->length[1]*element_size;
  int psl1 = instance->state->length[1]*element_size;
  
  precondition(buf!=NULL);
  precondition(instance!=NULL);
  
  source_buffer = buf;
  dest_buffer = (void *)((char *)instance->state->data +
	(partition->start[0] - instance->state->start[0]) * psl1 +
	(partition->start[1] - instance->state->start[1]) * element_size);
  if (psl1 == pl1) {
	memcpy(dest_buffer, source_buffer, partition->length[0]*pl1);
	return (void *)((char *)source_buffer+partition->length[0]*pl1);
  }
  for (i=0; i<partition->length[0]; i++)
      { 
	memcpy(dest_buffer, source_buffer, pl1);
	dest_buffer=(void *)((char *)dest_buffer+psl1);
	source_buffer=(void *)((char *)source_buffer+pl1);
      }
  return source_buffer;
}


/*=======================================================================*/
/* This procedure marshalls a partition from 'instance' into 'message'.  */
/*=======================================================================*/
void 
marshall_partition_1(message_p message, instance_p instance, int partnum)
{
  partition_p partition = instance->state->partition[partnum];
  int element_size = instance->po->element_size;

  precondition(instance!=NULL);
  precondition(message!=NULL);

  message_append_data(message,
	 (void *)((char *)instance->state->data+(partition->start[0]-instance->state->start[0])*element_size),
	 partition->num_elements * element_size);
}


/*=======================================================================*/
/* Same procedure as above, but for 1 dimensional partitioned
   objects. */
/*=======================================================================*/
void *
unmarshall_partition_1(void *buffer, instance_p instance, int partnum)
{
  partition_p partition = instance->state->partition[partnum];
  int element_size = instance->po->element_size;

  precondition(instance!=NULL);
  precondition(buffer!=NULL);
  
  memcpy((void *)((char *)instance->state->data+(partition->start[0]-instance->state->start[0])*element_size), buffer,
	 partition->length[0]*element_size);
  return (void *)((char *)buffer+partition->length[0]*element_size);
}

/*=======================================================================*/
/* Select marshalling function according to number of dimensions.	 */
/*=======================================================================*/
void 
marshall_partition(message_p m, instance_p i, int p)
{
  (*(marshall_function_table[i->po->num_dims]))(m, i, p);
}

/*=======================================================================*/
/* Select unmarshalling function according to number of dimensions.	 */
/*=======================================================================*/
void *
unmarshall_partition(message_p m, instance_p i, int p)
{
  return (*(unmarshall_function_table[i->po->num_dims]))(m, i, p);
}
