#ifndef __sys_collection__
#define __sys_collection__

#include "sys_po.h"
#include "message.h"
#include "set.h"

typedef struct coll_header_s coll_header_t, *coll_header_p;
typedef struct coll_opdescr_s coll_opdescr_t, *coll_opdescr_p;
typedef struct coll_buffers_s coll_buffers_t, *coll_buffers_p;
typedef struct coll_channel_s coll_channel_t, *coll_channel_p;
typedef struct coll_operation_s coll_operation_t, *coll_operation_p;
typedef void (*marshall_function_p)(int,coll_channel_p,void*,int);
typedef void (*combine_function_p)(int,coll_channel_p,message_p);
typedef void (*unmarshall_function_p)(int,coll_channel_p,message_p);
typedef void (*send_function_p)(int,coll_channel_p ch);

#include "reduction_function.h"
#include "timeout.h"
#include "coll_operation.h"
#include "coll_channel.h"
#include "message_handlers.h"

struct collection_s {
  coll_channel_p ch[2];
  int chnum;
  int finishrcv;
  int *rcvphase;
  int *sender;
  int *receiver;
  set_p members;
};

#endif
