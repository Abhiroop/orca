#ifndef __sys_po__
#define __sys_po__

#define MAX_DIMS 2
#define operation_index(po,op) item_index((po)->opcode_table,(item_p)(op))


typedef struct instance_s *instance_p, instance_t;
typedef struct consistency_s *consistency_p, consistency_t;
typedef struct state_s *state_p, state_t;
typedef struct partition_s *partition_p, partition_t;
typedef struct pdg_s *pdg_p, pdg_t;
typedef void (*po_opcode)(int sender, instance_p instance, void **args);
typedef pdg_p (*build_pdg_p)(instance_p instance);
typedef pdg_p (*dynamic_pdg_p)(instance_p instance, void **args);
typedef struct po_s *po_p, po_t;
typedef struct po_operation_s *po_operation_p, po_operation_t;
typedef void (*handler_p)(int sender, instance_p instance, po_opcode opcode,void **args);
typedef struct handlers_s handlers_t, *handlers_p;

typedef enum 
{
  INVOCATION,
  TRANSFER,
  EXECUTION,
  RETURN,
  COMMIT
} attribute_t;
  
#include "util.h"
#include "pdg.h"
#include "reduction_function.h"
#include "collection.h"

struct handlers_s
{
  boolean parallel_update;
  handler_p i_handler;
  handler_p t_handler;
  handler_p e_handler;
  handler_p r_handler;
  handler_p c_handler;
};

typedef struct po_pardscr_s po_pardscr_t, *po_pardscr_p;

/* Flags for parameters. */
#define IN_PARAM	0x1
#define OUT_PARAM	0x2
#define GATHER		0x4
#define REDUCE		0x8

/* A parameter descriptor. */
struct po_pardscr_s
{
  int par_size;		/* static size of parameter */
  int par_flags;	/* the above flags */
  reduction_function_p par_func;
			/* if is_reduce() the reduction function, or else
			   not used.
			*/
  void *par_d;		/* type descriptor */
};

struct po_operation_s {
  int optim_counter; 
  po_p po;
  po_opcode opcode;
  char *name;
  handlers_t handlers;
  int id;
  int nparams;
  po_pardscr_p param_descrs;
#if 0
  int *out_index;
#endif
  build_pdg_p build_pdg;
  dynamic_pdg_p dyn_pdg;
  void *opdescr;
};

struct po_s {
  char *name;
  int id;
  map_p op_table;      /* Table of operation descriptors. */
  map_p opcode_table;  /* Table of pointers to operation code. */
  int num_dims;
  int element_size;
  void *tpdscr;		/* type descriptor of object. */
#if 0
  int cnt;		/* for counting return_channel numbers */
#endif
};

int init_po_module(int me, int group_size, int pdebug);
int finish_po_module(void);

#define is_in_param(p)	((p)->par_flags & IN_PARAM)
#define is_out_param(p)	((p)->par_flags & OUT_PARAM)
#define is_gather(p)	((p)->par_flags & GATHER)
#define is_reduce(p)	((p)->par_flags & REDUCE)

po_p new_po(char *name, int num_dims, int element_size, void *tpdscr);
int free_po(po_p po);

po_operation_p new_po_operation(po_p po, char *name, po_opcode opcode, handlers_t handlers,
		     int npars, po_pardscr_p param_descrs,
		     build_pdg_p build_pdg);

int free_po_operation(po_p po, po_opcode opcode);
po_p get_object(int o_id);

#endif
