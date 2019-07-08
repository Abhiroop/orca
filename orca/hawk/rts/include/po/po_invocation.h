/* Invocation module header file. */
#ifndef __invocation__
#define __invocation__

#include "po.h"
#include "instance.h"
#include "reduction_function.h"
#include "message.h"

typedef struct marshalled_invocation_s *marshalled_invocation_p, 
marshalled_invocation_t;

struct marshalled_invocation_s {
  int sender;
  int instance_id;
  int operation_id;
  void **args;     /* only used by the one who sends the operation. */
};

int init_invocation_module(int moi, int gsize, int pdebug, int max_arg_size, int *argc, char **argv);
int finish_invocation_module(void);

int do_operation(instance_p instance, po_opcode opcode, void **args);

int synch_handlers(void);
int synch_invocation(instance_p instance);
int wait_for_end_of_invocation(instance_p instance);
int marshalled_invocation_size(void);

int get_handler_id(handler_p handler);
handler_p get_handler_p(int handler);

/********************/
/* Noop Handler. */
/********************/
void noop(int sender, instance_p instance, po_opcode opcode, void **args);

/********************/
/* Invocation Handlers. */
/********************/
void i_local(int sender, instance_p instance, po_opcode opcode, void **args);

void i_collective(int sender, instance_p instance, po_opcode opcode, void **args);

void i_multicast(int sender, instance_p instance, po_opcode opcode, void **args);

void i_unicast(int sender, instance_p instance, po_opcode opcode, void **args);

/********************/
/* Transfer Handlers. */
/********************/
void t_collective_caller(int sender, instance_p instance, po_opcode opcode,   
			 void **args);

void t_collective_all(int sender, instance_p instance, 
		      po_opcode opcode,   
		      void **args);

void t_receiver(int sender, instance_p instance, 
		po_opcode opcode,   
		void **args);

void t_sender(int sender, instance_p instance, 
	      po_opcode opcode,   
	      void **args);

void t_no_transfer(int sender, instance_p instance, 
		   po_opcode opcode,   
		   void **args);

/********************/
/* Execution Handlers. */
/********************/
void e_sequential_local(int sender, instance_p instance, 
			po_opcode opcode,   
			void **args);

void e_sequential_replicated(int sender, instance_p instance, 
			     po_opcode opcode,   
			     void **args);

void e_parallel_blocking(int sender, instance_p instance, 
			 po_opcode opcode,   
			 void **args);

void e_parallel_consistent(int sender, instance_p instance, 
			   po_opcode opcode,   
			   void **args);

void e_parallel_nonblocking(int sender, instance_p instance, 
			    po_opcode opcode,   
			    void **args);

void e_parallel_control(int sender, instance_p instance, 
			po_opcode opcode,   
			void **args);

/********************/
/* Return Handlers. */
/********************/
void r_collective_all(int sender, instance_p instance, 
		      po_opcode opcode,   
		      void **args);

void r_collective_caller(int sender, instance_p instance, 
			 po_opcode opcode,   
			 void **args);

void r_local(int sender, instance_p instance, 
	     po_opcode opcode,   
	     void **args);

void r_unicast(int sender, instance_p instance, 
	       po_opcode opcode,   
	       void **args);

/********************/
/* Commit Handlers. */
/********************/
void c_commit_owned(int sender, instance_p instance, 
		    po_opcode opcode,   
		    void **args);

void c_save_all(int sender, instance_p instance, 
		po_opcode opcode,   
		void **args);

void c_no_commit(int sender, instance_p instance, 
		 po_opcode opcode,   
		 void **args);

extern handlers_t ParRead;
extern handlers_t ParWrite;
extern handlers_t SeqWrite;
extern handlers_t SeqRead;
extern handlers_t RepRead;
extern handlers_t SeqInit;
extern handlers_t RtsOp;
extern handlers_t OrcaLocalRead;
extern handlers_t OrcaRemoteRead;
extern handlers_t OptOrcaLocalRead;
extern handlers_t OrcaLocalWrite;
extern handlers_t OrcaRepRead;
extern handlers_t OrcaRepWrite;

int get_handler_id(handler_p handler);
handler_p get_handler_p(int handler);

/* Some hooks to handle dynamic IN parameter types for Orca: */
extern void (*opmarshall_func)(message_p mp, void **arg, void *d);
extern char *(*opunmarshall_func)(char *p, void ***arg, void *d, void **args, int sender, instance_p ip);
extern void (*opfree_in_params_func)(void **args, void *d, int remove_outs);

/* Hook to handle gathered parameter in orca: */
extern void *(*p_gatherinit)(instance_p p, void *a, void *d);

#endif
