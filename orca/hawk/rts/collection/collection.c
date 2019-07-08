#include <stdlib.h>
#include "collection.h"
#include "instance.h"
#include "state.h"
#include "coll_channel.h"
#include "coll_operation.h"
#include "forest.h"
#include "message.h"
#include "synchronization.h"
#include "set.h"
#include "assert.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

int 
init_collection(int moi, int gsize, int pdebug, int *argc, char **argv) {
  
  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_forest(moi,gsize,pdebug,argc,argv);
  init_timeout(moi,gsize,pdebug,argc,argv);
  init_coll_channel(moi,gsize,pdebug);
  init_message_handler(moi,gsize,pdebug);
  init_reduction_function();
  init_coll_operation(moi,gsize,pdebug);
  return 0;
}

int 
finish_collection(void) {
  if (--initialized) return 0;
  finish_coll_operation();
  finish_reduction_function();
  finish_message_handler();
  finish_coll_channel();
  finish_timeout();
  finish_forest();
  return 0;
}


collection_p 
new_collection(set_p set) {
  collection_p coll;

  coll=(collection_p )malloc(sizeof(collection_t));
  /* Initialize members set */
  if (set==NULL) {
    coll->members = new_set(group_size);
    full_set(coll->members);
  }
  else
    coll->members=duplicate_set(set);
  /* initialize channels */
  coll->forest=new_forest(coll->members);
  coll->ch[0]=new_coll_channel(coll);
  assert(coll->ch[0]!=NULL);
  coll->ch[1]=new_coll_channel(coll);
  assert(coll->ch[1]!=NULL);
  coll->chnum=0;
  return coll;
}

int
coll_add_instance(collection_p coll, void *instance)
{
  coll->ch[0]->op->scratch = instance;
  coll->ch[1]->op->scratch = instance;
  return 0;
}

int 
free_collection(collection_p coll) {

  free_coll_channel(coll->ch[0]);
  free_coll_channel(coll->ch[1]);
  free_forest(coll->forest);
  free_set(coll->members);
  free(coll);
  return 0;
}


int 
collection_wait(collection_p coll) {
  return coll_channel_wait(coll->ch[coll->chnum]);
}

int
gather(collection_p coll, int caller, void *sbuffer, int ssize,
       void *rbuffer, int rsize) {
  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],
			caller,gatherop,sbuffer,ssize,rbuffer,rsize,NULL);
}

int
gather_all(collection_p coll, void *sbuffer, int ssize,
       void *rbuffer, int rsize) {
  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],
			-1,gatherallop,sbuffer,ssize,rbuffer,rsize,NULL);
}

int
gather_po(collection_p coll, void *data) {
  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],
			-1,gather_poop,data,0,data,0,NULL);
}

int
reduce_all(collection_p coll, void *sbuffer, int ssize,
	  void *rbuffer, int rsize, reduction_function_p rf) {
  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],
			-1,reduceallop,sbuffer,ssize,rbuffer,rsize,rf);
}

int
reduce(collection_p coll, int caller, void *sbuffer, int ssize,
	  void *rbuffer, int rsize, reduction_function_p rf) {
  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],
			caller,reduceop,sbuffer,ssize,rbuffer,rsize,rf);
}

int
barrier(collection_p coll) {

  coll_channel_wait(coll->ch[coll->chnum]);
  coll->chnum = !coll->chnum;
  return coll_operation(coll->ch[coll->chnum],-1,barrierop,NULL,0,NULL,0,NULL);
}


int
send_exit(collection_p coll)
{
  return send_exit_exception(coll->ch[0]);
}
