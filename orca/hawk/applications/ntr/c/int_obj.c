#include "communication.h"
#include "po.h"
#include "util.h"
#include "synchronization.h"
#include "collection.h"
#include "int_obj.h"
#include "amoeba.h"
#include "semaphore.h"

static int group_size;
static int me;
static int proc_debug;
static po_p int_obj_class;

extern exit();
extern atoi();
extern millisleep();
extern malloc();
extern free();

static int count;
static int max_count;
static semaphore sema;

/* ======================================================= */
/* ======================================================= */
void up_int(int sender, instance_p instance, void **out_arg)
{
  sema_up(&sema);
}

/* ======================================================= */
/* ======================================================= */
void down_int(int sender, instance_p instance, void **out_arg)
{
  int *out_int=out_arg[0];

  (*out_int)=-1;
  if (count<max_count) {
    sema_down(&sema);
    count++;
    (*out_int)=count;
  }
}


/* ======================================================= */
/* ======================================================= */
void init_int(int sender, instance_p instance, void **in_arg)
{
  int *max_level=in_arg[0];

  sema_init(&sema,0);
  count=0;
  max_count=(*max_level);
}

/* ======================================================= */
/* ======================================================= */
static po_pardscr_t down_int_Descr[] = {
	{ sizeof(int), OUT_PARAM, 0, 0, 0}
};

static po_pardscr_t int_int_Descr[] = {
	{ sizeof(int), IN_PARAM, 0, 0, 0}
};


int new_int(int moi, int gsize, int pdebug)
{
  precondition(moi>=0);
  precondition(moi<gsize);
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;

  int_obj_class=new_po("int_obj", 1, sizeof(int), (void *)0);
  if (int_obj_class==NULL) return -1;

  new_po_operation(int_obj_class,"up_int",(po_opcode)up_int,
		   OrcaRepWrite, 0,0,NULL);
  new_po_operation(int_obj_class,"down_int",(po_opcode)down_int,
		   OrcaRepWrite, 1, down_int_Descr,NULL);
  new_po_operation(int_obj_class,"init_int",(po_opcode)init_int,
		   OrcaRepWrite, 1, int_int_Descr,NULL);
  synch_handlers();
  return 0;
}


/* ======================================================= */
/* ======================================================= */
instance_p int_instance(set_p processors)
{
  instance_p int_obj;

  int_obj=new_instance(int_obj_class, &group_size, (void *)0);
  change_attribute(int_obj, (po_opcode)up_int, TRANSFER, t_no_transfer);
  change_attribute(int_obj, (po_opcode)down_int, TRANSFER, t_no_transfer);
  synch_handlers();
  default_partitioning(int_obj,processors);
  block_distribution(int_obj,processors);
  synch_handlers();
  return int_obj;
}

