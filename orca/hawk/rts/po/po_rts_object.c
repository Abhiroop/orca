#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "po.h"
#include "instance.h"
#include "consistency.h"
#include "po_invocation.h"
#include "misc.h"
#include "set.h"
#include "po_rts_object.h"
#include "po_timer.h"
#include "collection.h"
#include "precondition.h"
#include "assert.h"
#include "sleep.h"

#define MODULE_NAME "PO_RTS_OBJECT"
#define MAX_GRP_MSG_SIZE 16000
#define marshall_array(a,s) marshall_proc_list(a,s)
#define unmarshall_array(a,s) unmarshall_proc_list(a,s)

/* Note that in all do_ functions the args[] array is too large.
   This is intentional, because the execution handlers fumble in an extra
   parameter.
*/

po_lock_p po_rts_lock;

static instance_p rts;
static po_p rts_po;

static po_pardscr_t par_intparam[1] = { 
  { sizeof(int), IN_PARAM, 0, 0 }
};
static po_pardscr_t par_intbufparam[2] = {
  { sizeof(int), IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 }
};
static po_pardscr_t par_bufbufintparam[3] = {
  { 0, IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 },
  { sizeof(int), IN_PARAM, 0, 0 }
};
static po_pardscr_t par_intbufbufparam[3] = {
  { sizeof(int), IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 }
};
static po_pardscr_t par_intintbufparam[3] = {
  { sizeof(int), IN_PARAM, 0, 0 },
  { sizeof(int), IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 }
};
static po_pardscr_t par_intbufbufptrparam[4] = {
  { sizeof(int), IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 },
  { 0, IN_PARAM, 0, 0 },
  { sizeof(instance_p), OUT_PARAM, 0, 0 }
};
static po_pardscr_t par_intintptrintparam[4] = {
  { sizeof(int), IN_PARAM, 0, 0 },
  { sizeof(int), IN_PARAM, 0, 0 },
  { sizeof(po_opcode), IN_PARAM, 0, 0},
  { sizeof(int), IN_PARAM, 0, 0}
};

static void call_rowwise_partitioning(int sender, instance_p instance, 
				      void **args);
static void call_columnwise_partitioning(int sender, instance_p instance, 
					void **args);
static void call_my_partitioning(int sender, instance_p instance, 
                                 void **args);
static void call_new_instance(int sender, instance_p instance, 
			      void **args);
static void call_free_instance(int sender, instance_p instance, 
			       void **args);
void call_finish_rts(int sender, instance_p instance, 
			    void **args);
static void call_log_all_po_timers(int sender, instance_p instance, 
			  void **args);
static void call_my_distribution(int sender, instance_p instance, 
				 void **args);
static void call_change_attribute(int sender, instance_p instance, 
				   void **args);
static void call_set_dependencies(int sender, instance_p instance, 
				   void **args);
static void call_distribute_on_list(int sender, instance_p instance, 
				    void **args);
static void call_distribute_on_n(int sender, instance_p instance, void **args);

static void *marshall_dist(int num_dims, int **dist);
static int **unmarshall_dist(int num_dims, void *buffer);
static void *marshall_proc_list(int nun_proc, int *proc_list);
static int *unmarshall_proc_list(void *buffer, int *num_proc);

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static collection_p finish_ch;

/*=======================================================================*/
/* Initialized the RTS instance. The instance has several methods
   partitioning and distribution of other instances. The instance to be
   distributed is given as an argument to the method call. All the RTS
   methods are write sequential methods. */
/*=======================================================================*/

int 
init_rts_object(int moi, int gsize, int pdebug, int max_arg_size, int *argc, char **argv)
{
  int length;
  set_p processors;

  if (initialized++) return 0;
  precondition((moi>=0)&&(moi<gsize));

  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  length=gsize;

  init_lock(me,group_size,proc_debug);
  po_rts_lock=new_lock();
  init_po_module(me,group_size,proc_debug);
  init_instance_module(me,group_size,proc_debug, argc, argv);
  init_consistency_module(me,group_size,proc_debug,argc,argv);
  init_invocation_module(me,group_size,proc_debug,MAX_GRP_MSG_SIZE,argc,argv);
  init_message(me,group_size,proc_debug);
  init_collection(me,group_size,proc_debug,argc,argv);
  processors=new_set(gsize);
  full_set(processors);

  finish_ch=new_collection(NULL);
  rts_po=new_po("rts",1,sizeof(int),(void *) 0);
  new_po_operation(rts_po,"call_free_instance",
		   (po_opcode)call_free_instance,RtsOp,
		   1, par_intparam, NULL);
  new_po_operation(rts_po,"call_finish_rts",
		   (po_opcode)call_finish_rts,RtsOp,
		   0, (po_pardscr_p) 0, NULL);
  new_po_operation(rts_po,"call_my_partitioning",
                   (po_opcode)call_my_partitioning,RtsOp,
                   2, par_intbufparam, NULL);
  new_po_operation(rts_po,"call_rowwise_partitioning",
		   (po_opcode)call_rowwise_partitioning,RtsOp,
		   1, par_intparam, NULL);
  new_po_operation(rts_po,"call_columnwise_partitioning",
		   (po_opcode)call_columnwise_partitioning,RtsOp,
		   1, par_intparam, NULL);
  new_po_operation(rts_po,"call_my_distribution",
		   (po_opcode)call_my_distribution,RtsOp,
		   2, par_intbufparam, NULL);
  new_po_operation(rts_po,"call_distribute_on_n",
		   (po_opcode)call_distribute_on_n,RtsOp,
		   2, par_intbufparam, NULL);
  new_po_operation(rts_po,"call_distribute_on_list",
		   (po_opcode)call_distribute_on_list,RtsOp,
		   3, par_intbufbufparam, NULL);
  new_po_operation(rts_po,"call_log_all_po_timers",
		   (po_opcode)call_log_all_po_timers,RtsOp,
		   3, par_bufbufintparam, NULL);
  new_po_operation(rts_po,"call_new_instance",
		   (po_opcode)call_new_instance,RtsOp,
		   4, par_intbufbufptrparam, NULL);
  new_po_operation(rts_po,"call_change_attribute",
		   (po_opcode)call_change_attribute,RtsOp,
		   4, par_intintptrintparam, NULL);

  rts=new_instance(rts_po,&length,0);
  my_partitioning(rts,&group_size);
  block_distribution(rts,processors);
  free_set(processors);

  synch_handlers();
  
  return 0;
}

/*======================================================================*/
/* Add operations which exist for all PO's. These used to be operations
   on rts_op, that's why this is here. */
/*======================================================================*/
void
add_ops(po_p po)
{
  new_po_operation(po, "call_set_dependencies",
		   (po_opcode)call_set_dependencies,RtsOp,
		   3, par_intintbufparam, NULL);
}

/*=======================================================================*/
/* Deletes the RTS instance. */
/*=======================================================================*/
int 
finish_rts_object(void)
{
  if (--initialized) return 0;

  barrier(finish_ch);
  collection_wait(finish_ch);
  if (me==0) rts_sleep(1);
  free_collection(finish_ch);
  free_instance(rts);
  free_po_operation(rts_po,(po_opcode)call_free_instance);
  free_po_operation(rts_po,(po_opcode)call_finish_rts);
  free_po_operation(rts_po,(po_opcode)call_rowwise_partitioning);
  free_po_operation(rts_po,(po_opcode)call_columnwise_partitioning);
  free_po_operation(rts_po,(po_opcode)call_my_partitioning);
  free_po_operation(rts_po,(po_opcode)call_my_distribution);
  free_po_operation(rts_po,(po_opcode)call_distribute_on_n);
  free_po_operation(rts_po,(po_opcode)call_distribute_on_list);
  free_po_operation(rts_po,(po_opcode)call_log_all_po_timers);
  free_po_operation(rts_po,(po_opcode)call_new_instance);
  free_po_operation(rts_po,(po_opcode)call_change_attribute);
  free_po_operation(rts_po,(po_opcode)call_set_dependencies);
  free_po(rts_po);
  finish_collection();
  finish_message();
  finish_invocation_module(); 
  finish_consistency_module();
  finish_instance_module();
  finish_po_module();
  free_lock(po_rts_lock);
  finish_lock();
  return 0;
}

/*=======================================================================*/
/* Calls the method to change the execution attribute of an instance. */
/*=======================================================================*/

int 
do_change_attribute(instance_p instance, po_opcode opcode, 
		    attribute_t attribute, handler_p handler)
{
  void *args[5];
  int h;
  int opid;

  args[0]=&(instance->id);
  opid=operation_index(instance->po,(item_p)opcode);
  assert(opid>=0);
  args[1]=&opid;
  args[2]=&attribute;
  h=get_handler_id(handler);
  assert(h>=0);
  args[3]=&h;
  do_operation(rts,(po_opcode)call_change_attribute,args);
  wait_for_end_of_invocation(rts);
  return 0;
}

/*=======================================================================*/
/* Calls the method to create an instance. */
/*=======================================================================*/

instance_p 
do_new_instance(po_p po, int *length, int *base)
{
  void *args[5];
  instance_p instance;
  int len = sizeof(int)*(po->num_dims+1);

  precondition(po!=NULL);
  precondition(length!=NULL);

  args[0]=&(po->id);
  args[1]=(int *)malloc(len);
  *((int *)(args[1])) = len;
  memcpy((int *)(args[1])+1,length,len-sizeof(int));
  if (! base) len = sizeof(int);
  args[2]=(int *)malloc(len);
  *((int *)(args[2])) = len;
  if (base) memcpy((int *)(args[2])+1,base,len-sizeof(int));
  args[3] = &instance;
  do_operation(rts,(po_opcode)call_new_instance, args);
  wait_for_end_of_invocation(rts);
  free(args[1]);
  free(args[2]);
  return instance;
}


/*=======================================================================*/
/* Calls the method to free an instance. */
/*=======================================================================*/

int 
do_free_instance(instance_p instance)
{
  void *args[2];
  precondition(instance->id>0);  /* Do not allow deletion of the RTS
				    instance through this
				    procedure. */
  args[0] = &(instance->id);
  wait_for_end_of_invocation(instance);
  do_operation(rts,call_free_instance, args);
  wait_for_end_of_invocation(rts);
  return 0;
}

/*=======================================================================*/
/* Calls the method to dump amoeba timers. */
/*=======================================================================*/

int 
do_log_all_po_timers(char *filename, set_p processors, int label)
{
  void *args[4];
  char *fname;
  void *mset;
  int mset_size;
  boolean empty = FALSE;

  if (processors == NULL) {
    processors=new_set(group_size);
    full_set(processors);
    empty=TRUE;
  }
  fname=(char *)malloc(strlen(filename)+1+sizeof(int));
  ((int *)fname)[0]=strlen(filename)+1+sizeof(int);
  memcpy(fname+sizeof(int),filename, strlen(filename)+1);
  args[0]=fname;
  mset=marshall_set(processors,&mset_size);
  args[1]=mset;
  args[2] = &label;
  do_operation(rts,(po_opcode)call_log_all_po_timers,args);
  wait_for_end_of_invocation(rts);
  free(mset);
  free(fname);

  if (empty) free_set(processors);

  return 0;
}

/*=======================================================================*/
/* Calls the method to finish the rts instance and the rts. */
/*=======================================================================*/

int 
do_finish_rts_object(void)
{
  do_operation(rts,call_finish_rts,(void **) 0);
  wait_for_end_of_invocation(rts);
  return 0;
}

/*=======================================================================*/
/* Invokes an instance for a rowwise partitioning. */
/*=======================================================================*/
int 
do_rowwise_partitioning(instance_p instance)
{
  void *args[2];

  args[0] = &(instance->id);
  do_operation(rts,call_rowwise_partitioning,args);
  wait_for_end_of_invocation(rts);
  return 0;
}

/*=======================================================================*/
/* Invokes an instance for a columnwise partitioning. */
/*=======================================================================*/
int 
do_columnwise_partitioning(instance_p instance)
{
  void *args[2];

  args[0] = &(instance->id);
  do_operation(rts,call_columnwise_partitioning,args);
  wait_for_end_of_invocation(rts);
  return 0;
}

/*=======================================================================*/
/* Invokes an instance for a user defined partitioning. */
/*=======================================================================*/
int 
do_my_partitioning(instance_p instance, int *num_parts)
{
  int *buffer;
  int i;
  int num_dims = instance->po->num_dims;
  void *args[3];
  int sz = sizeof(int)*(1+num_dims);
 
  args[0] = &(instance->id);
  buffer=(int *)malloc(sz);
  buffer[0] = sz;
  for (i=0; i<num_dims; i++) buffer[i+1]=num_parts[i];
  args[1] = buffer;
  do_operation(rts,(po_opcode)call_my_partitioning,args);
  wait_for_end_of_invocation(rts);
  free(buffer);
  return 0;
}
 
/*=======================================================================*/
/* Invokes an instance for distribution defined by the user. */
/*=======================================================================*/
int 
do_my_distribution(instance_p instance, int *owner)
{
  void *args[3];

  args[0] = &(instance->id);
  args[1]= malloc(sizeof(int)*(1+instance->state->num_parts));
  *(int *)(args[1]) = sizeof(int)*(1+instance->state->num_parts);
  memcpy((int *)args[1]+1,owner, sizeof(int)*instance->state->num_parts);
  do_operation(rts,(po_opcode)call_my_distribution, args);
  wait_for_end_of_invocation(rts);
  free(args[1]);
  return 0;
}

int 
do_distribute_on_list(instance_p instance, int num_proc, 
		      int *proc_list, int **dist) {
  void *mdist;
  void *args[4];
  void *mproc;

  mdist=marshall_dist(instance->po->num_dims,dist);
  mproc=marshall_proc_list(num_proc,proc_list);
  args[0] = &(instance->id);
  args[1] = mproc;
  args[2] = mdist;
  do_operation(rts,call_distribute_on_list,args);
  wait_for_end_of_invocation(rts);
  free(mdist);
  free(mproc);
  return 0;  
}

int 
do_distribute_on_n(instance_p instance, int **dist) {
  void *mdist;
  void *args[3];

  mdist=marshall_dist(instance->po->num_dims,dist);
  args[0] = &(instance->id);
  args[1] = mdist;
  do_operation(rts,call_distribute_on_n,args);
  wait_for_end_of_invocation(rts);
  free(mdist);
  return 0;
}


/*=======================================================================*/
/*=======================================================================*/
int 
do_create_gather_channel(instance_p instance) {
  return 0;
}  

#define MAXDIM 64

static pdg_p cached_pdg;
static instance_p cached_instance;
static po_opcode cached_opcode;

int 
do_add_dependency(instance_p instance, po_opcode opcode, ...) {
  va_list
	ap;
  pdg_p	pdg;
  int	psource, pdest;
  int   srcbuf[MAXDIM];
  int   dstbuf[MAXDIM];
  int   *src = srcbuf;
  int   *dst = dstbuf;
  int   i;

  if (instance->po->num_dims > MAXDIM) {
	src = malloc(sizeof(int) * instance->po->num_dims);
	dst = malloc(sizeof(int) * instance->po->num_dims);
  }
  va_start(ap, opcode);
  for (i = 0; i < instance->po->num_dims; i++) {
	 src[i] = va_arg(ap, int);
  }
  for (i = 0; i < instance->po->num_dims; i++) {
	 dst[i] = va_arg(ap, int);
  }
  va_end(ap);
  if (cached_pdg && instance == cached_instance && opcode == cached_opcode) {
	pdg = cached_pdg;
  }
  else {
  	pdg=get_pdg(instance,opcode);
	cached_instance = instance;
	cached_opcode = opcode;
	cached_pdg = pdg;
  }
  assert(pdg!=NULL);
  pdg->valid=FALSE;
  psource=partition(instance,src);
  pdest=partition(instance,dst);
  if (psource != pdest) {
    add_edge(pdg,psource, instance->state->owner[psource], 
	   pdest, instance->state->owner[pdest]);
  }
  if (instance->po->num_dims > MAXDIM) {
	free(src);
	free(dst);
  }
  return 0;  
}

int 
do_add_pdependency(instance_p instance, po_opcode opcode, int psource, int pdest) {
  pdg_p pdg;

  if (cached_pdg && instance == cached_instance && opcode == cached_opcode) {
	pdg = cached_pdg;
  }
  else {
  	pdg=get_pdg(instance,opcode);
	cached_instance = instance;
	cached_opcode = opcode;
	cached_pdg = pdg;
  }
  assert(pdg!=NULL);
  pdg->valid=FALSE;
  if (psource != pdest) {
    add_edge(pdg,psource, instance->state->owner[psource], 
	   pdest, instance->state->owner[pdest]);
  }
  return 0;  
}

int 
do_remove_dependency(instance_p instance, po_opcode opcode, ...) {
  va_list
	ap;
  pdg_p	pdg;
  int	psource, pdest;
  int   srcbuf[MAXDIM];
  int   dstbuf[MAXDIM];
  int   *src = srcbuf;
  int   *dst = dstbuf;
  int   i;

  if (instance->po->num_dims > MAXDIM) {
	src = malloc(sizeof(int) * instance->po->num_dims);
	dst = malloc(sizeof(int) * instance->po->num_dims);
  }
  va_start(ap, opcode);
  for (i = 0; i < instance->po->num_dims; i++) {
	 src[i] = va_arg(ap, int);
  }
  for (i = 0; i < instance->po->num_dims; i++) {
	 dst[i] = va_arg(ap, int);
  }
  va_end(ap);
  if (cached_pdg && instance == cached_instance && opcode == cached_opcode) {
	pdg = cached_pdg;
  }
  else {
  	pdg=get_pdg(instance,opcode);
	cached_instance = instance;
	cached_opcode = opcode;
	cached_pdg = pdg;
  }
  assert(pdg!=NULL);
  pdg->valid=FALSE;
  psource=partition(instance,src);
  pdest=partition(instance,dst);
  if (psource != pdest) {
    remove_edge(pdg,psource, instance->state->owner[psource], 
	   pdest, instance->state->owner[pdest]);
  }
  if (instance->po->num_dims > MAXDIM) {
	free(src);
	free(dst);
  }
  return 0;  
}

int 
do_remove_pdependency(instance_p instance, po_opcode opcode, int psource, int pdest) {
  pdg_p pdg;

  if (cached_pdg && instance == cached_instance && opcode == cached_opcode) {
	pdg = cached_pdg;
  }
  else {
  	pdg=get_pdg(instance,opcode);
	cached_instance = instance;
	cached_opcode = opcode;
	cached_pdg = pdg;
  }
  assert(pdg!=NULL);
  pdg->valid=FALSE;
  if (psource != pdest) {
    remove_edge(pdg,psource, instance->state->owner[psource], 
	   pdest, instance->state->owner[pdest]);
  }
  return 0;  
}

int 
do_clear_dependencies(instance_p instance, po_opcode opcode) {
  pdg_p pdg;

  wait_for_end_of_invocation(instance);
  /* Create an empty PDG for the operation. */
  pdg=get_pdg(instance,opcode);
  if (pdg!=NULL) free_pdg(pdg);
  pdg=new_pdg(instance->state->num_parts);
  cached_pdg = pdg;
  cached_instance = instance;
  cached_opcode = opcode;
  pdg->valid=FALSE;
  assert(pdg!=NULL);
  set_pdg(instance,opcode,pdg);
  return 0;  
}

int 
do_set_dependencies(instance_p instance, po_opcode opcode) {
  void *args[4];
  int opid;
  pdg_p pdg;

  args[0] = &(instance->id);
  opid=operation_index(instance->po,(item_p)opcode);
  assert(opid>=0);
  args[1] = &opid;
  pdg=get_pdg(instance,opcode);
  assert(pdg!=NULL);
  args[2] = marshall_pdg(pdg);
  do_operation(instance,call_set_dependencies,args);
  wait_for_end_of_invocation(instance);
  free(args[2]);
  return 0;  
}


/*=======================================================================*/
/********* Local functions corresponding to RTS operations ***************/
/*=======================================================================*/

void 
call_free_instance(int sender, instance_p rinstance, void **args) {
  instance_p instance;

  precondition(args[0]!=NULL);
  instance=get_instance(*((int *)(args[0])));
  precondition(instance!=NULL);
  free_instance(instance);
}

/*=======================================================================*/
/*=======================================================================*/

void 
call_finish_rts(int sender, instance_p rinstance, void **args)
{
  exit(0);
}


void
call_distribute_on_n(int sender, instance_p rinstance, void **args) {
  instance_p instance;
  int **dist;
  
  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  dist=unmarshall_dist(instance->po->num_dims,args[1]);
  distribute_on_n(instance,dist);
  synch_handlers();
  free(dist[0]);
  free(dist[1]);
  free(dist);
}

void
call_distribute_on_list(int sender, instance_p rinstance, void **args) {
  instance_p instance;
  int **dist;
  int *proc_list;
  int num_proc;

  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  proc_list=unmarshall_proc_list(args[1],&num_proc);
  dist=unmarshall_dist(instance->po->num_dims,args[2]);
  distribute_on_list(instance,num_proc,proc_list,dist);
  synch_handlers();
  free(proc_list);
  free(dist[0]);
  free(dist[1]);
  free(dist);
}


/*=======================================================================*/
/*=======================================================================*/

void 
call_rowwise_partitioning(int sender, instance_p rinstance, void **args)
{
  instance_p instance;

  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  precondition(instance!=NULL);

  rowwise_partitioning(instance);
}


/*=======================================================================*/
/*=======================================================================*/

void 
call_columnwise_partitioning(int sender, instance_p rinstance, void **args)
{
  instance_p instance;

  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  precondition(instance!=NULL);

  columnwise_partitioning(instance);
}


/*=======================================================================*/
/*=======================================================================*/

void 
call_log_all_po_timers(int sender, instance_p instance, void **args)
{
  set_p processors;
  char *filename;
  int label;

  filename=((char *)args[0]+sizeof(int));
  label = *(int *) args[2];
  processors=unmarshall_set(args[1]);
  rts_sleep(me % 4);
  if (is_member(processors, me)){
      if (label < 0) label=me;
      log_all_po_timers(filename,label,PO_MICROSEC);
  }
  free_set(processors);
}

/*=======================================================================*/
/*=======================================================================*/

void 
call_new_instance(int sender, instance_p instance, void **args)
{
  po_p po;
  instance_p inst;
  int *base = *((int *) args[2]) == sizeof(int) ? 0 : (int *)(args[2])+1;

  po=get_object(*((int *)(args[0])));
  inst=new_instance(po,(int *)(args[1])+1,base);
  if (me==sender) *(instance_p *) (args[3]) = inst;
}

void 
call_my_partitioning(int sender, instance_p rinstance, void **args)
{
  instance_p instance;
  int *num_parts;
 
  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  num_parts=(int *)(args[1])+1;
  my_partitioning(instance,num_parts);
}
 
/*=======================================================================*/
/*=======================================================================*/

void 
call_my_distribution(int sender, instance_p rinstance, void **args)
{
  instance_p instance;

  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  
  my_distribution(instance,(int *)args[1]+1);
}

/*=======================================================================*/
/*=======================================================================*/

void 
call_change_attribute(int sender, instance_p rinstance, void **args)
{
  instance_p instance;
  po_opcode opcode;
  handler_p handler;

  precondition(args!=NULL);
  instance=get_instance(*((int *)(args[0])));
  assert(instance!=NULL);
  opcode=(po_opcode)get_item((instance)->po->opcode_table,*((int *)args[1]));
  assert(opcode!=NULL);
  handler=get_handler_p(*((int *)args[3]));
  assert(handler!=NULL);
  change_attribute(instance, opcode, (attribute_t )(*((int *)args[2])), handler);
  compute_optim_counter(instance);
}

/*=======================================================================*/

void 
call_set_dependencies(int sender, instance_p rinstance, void **args) {
  instance_p instance;
  po_opcode opcode;
  pdg_p pdg;
  int op_ind = *((int *) args[1]);

  instance=get_instance(*((int *)(args[0])));
  assert(instance!=NULL);
  assert(instance->state!=NULL);
  opcode=(po_opcode)get_item((instance)->po->opcode_table, op_ind);
  assert(opcode!=NULL);
  pdg=get_pdg(instance,opcode);
  if (me==sender) {
    assert(pdg!=NULL);
    pdg->valid=TRUE;
  }
  else {
    if (pdg!=NULL) free_pdg(pdg);
    pdg=unmarshall_pdg(args[2]);
    assert(pdg!=NULL);
    pdg->valid=TRUE;
    set_pdg(instance,opcode,pdg);
  }
  build_deps(pdg, instance->state->owner);
  if (instance->state->num_parts == instance->state->num_owned_parts) {
    /* Leave handlers alone. */
    return;
  }
  if (/* (pdg->with_ldeps==0) && */ (pdg->with_rdeps==0)) {
    change_attribute(instance, opcode, EXECUTION, e_parallel_consistent);
    change_attribute(instance, opcode, TRANSFER, noop);
    instance->optim_counter[op_ind] = 0;
  }
  else {
    change_attribute(instance, opcode, EXECUTION, e_parallel_nonblocking);
    change_attribute(instance, opcode, TRANSFER, t_sender);
    instance->optim_counter[op_ind] = 1;
  }
  compute_optim_counter(instance);
}

void
insert_pdg(instance_p instance, po_opcode opcode, pdg_p pdg)
{
  int i;

  set_pdg(instance,opcode,pdg);
  pdg->with_rdeps = 0;
  pdg->with_ldeps = 0;
  build_deps(pdg, instance->state->owner);
  i = operation_index(instance->po,(item_p)opcode);
  if (instance->state->num_parts == instance->state->num_owned_parts) {
    /* Leave handlers alone. */
    return;
  }
  if (/* (pdg->with_ldeps==0) && */ (pdg->with_rdeps==0)) {
    change_attribute(instance, opcode, EXECUTION, e_parallel_consistent);
    change_attribute(instance, opcode, TRANSFER, noop);
    instance->optim_counter[i] = 0;
  }
  else {
    change_attribute(instance, opcode, EXECUTION, e_parallel_nonblocking);
    change_attribute(instance, opcode, TRANSFER, t_sender);
    instance->optim_counter[i] = 1;
  }
  compute_optim_counter(instance);
}


/*=======================================================================*/
/************************** Other Static Functions ***********************/
/*=======================================================================*/

void *
marshall_dist(int num_dims, int **dist) {
  char *buffer, *b;
  int mdist_size;

  b=buffer=malloc(sizeof(int)*(2*num_dims+1));
  mdist_size = sizeof(int)*(2*num_dims+1);
  memcpy(buffer,&mdist_size,sizeof(int));
  buffer+=sizeof(int);
  memcpy(buffer,dist[0],sizeof(int)*num_dims);
  buffer += sizeof(int)*num_dims;
  memcpy(buffer,dist[1],sizeof(int)*num_dims);
  return b;
}

int **
unmarshall_dist(int num_dims, void *b) {
  int **dist;
  char *buffer = b;

  dist=(int **)malloc(sizeof(int *)*2);
  dist[0]=(int *)malloc(sizeof(int)*num_dims);
  dist[1]=(int *)malloc(sizeof(int)*num_dims);
  buffer += sizeof(int);
  memcpy(dist[0], buffer, sizeof(int)*num_dims);
  buffer += sizeof(int)*num_dims;
  memcpy(dist[1], buffer, sizeof(int)*num_dims);
  return dist;
}


void *
marshall_proc_list(int num_proc, int *list) {
  char *buffer, *b;
  int mproc_list_size;

  b = buffer = malloc(sizeof(int)*(2+num_proc));
  mproc_list_size=sizeof(int)*(2+num_proc);
  memcpy(buffer,&mproc_list_size, sizeof(int));
  buffer += sizeof(int);
  memcpy(buffer,&num_proc, sizeof(int));
  buffer += sizeof(int);
  memcpy(buffer,list,sizeof(int)*num_proc);
  return b;
}

int *
unmarshall_proc_list(void *b, int *num_proc) {
  int *proc_list;
  char *buffer = b;
  
  buffer += sizeof(int);
  memcpy(num_proc,buffer,sizeof(int));
  buffer += sizeof(int);
  proc_list=(int *)malloc(sizeof(int)*(*num_proc));
  memcpy(proc_list,buffer,sizeof(int)*(*num_proc));
  return proc_list;
}
