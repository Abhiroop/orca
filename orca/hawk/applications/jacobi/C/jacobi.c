#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "rts_init.h"
#include "po_rts_object.h"
#include "misc.h"
#include "po_timer.h"
#include "vector.h"

#define MAX_VALUE  4.0

static int group_size;
static int me;
static int proc_debug;
static int vector_size;
static boolean verbose=FALSE;

/* =============== */
int 
main(int argc, char *argv[]) {
  char *argument;
  instance_p vector;
  boolean stable;
  static po_timer_p timer;
  int num_iteration;
  double MaxUpdate;

  init_rts(&argc,argv);
  get_group_configuration(&me,&group_size,&proc_debug);

  argument=get_argument(argc,argv,"-size");
  assert(argument!=NULL);
  vector_size=atoi(argument);

  argument=get_argument(argc,argv,"-verbose");
  if (argument!=NULL) verbose=atoi(argument);

  init_vector_class(me,group_size,proc_debug);

  if (me==0)
    {
      void *args[2];
      int parts[1];
      int *dist[2];
      int dd[2];

      printf("Jacobi, size %d\n", vector_size);
      vector=vector_instance(vector_size);
      assert(vector!=NULL);

      parts[0] = group_size;
      do_my_partitioning(vector,parts);

      dist[0] = &dd[0];
      dist[1] = &dd[1];
      dd[0] = group_size;
      dd[1] = BLOCK;
      do_distribute_on_n(vector, dist);

      do_create_gather_channel(vector);

      do_operation(vector,(po_opcode)init_system,args);
      wait_for_end_of_invocation(vector);

      stable=FALSE;
  
      timer = new_po_timer("jacobi");
      start_po_timer(timer);

      args[0] = &MaxUpdate;
      num_iteration=0;
      while (!stable) {
	  if (verbose) fprintf(stderr,"Iteration %d\n", num_iteration);
          args[0] = &MaxUpdate;
	  do_operation (vector,(po_opcode)update_x,args);
	  wait_for_end_of_invocation(vector);
          /* do_operation(vector,(po_opcode)print_system,args);
          wait_for_end_of_invocation(vector); */
	  num_iteration++;
	  /* printf("Iteration %d, MaxUpdate = %g\n", num_iteration, MaxUpdate); */
	  stable=(MaxUpdate<=PRECISION);
	}
      end_po_timer(timer);
    
      /*  
      do_operation(vector,(po_opcode)print_system,args);
      wait_for_end_of_invocation(vector);
      */
#ifdef CHECK
      do_operation(vector,(po_opcode)do_check_result,args);
      wait_for_end_of_invocation(vector);
#endif

      printf("#CPUS %d    %6.3f sec    niter: %d\n", group_size, read_last(timer, PO_SEC), num_iteration);
      printf("%30s\t%5.2f\n", "time per iteration (msec):", 
	     read_last(timer, PO_MILLISEC) / num_iteration);

      free_po_timer(timer);
      do_free_instance(vector);
    }

  finish_rts();

  return 0;
}
