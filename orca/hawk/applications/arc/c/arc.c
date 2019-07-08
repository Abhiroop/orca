/*==================================================*/
/*  ArcConsistency.c 24 Jul 1995                    */
/*  graph solution                                  */
/*==================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "po_rts_object.h"
#include "misc.h"
#include "amoeba.h"
#include "module/syscall.h"
#include "domain.h"
#include "arc.h"
#include "rts_init.h"

static int group_size;
static int me;
static int proc_debug;
static char filename[256];

char *name_file;

/* =============== */

int main(int argc, char *argv[])
{

    char *argument;
    instance_p domain;
    BOOL working;
    int proc[MAXVARS];        /* what variable is assigned to each
				 processor. */
    IN_ARGUM in_argum;
    OUT_ARGUM out_argum;
    int nr_modif, con_checks, iteration;
    BOOL no_solution;
    long start, end;
    set_p processors;
    handler_p handler;
    int number_part_per_proc; /* the number of partitions per processor */

    init_rts(&argc,argv);
    get_group_configuration(&me,&group_size,&proc_debug);
    in_argum.verbose = 0;
    argument=get_argument(argc,argv,"-verb");
    if (argument!=NULL)
      in_argum.verbose=atoi(argument);
    name_file = get_argument(argc, argv, "-file");
    assert(name_file != NULL);

    argument=get_argument(argc,argv,"-n");
    if (argument!=NULL)
      in_argum.n = atoi(argument);

    argument=get_argument(argc,argv,"-attr");
    if (argument==NULL)
      handler=e_parallel_nonblocking;
    else
      if (strcmp(argument,"nonblocking")==0)
	handler=e_parallel_nonblocking;
      else
	if (strcmp(argument,"blocking")==0)
	  handler=e_parallel_blocking;
	else {
	  assert(strcmp(argument,"control")==0);
	  handler=e_parallel_control;
	}
    
    number_part_per_proc = 1;
    argument=get_argument(argc,argv,"-num_parts");
    if (argument!=NULL) number_part_per_proc=atoi(argument);

    new_domain_class(me,group_size,proc_debug);
    if (me==0) {

      void *args[2];
      domain = domain_instance(in_argum.n,handler);

      /* read the data file and create the partition file */
      Init_val(&in_argum.n, &in_argum.a, name_file, proc, group_size);
      
      processors=new_set(group_size);
      full_set(processors);
      do_even_partitioning(domain, group_size*number_part_per_proc);
      do_block_distribution(domain, processors);
      free_set(processors);
/*
      do_create_gather_channel(domain);
*/
      args[0]=name_file;

      do_operation(domain,(po_opcode)Init_D,args);
      wait_for_end_of_invocation(domain);
      
      start=sys_milli();	
      no_solution = FALSE;
      con_checks = 0;
      nr_modif = 0;
      working = TRUE;
      iteration = 0;
      
      while (working && (!no_solution)) {
	working = FALSE;
	args[0]=&in_argum;
	args[1]=&out_argum;
	do_operation(domain,(po_opcode)Spac_Loop,args);
	wait_for_end_of_invocation(domain);
	do_operation(domain,(po_opcode)Actual_D,args);
	wait_for_end_of_invocation(domain);
	working = out_argum.modif;
	no_solution |= out_argum.no_solution;
	nr_modif += out_argum.nr_modif;
	con_checks += out_argum.con_checks;
	iteration++;
      }
      
      end=sys_milli();
      
      if (!no_solution) {
	args[0]=&in_argum;
	do_operation(domain,(po_opcode)Get_Print_D,args);
	wait_for_end_of_invocation(domain);
      }
      else
	printf("\n No solution! \n");
      
      if(in_argum.verbose)
	printf("\n no_modif = %i, con_checks = %i\n", 
	       nr_modif, con_checks);
      printf("%d \t %d\n", group_size, (int )(end-start));
      sprintf(filename,"%s.timers", argv[0]);
      do_log_all_po_timers(filename, NULL, group_size);
      do_finish_rts_object();
    }
    else
      thread_exit();
    
    return 0;
  }










