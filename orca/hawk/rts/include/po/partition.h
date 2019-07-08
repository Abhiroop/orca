#ifndef __partition__
#define __partition__

#include "sys_po.h"
#include "util.h"

struct partition_s {
  int *start; /* Indices of the first element. */
  int *end; /* Indices of the last element. */
  int *length; /* Length in each dimension. */
  int num_elements; /* Number of elements. */
  void *elements; /* Pointer to the elements. */
  int part_num; /* Partition number. */
  int *part_indices; 
  int requested;
  int received;
  boolean last_color;
};

partition_p new_partition(int part_num, int *part_indices,
			  int elem_size, int num_dim, 
			  int *start, int *end, int *total_length);
partition_p duplicate_partition(instance_p instance, partition_p p);
int free_partition(partition_p partition);
int print_partition(partition_p partition, int num_dim);

int save_partition(instance_p instance, void *buf, void *elts,
		   int part_num);
int commit_partition(instance_p instance, void *buf, void *elts,
		     int part_num);
#endif
