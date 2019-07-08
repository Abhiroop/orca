#ifndef __error_code__
#define __error_code__

#include "precondition.h"

#ifdef AMOEBA
#include "assert.h"

#ifdef PROTO_DEBUG
#define print_error(a,b,c) { fprintf(stderr,"In %30s: function %30s\t%s\n", a,b,c); exit(1); }
#endif /* PROTO_DEBUG */

#else /* NOT AMOEBA */

#ifdef PROTO_DEBUG
#define print_error(a,b) 

#else /* NOT PROTO_DEBUG */
#define print_error(a,b) 

#endif /* PROTO_DEBUG */

#ifdef PRECONDITION_ON
#undef assert
#define assert(c) precondition(c)
#else
#include <assert.h>
#endif

#endif /* AMOEBA */

typedef enum {
  COLLECTION_ALREADY_INITIALIZED,
  COLLECTION_NOT_INITIALIZED,
  ERROR_CREATING_SYS_CHANNEL,
  ERROR_DURING_SYS_BARRIER,
  ILLEGAL_CHANNEL,
  ILLEGAL_GROUP_SIZE,
  ILLEGAL_MEMBER_NUMBER,
  ILLEGAL_OPERATION_TYPE,
  ILLEGAL_PARTIONING,
  ILLEGAL_ROOT,
  NULL_SEND_BUFFER,
  NULL_RECEIVE_BUFFER,
  ILLEGAL_LIST,
  ILLEGAL_SET,
  SET_ALREADY_INITIALIZED,
  SET_NOT_INITIALIZED,
  ERROR_ALLOCATING_MEMORY,
  ILLEGAL_NUMBER_OF_PARTITIONS,
  ILLEGAL_PARTITION_SIZE_POINTER,
  ILLEGAL_GROUP_SET,
  ILLEGAL_PARTIITON_SIZE,
  ILLEGAL_ELEMENT_SIZE,
  ILLEGAL_DATA_MAPPING,
  FLIP_CHANNEL_NOT_INITIALIZED,
  HANDLER_ID_ALREADY_USED,
  ERROR_INITIALIZING_RAW_FLIP,
  ERROR_GETTING_CAP,
  ERROR_REGISTERING_FLIP_ADDRESS,
  ILLEGAL_ARGUMENTS,
  NEGATIVE_TREE_DEPTH,
  INTERNAL_INCONSISTENCY,
  MAP_IS_FULL,
  MAP_TABLE_FULL,
  NO_SUCH_ITEM,
  TOO_MANY_MAPS,
  ILLEGAL_SIZE,
  ILLEGAL_MESSAGE,
  SIZE_LARGER_THAN_DATA
} error_code;

#define rts_assert(function,condition,print_error_code)  if (!(condition))     	\
{ fprintf(stderr,								\
	  "assertion " #condition " in function " #function " is false\n");	\
  fprintf(stderr,								\
	  "error code is " #print_error_code " \n");	                        \
  assert(0); 									\
}

#ifdef PROTO_DEBUG
#define proc_trace(a) { if (me==proc_debug) { fprintf(stderr,"%d:\t " #a "\n",me); fflush(stderr); } }
#define trace(a) { fprintf(stderr,"\t " #a "\n"); fflush(stderr); }
#else
#define proc_trace(a) 
#define trace(a) 
#endif

#endif
