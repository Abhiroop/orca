#ifndef __coll_operation__
#define __coll_operation__

#include "sys_collection.h"
#include "message.h"

struct coll_operation_s {
  marshall_function_p mf;
  combine_function_p cf;
  unmarshall_function_p uf;
  send_function_p sf;
  int opcode;
};

int init_coll_operation(int moi, int gsize, int pdebug);
int finish_coll_operation(void);

coll_operation_p new_coll_operation(marshall_function_p mf, combine_function_p cf,
				    unmarshall_function_p uf, send_function_p sf);

extern coll_operation_t operation_table[];

extern coll_operation_p gather_poop;
extern coll_operation_p gatherop;
extern coll_operation_p gatherallop;
extern coll_operation_p reduceop;
extern coll_operation_p reduceallop;
extern coll_operation_p barrierop;

#endif
