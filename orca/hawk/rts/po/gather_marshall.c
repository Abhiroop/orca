#include <stdlib.h>
#include <string.h>

#include "gather_marshall.h"
#include "instance.h"
#include "misc.h"
#include "condition.h"
#include "po_timer.h"
#include "message.h"
#include "precondition.h"
#include "assert.h"

#define MODULE_NAME "GATHER_MARSHALL"
#define max_dim 2

typedef void (*marshall_function)(message_p m, instance_p i, int p, char *sb, int ss);
typedef void *(*unmarshall_function)(void *m, instance_p i, int p, char * b, int rs);

static void marshall_1(message_p m, instance_p i, int p, char *sb, int ss);
static void marshall_2(message_p m, instance_p i, int p, char *sb, int ss);
static void *unmarshall_1(void *m, instance_p i, int p, char *rb, int rs);
static void *unmarshall_2(void *m, instance_p i, int p, char *rb, int rs);

static marshall_function marshall_function_table[max_dim+1]={
  NULL,
  marshall_1,
  marshall_2,
};

static unmarshall_function unmarshall_function_table[max_dim+1]={
  NULL,
  unmarshall_1,
  unmarshall_2,
};

/*=======================================================================*/
/* Marshall the part of a gather parameter corresponding to partition "partnum"
   into the message "message".
   This version is for 2-dimensional partitioned objects.
*/
/*=======================================================================*/
void 
marshall_2(message_p message, instance_p instance, int partnum, char *sb, int ss)
{
  register int i;
  void *source_buffer;
  partition_p partition = instance->state->partition[partnum];
  int pl1 = partition->length[1]*ss;
  int psl1 = instance->state->length[1]*ss;
  int pst0 = partition->start[0] - instance->state->start[0];
  int pst1 = partition->start[1] - instance->state->start[1];
  precondition(message!=NULL);
  precondition(instance!=NULL);
  
  source_buffer = (void *)((char *)sb + pst0 * psl1 + pst1 * ss);
  if (psl1 == pl1) {
	message_append_data(message, source_buffer, partition->length[0]*pl1);
  }
  else for (i=0; i<partition->length[0]; i++) { 
    message_append_data(message, source_buffer, pl1);
    source_buffer=(void *)((char *)source_buffer+psl1);
  }
}

/*=======================================================================*/
/* This unmarshalls data from 'buf' into the receive buffer.
   This function works for 2 dimensional partitioned objects. */
/*=======================================================================*/
void 
*unmarshall_2(void *buf, instance_p instance, int partnum, char *rb, int rs)
{
  register int i;
  void *source_buffer;
  void *dest_buffer;
  partition_p partition = instance->state->partition[partnum];
  int pl1 = partition->length[1]*rs;
  int psl1 = instance->state->length[1]*rs;
  
  precondition(buf!=NULL);
  precondition(instance!=NULL);
  
  source_buffer = buf;
  dest_buffer = (void *)(rb +
	(partition->start[0] - instance->state->start[0]) * psl1 +
	(partition->start[1] - instance->state->start[1]) * rs);
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
/* Marshall the part of a gather parameter corresponding to partition "partnum"
   into the message "message". */
/*=======================================================================*/
void 
marshall_1(message_p message, instance_p instance, int partnum, char *sb, int ss)
{
  partition_p partition = instance->state->partition[partnum];

  precondition(instance!=NULL);
  precondition(message!=NULL);

  message_append_data(message,
	 (void *)(sb+(partition->start[0]-instance->state->start[0])*ss),
	 partition->num_elements * ss);
}


/*=======================================================================*/
/* Unmarshall the part of a gather parameter corresponding to partition
   "partnum" from buffer into rb. */
/*=======================================================================*/
void *
unmarshall_1(void *buffer, instance_p instance, int partnum, char *rb, int rs)
{
  partition_p partition = instance->state->partition[partnum];

  precondition(instance!=NULL);
  precondition(buffer!=NULL);
  
  memcpy((void *)(rb+(partition->start[0]-instance->state->start[0])*rs), buffer,
	 partition->length[0]*rs);
  return (void *)((char *)buffer+partition->length[0]*rs);
}

/*=======================================================================*/
/* Select marshalling function according to number of dimensions.	 */
/*=======================================================================*/
void 
gather_marshall(message_p m, instance_p i, int p, char *sb, int ss)
{
  (*(marshall_function_table[i->po->num_dims]))(m, i, p, sb, ss);
}

/*=======================================================================*/
/* Select unmarshalling function according to number of dimensions.	 */
/*=======================================================================*/
void *
gather_unmarshall(message_p m, instance_p i, int p, char *rb, int rs)
{
  return (*(unmarshall_function_table[i->po->num_dims]))(m, i, p, rb, rs);
}
