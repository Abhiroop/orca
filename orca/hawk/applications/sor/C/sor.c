#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "rts_init.h"
#include "misc.h"
#include "grid.h"
#include "assert.h"

static int num_rows;
static int num_cols; 

static double MaxUpdate;
static int num_iteration;

#define M_PI            3.14159265358979323846
#define TOLERANCE       0.001

static int color;

static int group_size;
static int me;
static int proc_debug;

static double r, stopdiff;
static po_timer_p timer;

/* =============== */
int main(int argc, char *argv[])
{
  char *argument;
  instance_p matrix;
  boolean stable;
  handler_p handler;

  init_rts(&argc,argv);
  get_group_configuration(&me,&group_size,&proc_debug);

  argument=get_argument(argc,argv,"-rows");
  assert(argument!=NULL);
  num_rows=atoi(argument);
  argument=get_argument(argc,argv,"-cols");
  assert(argument!=NULL);
  num_cols=atoi(argument);
  argument=get_argument(argc,argv,"-attr");
  if (argument==NULL)
    handler=e_parallel_nonblocking;
  else
    if (strcmp(argument,"nonblocking")==0)
      handler=e_parallel_nonblocking;
    else
      if (strcmp(argument,"blocking")==0)
	handler=e_parallel_blocking;
      else
        if (strcmp(argument,"control")==0)
       	  handler=e_parallel_control;
        else
          handler=e_parallel_consistent;

  r = 0.5 * (cos(M_PI/num_cols) + cos(M_PI/num_rows));
  omega = 2.0/(1.0 + sqrt(1.0 - r*r));
  stopdiff = TOLERANCE / (2.0 - omega);

  new_grid_class(me,group_size,proc_debug);
  if (me==0)
    {
      void *args[3];
      int *dist[2];
      int dd[4];
      
      matrix=grid_instance(num_rows, num_cols, handler);

      do_rowwise_partitioning(matrix);

      dist[0]=&dd[0];
      dist[1]=&dd[2];
      
      dist[0][0] = group_size;
      dist[0][1] = 1; 
      dist[1][0] = BLOCK;
      dist[1][1] = BLOCK;
      do_distribute_on_n(matrix, dist);
      do_create_gather_channel(matrix);

      do_operation(matrix,(po_opcode)InitGrid,args);
      wait_for_end_of_invocation(matrix);

      stable=FALSE;
      num_iteration=0;
      timer = new_po_timer("sor");

      printf("SOR, %dx%d\n", num_rows, num_cols);

      start_po_timer(timer);
      while ((!stable)) {
	color=BLACK;
	args[0] = &color;
	args[1] = &MaxUpdate;

	do_operation(matrix,(po_opcode)UpdateGrid, args);
	wait_for_end_of_invocation(matrix);
	stable=(MaxUpdate<=stopdiff);	

	color=RED;
	do_operation(matrix,(po_opcode)UpdateGrid, args);
	wait_for_end_of_invocation(matrix);	

	stable=stable&&(MaxUpdate<=stopdiff);
	num_iteration++;
      }
      end_po_timer(timer);
      add_po_timer(timer);
      printf("#CPUS %d    %6.3f sec     niter: %d\n", group_size, read_last(timer, PO_SEC), num_iteration);
      printf("%30s\t%5.2f\n", "time per iteration (msec):",
             read_last(timer, PO_MILLISEC) / num_iteration);

      
      free_po_timer(timer);
      /* do_log_all_po_timers(argv[0], NULL, group_size); */
      do_free_instance(matrix);
    }

  finish_rts();

  return 0;
}

