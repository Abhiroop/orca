/*==================================================*/
/*  Narrow-Band Tracking Radar.                     */
/*  March 1996.                                     */
/*==================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rts_init.h"
#include "matrix.h"
#include "int_obj.h"
#include "amoeba.h"
#include "module/mutex.h"
#include "thread.h"
#include "module/syscall.h"

extern sleep();

int k;                 /* n = 2 ** k */

/* the values for real and imaginar part of w powers */
double wr[MAXN];
double wi[MAXN];

static int group_size;
static int me;
static int proc_debug;
static int sleeptime;
static mutex sleeplock;
po_timer_p computation_timer;
po_timer_p a_computation_timer;
po_timer_p a_convert_timer;
po_timer_p a_sum_timer;
po_timer_p a_threshold_timer;

channel_p wch;
set_p worker_set;
po_time_t *timings;
int *tickets;

char *argument, *name_file;
instance_p *matrix, up_obj, down_obj;
int num_workers;
int num_processors;
int num_matrices;
long start, end;
int t, i;
int count;

int k;                   /* n = 2 ** k */ 
int k_l_1;               /* k_l_1 = k -l - 1 */
int l;                   /* iteration number */

set_p processors;
ARGUM argum;             /* n, verbose */
int number_part_per_proc; 
/* the number of partitions per processor */
 
void reader_process();
void worker_process();
 
/*======================= */
/*======================= */
int read_input_data(char *name, int size) {

  int row, i;
  FILE *input;
  
  input = fopen(name, "r");
  if(!input) {
    perror(name);
    printf("\n!!! %i cannot read file %s\n", me, name);
    exit(0);
  }
  
  i=0;
  for (row = 0; row < size ; row++, i++) 
    fscanf(input, "%lf %lf", &(wr[i]), &(wi[i]));

  fclose(input);

  return 0;
}

/* =============== */
int main(int argc, char *argv[])
{
  void *in_arg[1];
  
  /* geting the arguments : n, k, name_file */

  if (argc==1) {
    if (me==0) {
      fprintf(stderr, "Usage: %s -workers num_workers", argv[0]);
      fprintf(stderr, " -m num_matrices -k k -file file [-part nparts]\n");
    }
    exit(1);
  }

  init_rts(&argc,argv);
  get_group_configuration(&me,&group_size,&proc_debug);

  number_part_per_proc = 1;
  argument=get_argument(argc,argv,"-part");
  if (argument!=NULL) number_part_per_proc=atoi(argument);

  argument=get_argument(argc,argv,"-workers");
  assert(argument!=NULL);
  num_workers = atoi(argument);
  assert(((group_size-1) % num_workers)==0);
  num_processors = (group_size-1) / num_workers;
  
  sleeptime=200;
  argument=get_argument(argc,argv,"-sleep");
  if (argument!=NULL) {
    sleeptime=atoi(argument);
    assert(sleeptime>0);
  }

  matrix=(instance_p *)malloc(num_workers*sizeof(instance_p));
  argument = get_argument(argc, argv, "-m");
  assert(argument!=NULL);
  num_matrices = atoi(argument);

  argument=get_argument(argc,argv,"-k");
  assert(argument != NULL);
  k=atoi(argument);

  name_file = get_argument(argc, argv, "-file");
  assert(name_file != NULL);

  argum.number = 1 << k;                         /* args for Init */
 
  number_part_per_proc = 1;
  argument=get_argument(argc,argv,"-part");
  if (argument!=NULL) number_part_per_proc=atoi(argument);

  /* Create classes and instances. */
  new_int(me,group_size,proc_debug);

  worker_set=new_set(group_size);
  for (i=0; i<(group_size-1); i++)
    if ((i % num_processors)==0) add_member(worker_set,i);
  add_member(worker_set,group_size-1);
  wch=new_channel(barrier,binomial_tree,0,NULL,worker_set);

  if (is_member(worker_set,me))
    {
      wch->timer->min_timer=20000;
      wch->timer->interv_timer=20000;
    }
  up_obj=int_instance(worker_set);
  down_obj=int_instance(worker_set);

  /* Create fft class and one instance per worker process. */
  new_fft_class(me,group_size,proc_debug);
  for (i=0; i<num_workers; i++)
    matrix[i] = fft_instance(argum.number);
  
  read_input_data(name_file,argum.number);
  synch_handlers();
  num_matrices=num_matrices+2*num_workers;
  /* If I am a worker process, partition, distribute, 
     and process matrices. */
  if (me==(group_size-1)) {
    in_arg[0]=&num_matrices;
    do_operation(up_obj,(po_opcode)init_int,in_arg);
    wait_for_end_of_invocation(up_obj);
  }
  else if ((me % num_processors)==0)  {
    /* Determine the processors that will process a 
       matrix for this worker. */
    processors=new_set(group_size);
    for (i=0; i<num_processors; i++)
      add_member(processors,me+i);
    do_even_partitioning(matrix[me/num_processors],
			 num_processors* number_part_per_proc);
    do_block_distribution(matrix[me/num_processors],processors);
    assert(matrix[me/num_processors]->state->data!=NULL);
    free_set(processors);
  }
  sleep(5);
  timings=(po_time_t *)malloc(sizeof(po_time_t)*(num_matrices+1));
  tickets=(int *)malloc(sizeof(int)*(num_matrices+1));
  count=0;
  synch_handlers();
  if (me==(group_size-1)) {
    reader_process();
  }
  else if ((me % num_processors)==0)  
    worker_process();


  /* Wait for all processes to finish. */
  if (is_member(worker_set,me)) {
    barrier(wch);
    wait_on_channel(wch);
  }
  end=sys_milli();

/*   for (i=0; i<count; i++) { */
/*     printf("%d \t %10.3f\n", group_size-1, timings[i]); */
/*   } */

  if (is_member(worker_set,me)) {
    barrier(wch);
    wait_on_channel(wch);
  }

  if (me==(group_size-1)) {
    printf("%d \t %9.2f\n", num_workers, (float )(end - start)/1000); 
    do_finish_rts_object();
  }

  thread_exit();
  return 0;
}


/* ======================================================================= */
void reader_process() {

  mu_init(&sleeplock);
  mu_lock(&sleeplock);
  for (t=1; t<=num_workers; t++) {
    /* Fake reading matrix. (Takes too much time on Amoeba...) */
    mu_trylock(&sleeplock,sleeptime);
    do_operation(up_obj,(po_opcode)up_int,NULL);
    wait_for_end_of_invocation(up_obj);
  }
  barrier(wch);
  wait_on_channel(wch);

  for (t=1; t<=num_workers; t++) {
    /* Fake reading matrix. (Takes too much time on Amoeba...) */
    mu_trylock(&sleeplock,sleeptime);
    do_operation(up_obj,(po_opcode)up_int,NULL);
    wait_for_end_of_invocation(up_obj);
  }
  barrier(wch);
  wait_on_channel(wch);

  start=sys_milli();
  for (t=1; t<=num_matrices+2*num_workers; t++) {
    /* Fake reading matrix. (Takes too much time on Amoeba...) */
    mu_trylock(&sleeplock,sleeptime);
    do_operation(up_obj,(po_opcode)up_int,NULL);
    wait_for_end_of_invocation(up_obj);
  }
}

/* ======================================================================= */
void do_computation() {
  void *in_arg[2];

  k_l_1 = k - 1;	  
    
  /* Initialize the matrix. */
  do_operation(matrix[me/num_processors],(po_opcode)Initial_A, NULL);
  wait_for_end_of_invocation(matrix[me/num_processors]);
  
  for (l = 0; l < k; l++) {
    in_arg[0]=&k_l_1;
    do_operation(matrix[me/num_processors],
		 (po_opcode)insert_pdg_A_computation, in_arg);
    wait_for_end_of_invocation(matrix[me/num_processors]);
    do_operation
      (matrix[me/num_processors],(po_opcode)A_computation, in_arg);
    wait_for_end_of_invocation(matrix[me/num_processors]);
    
    k_l_1--;
    
  }
}

/* ======================================================================= */
void worker_process() {
  
  int start;
  void *out_arg[1];

  /* Use ticket object to pick a new matrix to process. */
  
  computation_timer=new_po_timer("Computation");
  a_computation_timer=new_po_timer("a_computation");
  a_convert_timer=new_po_timer("a_convert");
  a_sum_timer=new_po_timer("a_sum");
  a_threshold_timer=new_po_timer("a_sum");

  out_arg[0]=&t;
  do_operation(down_obj,(po_opcode)down_int,out_arg);
  wait_for_end_of_invocation(down_obj);

  /*
  fprintf(stderr,"(%d,%d)",me,t);
  fflush(stderr);
  */
  
  do_computation();
  barrier(wch);
  wait_on_channel(wch);

  out_arg[0]=&t;
  do_operation(down_obj,(po_opcode)down_int,out_arg);
  wait_for_end_of_invocation(down_obj);
  count=0;
  do_computation();
  barrier(wch);
  wait_on_channel(wch);

  start=sys_milli();

  /* Get ticket for next matrix. */
  out_arg[0]=&t;
  start_po_timer(computation_timer);
  do_operation(down_obj,(po_opcode)down_int,out_arg);
  wait_for_end_of_invocation(down_obj);
    
  while (t>=0) {
    do_computation();
    end_po_timer(computation_timer);
    add_po_timer(computation_timer);
    timings[count]=read_last(computation_timer,PO_MILLISEC);
    count++;
    /* Get ticket for next matrix. */
    out_arg[0]=&t;
    do_operation(down_obj,(po_opcode)down_int,out_arg);
    wait_for_end_of_invocation(down_obj);
  }
  
  
}
