/*==================================================*/
/*  fft.c 5 Jun 1995                            */
/*==================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "misc.h"
#include "vector.h"
#include "rts_init.h"
#include "precondition.h"

int k;                 /* n = 2 ** k */

extern size_t strlen(const char *s);

static int group_size;
static int me;
static int proc_debug;

char *argument, *name_file;
instance_p vector;
int t, i;

int k;                   /* n = 2 ** k */ 
int k_l_1;               /* k_l_1 = k -l - 1 */
int l;                   /* iteration number */
ARGUM argum;             /* n, verbose */
int number_part_per_proc; /* the number of partitions per processor */
handler_p handler;
static char filename[256];

static po_timer_p timer;

/* ================================================ */
int main(int argc, char *argv[])
{
  void *args[1];

  if (argc<6) {
    if (me==0) {
      fprintf(stderr,
	      "Usage: %s -k k -file file [-attr exec_attribute] ", argv[0]);
      fprintf(stderr, "[-num_parts nparts]\n");
    }
    exit(1);
  }

  init_rts(&argc,argv);
  get_group_configuration(&me,&group_size,&proc_debug);

  argument=get_argument(argc,argv,"-k");
  assert(argument != NULL);
  k=atoi(argument);

  name_file = get_argument(argc, argv, "-file");
  assert(name_file != NULL);

  argum.number = 1 << k;                         /* args for Init */
 
  number_part_per_proc = 1;
  argument=get_argument(argc,argv,"-num_parts");
  if (argument!=NULL) number_part_per_proc=atoi(argument);

  argument=get_argument(argc,argv,"-attr");
  if ((argument==NULL) || (strcmp(argument,"nonblocking")==0)) {
    handler=e_parallel_nonblocking;
  }
  else
    if (strcmp(argument,"blocking")==0) {
      handler=e_parallel_blocking;
    }
    else 
      if (strcmp(argument,"control")==0)
	handler=e_parallel_control;
      else {
	if (me==0) {
	  fprintf(stderr,"%s: Unknown Prefetching Strategy\n", argument);
	  exit(1);
	}
	exit(0);
      }

  new_fft_class(me,group_size,proc_debug);
  if (me==0) {
    int parts[1];
    int *dist[2];
    int dd[2];

    vector = fft_instance(argum.number,handler);

    parts[0] = group_size* number_part_per_proc;
    do_my_partitioning(vector, parts);

    dist[0]=&dd[0];
    dist[1]=&dd[1];
    dd[0] = group_size;
    dd[1] = BLOCK;
    do_distribute_on_n(vector, dist);

    assert(vector->state->data!=NULL);
    do_operation(vector,(po_opcode)Initial_A, NULL);
    wait_for_end_of_invocation(vector);
    
    timer = new_po_timer("fft");
    start_po_timer(timer);

    k_l_1 = k - 1;
    for (l = 0; l < k; l++) {
      args[0]=&k_l_1;
      do_operation(vector,(po_opcode)insert_pdg_A_computation,args);
      wait_for_end_of_invocation(vector);
      do_operation(vector,(po_opcode)A_computation,args);
      /* wait_for_end_of_invocation(vector); */
      k_l_1--;
    }

    end_po_timer(timer);
    add_po_timer(timer);
    printf("#CPUS %d, k %d\t %9.2f\n", group_size, k, read_last(timer, PO_SEC));
    sprintf(filename,"%s.timers", argv[0]);
    do_log_all_po_timers(filename, NULL, group_size);
    do_free_instance(vector);
  }
  finish_rts();
  return 0;
}


