#include <stdlib.h>

#include "po.h"
#include "assert.h"
#include "precondition.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static map_p object_table;


/*=======================================================================*/
/* Initialized the module. */
/*=======================================================================*/

int 
init_po_module(int moi, int gsize, int pdebug) {
  precondition((moi>=0)&&(moi<gsize));

  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_map(me,group_size,proc_debug);
  object_table=new_map();
  assert(object_table!=NULL);
  return 0;
}


/*=======================================================================*/
/* Finishes the module. */
/*=======================================================================*/

int 
finish_po_module(void) {
  if (--initialized) return 0;
  free_map(object_table);
  finish_map();
  return 0;
}

/*=======================================================================*/
/* Creates a new object "type". */
/*=======================================================================*/

po_p 
new_po(char *name, int num_dims, int element_size, void *tpdscr) {
  po_p po;

  precondition(name!=NULL);
  precondition(num_dims<=MAX_DIMS);
  precondition(element_size>0);

  po=(po_p)malloc(sizeof(po_t));
  assert(po!=NULL);
  po->name=name;
  po->op_table=new_map();
  po->opcode_table=new_map();
  assert(po->op_table!=NULL);
  po->num_dims=num_dims;
  po->element_size=element_size;
  po->id=insert_item(object_table,(item_p)po);
  po->tpdscr = tpdscr;
#if 0
  po->cnt = 0;
#endif
  add_ops(po);
  return po;
}

/*=======================================================================*/
/* Frees space allocated for an object. */
/*=======================================================================*/

int 
free_po(po_p po) {
  int r;

  precondition(po!=NULL);

  r=remove_item(object_table,po->id);
  assert(r>=0);
  free_map(po->opcode_table);
  free_map(po->op_table);
  free(po);
  return 0;
}

/*=======================================================================*/
/* Registers a new operation with an object. */
/*=======================================================================*/

po_operation_p
new_po_operation(po_p po, char *name, po_opcode opcode, handlers_t handlers,
		     int nparams, po_pardscr_p param_descrs,
		     build_pdg_p build_pdg) {
  po_operation_p po_operation;
  int opid;
#if 0
  int i;
#endif

  precondition(po!=NULL);
  precondition(name!=NULL);
  precondition(opcode!=NULL);

  opid=insert_item(po->opcode_table,(item_p )opcode);
  assert(opid>=0);
  po_operation=(po_operation_p)malloc(sizeof(po_operation_t));
  assert(po_operation!=NULL);
  po_operation->id=insert_item(po->op_table,(item_p )po_operation);
  po_operation->name=name;
  assert(po_operation->id>=0);
  if (build_pdg!=NULL)
    po_operation->optim_counter=1;
  else
    po_operation->optim_counter=0;
  po_operation->opcode=opcode;
  po_operation->handlers=handlers;
  po_operation->nparams = nparams;
  po_operation->param_descrs = param_descrs;
  po_operation->build_pdg=build_pdg;
  po_operation->po=po;
  po_operation->opdescr=NULL;
  po_operation->dyn_pdg = NULL;
#if 0
  po_operation->out_index = malloc(nparams * sizeof(int));
  for (i = 0; i < nparams; i++) {
	if (po_operation->param_descrs[i].par_flags & OUT_PARAM) {
		po_operation->out_index[i] = po->cnt;
		po->cnt++;
	}
	else po_operation->out_index[i] = -1;
  }
#endif
  return po_operation;
}


/*=======================================================================*/
/* Releases the operation on an object. */
/*=======================================================================*/

int 
free_po_operation(po_p po, po_opcode opcode) {
  int opid;
  po_operation_p po_operation;
  
  precondition(po!=NULL);
  precondition(opcode!=NULL);

  opid=item_index(po->opcode_table,(item_p)opcode);
  assert(opid>=0);
  remove_item(po->opcode_table,opid);
  po_operation=get_item(po->op_table,opid);
  remove_item(po->op_table,opid);
#if 0
  free(po_operation->out_index);
#endif
  free(po_operation);
  return 0;
}

  
/*=======================================================================*/
/* Returns the pointer to an object given its id. */
/*=======================================================================*/

po_p 
get_object(int id){
  po_p obj;

  precondition(id>=0);

  obj=(po_p)get_item(object_table,id);
  assert(obj!=NULL);
  return obj;
}


