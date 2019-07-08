#ifndef __state__
#define __state__

#include "sys_po.h"
#include "atomic_int.h"
#include "condition.h"
#include "partition.h"

typedef enum {
  VALID,
  INVALID,
  INSYNC
} status_type;

struct state_s
{
  instance_p instance;             /* Pointer to the corresponding
				      instance. */
  int *start;
  int *end;
  void *val;
  int *length;                     /* Length of the instance along every 
				      dimension. */
  int total_size;                  /* Total size of the state of the 
				      instance. */
  int num_parts;                   /* Total number of partitions. */
  int *num_part_indices;           /* Total number of partitions along each
				      dimension. */
  int *part_size;		   /* Size of the partitions along each
				      dimension. (the first couple of partitions
				      may be one larger if there is no exact
				      fit). */
  int *num_larger_parts;	   /* the number of "larger" partitions. */
  int num_owned_parts;             /* Number of owned partitions. */
  int *owner;                      /* Owner of each partition. */
  int *order;                      /* Order in which dimensions are
				      stored. Only the row major order
				      is used at the moment, but this
				      might be inefficient for
				      structures partitioned
				      columnwise. */
  void *data;                      /* Pointer to the first byte of the
				      state. */
  void *owned_elements;            /* Pointers to the owned state. */
  partition_p *partition;          /* Pointers to the first part of
				      each partition. */
  partition_p *owned_partitions;   /* Pointers to the first byte of
				      each owned partition. The
				      partitions are ordered in
				      ascending order of their
				      numbers. */
};

#endif
