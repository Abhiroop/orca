/*==================================================*/
/* vector.c 13 Jul 1995                             */
/* solution with only one operation                 */
/* graph with all possible conexions                */
/*==================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "misc.h"
#include "po_invocation.h"
#include "vector.h"
#include "instance.h"
#include "po_rts_object.h"
#include "precondition.h"

/* =============== */
void MaxDouble(double *v1, double *v2)
{
  if ((*v1)<(*v2))
    (*v1)=(*v2);
}

int me, group_size, proc_debug;


po_p fft_class;             /* pointer to the fft class */

/* array with the powers of w */

double wr[MAXN], wi[MAXN];
extern double sin(double);
extern double cos(double);
extern char *name_file;

pdg_p pdg[MAXK];
static int kl;
static int power[MAXN];
static po_timer_p A_computation_timer;

/*================================================================= */
/*================================================================= */
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

/* ============================================================ */
void 
SumDouble(double *v1, double *v2) {
  (*v1) = (*v1) + (*v2);
}

/* ============================================================ */
/* auxilary functions                                           */
/* ============================================================ */
void 
gen_ind_simple(unsigned long int n, int k) {
    int j, l;
    unsigned long int  pw, number;
    power[0] = 0;
    pw = n/4;      /* value of bigest power */
    number = 1;    /* how many were already filled */
  
    for( j = 1; j < k; j++) {
	  for( l = number; l <= number * 2 -1; l++)
	    power[l] = power[l - number] + pw;
	  pw = pw / 2; number = number * 2;
      }
}

/* ============================================================= */
/* generate values for powers for w                              */
/* ============================================================= */
void 
gen_ind_w(unsigned long int n, int k) {
    int j;
    gen_ind_simple(n, k);
    for ( j = n/2; j < n; j++)
      power[j] = power[j - n  / 2];
}

/* ============================================================= */
/* compute values for powers of w                                */
/* ============================================================= */
void 
powers(unsigned long int n) { 
    double alfa;
    int j;
    alfa = - TWOPI/n;
    for (j = 0; j < n/2; j++) { 
      wr[j] = cos(alfa * power[j]);
      wi[j] = sin(alfa * power[j]);
    }
}


/* ================================================================== */
/* ================================================================== */
int 
k_log_2(unsigned long int n) {
    int k;
    unsigned long int p;
    p = n; k = 0;
    while (p > 1) {
	p = p >> 1;
	k++;
    }
    return k;
}


/* =================================================================== */
/* functions for operations                                            */
/* =================================================================== */


/* =================================================================== */
/* =================================================================== */
void 
Initial_A(int part, instance_p instance, void **args) {
  int i, k, number;
  PELEM vector;
 
  number = instance->state->length[0];
  read_input_data(name_file,number);
  k = k_log_2(number);
  vector=(PELEM )(instance->state->data);    
  for(i = 0; i < instance->state->length[0]; i++) {
    vector -> ar = wr[i]; 
    vector -> ai = wi[i];
    vector++;
  }
  /* construct the arrays with w powers */
  gen_ind_w(number, k);
  powers(number);
}

/* ================================================================== */
/* Builds the pdg associated with the function A_computation.         */
/* ================================================================== */
pdg_p
pdg_A_computation(instance_p instance, po_opcode opcode) {
    int k, dist, n;
    int ind, other;
    int p_source, p, p_other;

    precondition(instance!=NULL);
    precondition(opcode!=NULL);
  
    if (!is_member(instance->processors,me)) return 0;

    n = instance->state->num_parts * 
      (instance->state->partition[0]->end[0] - 
       instance->state->partition[0]->start[0] + 1);
    kl = k_log_2(n);

    /* a different graph is constructed for each iteration */

    for (k = kl - 1; (k >= 0) ; k--)
      {
	pdg[k]=new_pdg(instance->state->num_parts);
	
	dist = 1 << k;
	
	/* for each  owned partition */
	for (p=0; p<instance->state->num_parts; p++)
	  {
	    p_source = instance->state->partition[p]->part_num;
	    /* all elements from a partition have 
	       the "other" elements in the same partition
	       (all partitions have the same length)
	       */
	    ind = instance->state->partition[p]->start[0];
	    other = ind ^ dist; 
	    p_other  = partition(instance, &other);
	    
	    if ((p_other>=0) && (p_other!=p_source))
	      {
		if ((instance->state->owner[p_source]!=instance->state->owner[p_other]) &&
		    ((instance->state->owner[p_source]==me) ||
		     (instance->state->owner[p_other]==me)))
		  add_edge(pdg[k],p_source,instance->state->owner[p_source],
			   p_other, instance->state->owner[p_other]);
	      }
	  }
      }
      return pdg[kl - 1];
  }

/* =============================================================== */
/* Insert the new PDG for A_computation.                           */
/* =============================================================== */
void 
insert_pdg_A_computation(int sender, instance_p instance, void **args) {
  int pkl1;

  pkl1=*((int *)args[0]);
  assert((pkl1)<MAXK);
  if (instance->processors->num_members == 1) return;
  assert(pdg[pkl1]!=NULL);
  insert_pdg(instance,(po_opcode)A_computation,pdg[(pkl1)]);
}


/* ============================================================= */
/* computing new elements                                        */
/*============================================================== */
void 
A_computation(int part, instance_p instance, void **args) {
  int u, v, y, x;
  int other;            /* the other necesary element */
  int e0;
  int ind, k_l_1, dist;
  partition_p ppartition;
  PELEM vector, value;

  start_po_timer(A_computation_timer);
  k_l_1 = *((int *)args[0]);
  /* distance between elements */
  dist = 1 << k_l_1; /* dist = 2 ** kl1 */
  vector=(PELEM)(instance->state->data); /* the old values */
  /* get a pointer to the curent partition */
  ppartition=instance->state->partition[part];    
  value = ppartition->elements;
  e0 = ppartition->end[0];
  for(ind = ppartition->start[0]; ind <= e0; ind++)
    {
      other = ind ^ dist;
      /* if (ind AND (2 ** (k-l-1)) */
      if(ind & dist) 
	x = -1;
      else
	x = 1;
      
      u = max_index(ind, other);
      v = min_index(ind, other);
      
      /* y = ind / ( 2 ** (k - l)) */
      y = ind >> (k_l_1 + 1);
      
      if (x == -1) {
          value -> ar = vector[v].ar -  (wr[y] *  vector[u].ar -
					 wi[y] *  vector[u].ai);
          value -> ai = vector[v].ai -  (wr[y] *  vector[u].ai +
					 wi[y] *  vector[u].ar);
      }
      else {
          value -> ar = vector[v].ar +  (wr[y] *  vector[u].ar -
					 wi[y] *  vector[u].ai);
          value -> ai = vector[v].ai +  (wr[y] *  vector[u].ai +
					 wi[y] *  vector[u].ar);
      }
      
      value++;
    }
  end_po_timer(A_computation_timer);
  add_po_timer(A_computation_timer);
}

/* A_computation operation descriptor. */
static po_pardscr_t A_computation_Descr[] = {
	{ sizeof(int), IN_PARAM, 0, 0, 0},
	{ sizeof(double), OUT_PARAM, REDUCE, (reduction_function)MaxDouble, 0}
};

/* ================================================================== */
/* initialize the vector class                                        */
/* ================================================================== */
int 
new_fft_class(int moi, int gsize, int pdebug) {
    precondition(moi>=0);
    precondition(moi<gsize);
  
    me=moi;
    group_size=gsize;
    proc_debug=pdebug;

    /* get the pointer to the new class */
    
    register_reduction_function((reduction_function)MaxDouble);
    A_computation_timer=new_po_timer("A_computation");
    fft_class=new_po("Vector", 1, sizeof(ELEM), NULL);
    if (fft_class==NULL) return -1;
    
    new_po_operation(fft_class,"Initial_A",(po_opcode)Initial_A,
		     SeqInit, 0, 0, NULL);
    new_po_operation(fft_class,"A_computation",(po_opcode)A_computation,
		     ParWrite, 1, A_computation_Descr, 
		     (build_pdg_p)pdg_A_computation);
    new_po_operation(fft_class,"insert_pdg_A_computation",
		     (po_opcode)insert_pdg_A_computation,
		     RepRead, 1, A_computation_Descr, NULL);

    synch_handlers();
    return 0;
}


/* ================================================================== */
/* initialize the instance for fft_class                              */
/* ================================================================== */
instance_p 
fft_instance(int number, handler_p handler) {

    instance_p vector;
    /* create an instance for the fft_class */
    vector=do_new_instance(fft_class,&number,(int *)NULL);
    
    do_change_attribute(vector, (po_opcode)insert_pdg_A_computation, 
			TRANSFER, noop);
    if ((handler==e_parallel_blocking) || (handler==e_parallel_control))
      {
	do_change_attribute(vector, (po_opcode)A_computation, EXECUTION, handler);
	do_change_attribute(vector, (po_opcode)A_computation, TRANSFER, noop);
      } 
    else {
      assert(handler==e_parallel_nonblocking);
      do_change_attribute(vector, (po_opcode)A_computation, EXECUTION, handler);
      do_change_attribute(vector, (po_opcode)A_computation, TRANSFER, t_sender);
    } 

    return vector;
}












