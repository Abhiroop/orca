#ifndef __rts_object__
#define __rts_object__

#include "sys_po.h"
#include "instance.h"
#include "set.h"
#include "consistency.h"
#include "po_invocation.h"

int init_rts_object(int moi, int gsize, int pdebug, int max_arg_size,int *argc, char **argv);
int finish_rts_object(void);

instance_p do_new_instance(po_p po, int *length, int *base);
int do_free_instance(instance_p instance);
int do_my_partitioning(instance_p instance, int *num_parts);
int do_rowwise_partitioning(instance_p instance);
int do_columnwise_partitioning(instance_p instance);
int do_block_distribution(instance_p instance, set_p processors);
int do_cyclic_distribution(instance_p instance, set_p processors);
int do_create_gather_channel(instance_p instance);
int do_log_all_po_timers(char *filename, set_p processors, int label);
int do_my_distribution(instance_p instance, int *owner);

int do_distribute_on_list(instance_p instance, int num_proc, int *proc_list, 
			  int **dist);
int do_distribute_on_n(instance_p instance, int **dist);
int do_change_attribute(instance_p instance, po_opcode opcode, 
			attribute_t attribute, handler_p handler);
int do_clear_dependencies(instance_p, po_opcode);
int do_set_dependencies(instance_p, po_opcode);
int do_add_dependency(instance_p, po_opcode, ...);
int do_remove_dependency(instance_p, po_opcode, ...);
int do_add_pdependency(instance_p, po_opcode, int , int );
int do_remove_pdependency(instance_p, po_opcode, int , int );
int do_finish_rts_object(void);
void oc_call_finish_rts(void);
void insert_pdg(instance_p instance, po_opcode opcode, pdg_p pdg);
void add_ops(po_p po);

#endif
