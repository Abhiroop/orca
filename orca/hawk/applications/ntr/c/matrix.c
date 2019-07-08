/*==================================================*/
/*  matrix3.c 13 Jul 1995                           */
/* solution with only one operation                 */
/* graph with all possible conexions                */
/*==================================================*/
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include "communication.h"
#include "po.h"
#include "util.h"
#include "synchronization.h"
#include "collection.h"
#include "matrix.h"

extern malloc();
extern exit();
extern memcpy();

int me, group_size, proc_debug;


po_p fft_class;             /* pointer to the fft class */

/* array with the powers of w */

static int power[MAXN];
extern double wr[MAXN], wi[MAXN];

extern double sin(double);
extern double cos(double);
extern memset();
extern free();
extern sleep();

pdg_p pdg[MAXK];
static int kl;

/* =============== */
void SumDouble(double *v1, double *v2)
{
  (*v1) = (*v1) + (*v2);
}

/* ======================================================= */
/* auxilary functions                                      */
/* ======================================================= */

void gen_ind_simple(unsigned long int n, int k)
{
    int j, l;
    unsigned long int  pw, number;
    power[0] = 0;
    pw = n/4;      /* value of bigest power */
    number = 1;    /* how many were already filled */
  
    for( j = 1; j < k; j++)
      {
	  for( l = number; l <= number * 2 -1; l++)
	    power[l] = power[l - number] + pw;
	  pw = pw / 2; number = number * 2;
      }
}

/* ======================================================= */
/* generate values for powers for w */
void gen_ind_w(unsigned long int n, int k)
{
    int j;
    gen_ind_simple(n, k);
    for ( j = n/2; j < n; j++)
      power[j] = power[j - n  / 2];
}

/* ======================================================= */
/* compute values for powers of w */

void powers(unsigned long int n)
{ 
    double alfa;
    int j;
    alfa = - TWOPI/n;
    for (j = 0; j < n/2; j++)
      { wr[j] = cos(alfa * power[j]);
	wi[j] = sin(alfa * power[j]);

    }
}



/* ======================================================= */
int k_log_2(unsigned long int n)
{
    int k;
    unsigned long int p;
    p = n; k = 0;
    while(p > 1)
      {
	p = p >> 1;
	k++;
    }
    return k;
}
/* =============== */



/* ======================================================= */
/* functions for operations                                */
/* ======================================================= */


void Initial_A(int part, instance_p instance, void **arg)
{
  int i, k, number;
  PELEM matrix;
 
  number = instance->state->length[0];
  k = k_log_2(number);
  matrix=(PELEM )(instance->state->data);    
  for(i = 0; i < instance->state->length[0]; i++) {
    matrix -> ar = wr[i]; 
    matrix -> ai = wi[i];
    matrix++;
  }
/* construct the arrays with w powers */
  gen_ind_w(number, k);
  powers(number);
}

/* ========================================================== */
/* Builds the pdg associated with the function A_computation. */
/* ========================================================== */

pdg_p pdg_A_computation(instance_p instance, po_opcode opcode)
{
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
		  add_edge(pdg[k],p_source,instance->state->owner[p_source],
			   p_other, instance->state->owner[p_other]);
	      }
	  }
	
      }
      return pdg[kl - 1];
  }

/* ===================================== */
/* Insert the new PDG for A_computation. */
/* ===================================== */
void insert_pdg_A_computation(int sender, instance_p instance, void **args)
{
  int * pkl1=args[0];

  assert((*pkl1)<MAXK);

  if (instance->processors->num_members == 1) return;
  insert_pdg(instance,
	     (po_opcode)A_computation,pdg[(*pkl1)]);
}


/* ====================== */
/* computing new elements */
/*======================= */
void A_computation(int part, instance_p instance, void **args)
{
  int u, v, y, x;
  int other;            /* the other necesary element */
  int ind, k_l_1, dist;
  partition_p ppartition;
  PELEM matrix, value;
  int * pkl1=args[0];
  
  k_l_1 = *pkl1;
  /* distance between elements */
  dist = 1 << k_l_1; /* dist = 2 ** kl1 */
  matrix=(PELEM)(instance->state->data); /* the old values */
  /* get a pointer to the curent partition */
  ppartition=instance->state->partition[part];    
  value = ppartition->elements;
  for(ind = ppartition->start[0]; ind <= ppartition->end[0]; ind++)
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
      
      value -> ar = matrix[v].ar +  x * (wr[y] *  matrix[u].ar -
					 wi[y] *  matrix[u].ai);
      
      value -> ai = matrix[v].ai +  x * (wr[y] *  matrix[u].ai +
					 wi[y] *  matrix[u].ar);
      
      value++;
    }
  
}

/*===================================================================== */
void A_convert(int part, instance_p instance, void **args) {
  int *sum=args[1];
  int *d=args[0];
  partition_p ppartition;
  PELEM matrix, value;
  int ind;

  assert((*d)!=0);

  *sum=0;
  matrix=(PELEM)(instance->state->data); /* the old values */
  /* get a pointer to the curent partition */
  ppartition=instance->state->partition[part];    
  value = ppartition->elements;
  for(ind = ppartition->start[0]; ind <= ppartition->end[0]; ind++) {
    value->ar= sqrt(matrix->ar*matrix->ar + matrix->ai*matrix->ai) / (*d);
    (*sum) = (*sum) + value->ar;
    value++;
  }
}

/*===================================================================== */
void A_sum(int part, instance_p instance, void **args) {
  int *sum=args[0];
  partition_p ppartition;
  PELEM value;
  int ind;
  
  *sum=0;
  /* get a pointer to the curent partition */
  ppartition=instance->state->partition[part];    
  value = ppartition->elements;
  ppartition=instance->state->partition[part];    
  for(ind = ppartition->start[0]; ind <= ppartition->end[0]; ind++) {
    (*sum) = (*sum) + value->ar;
    value++;
  }
}

/*===================================================================== */
void A_threshold(int part, instance_p instance, void **args) {
  int *sum=args[0];
  partition_p ppartition;
  PELEM value;
  int ind;
  
  ppartition=instance->state->partition[part];    
  value = ppartition->elements;
  for(ind = ppartition->start[0]; ind <= ppartition->end[0]; ind++) {
    if (value->ar < (*sum)) value->ar = 0;
    value++;
  }
}


/*======================= */
/*======================= */
void Get_Print_A(int sender, instance_p instance, void **args)
{
    PARGUM p = args[0];
    int ind;
    PELEM matrix;
   
    long unsigned n;
    int k;
    n = p -> number;
    k = k_log_2(n);
    gen_ind_simple(n * 2, k + 1);


   /* get all elements */

    fetch_all(instance);

    /* get the acces to the data */
    matrix = (PELEM)(instance->state->data);   /* the old values */

    access_all(instance);

    if((me == sender) && p -> verbose)
      {
	  for (ind = 0; ind <instance->state->length[0]; ind++)
	    printf("\n ar[%i] = %f, ai[%i] = %f", 
		   ind, matrix[power[ind]].ar,
		   ind, matrix[power[ind]].ai);

      }
}

/* ======================================================= */
/* initialize the matrix class                             */
/* ======================================================= */

static po_pardscr_t A_computation_Descr[] = {
	{ sizeof(int), IN_PARAM, 0, 0, 0}
};

static po_pardscr_t A_convert_Descr[] = {
	{ sizeof(int), IN_PARAM, 0, 0, 0},
	{ sizeof(double), OUT_PARAM, REDUCE, (reduction_function)SumDouble, 0}
};

static po_pardscr_t A_sum_Descr[] = {
	{ sizeof(double), OUT_PARAM, REDUCE, (reduction_function)SumDouble, 0}
};

static po_pardscr_t A_threshold_Descr[] = {
	{ sizeof(double), IN_PARAM, 0, 0, 0}
};

int new_fft_class(int moi, int gsize, int pdebug)
{
    precondition(moi>=0);
    precondition(moi<gsize);
  
    me=moi;
    group_size=gsize;
    proc_debug=pdebug;

    /* get the pointer to the new class */
    
    fft_class=new_po("Matrix", 1, sizeof(ELEM), (void *)0);
    if (fft_class==NULL) return -1;
    
    register_reduction_function((reduction_function)SumDouble);
  
    new_po_operation(fft_class,"Initial_A",(po_opcode)Initial_A,
		     SeqInit, 0, 0, NULL);
    new_po_operation(fft_class,"A_computation",(po_opcode)A_computation,
		     ParWrite, 1, A_computation_Descr, 
		     (build_pdg_p)pdg_A_computation);
    new_po_operation(fft_class,"insert_pdg_A_computation",
		     (po_opcode)insert_pdg_A_computation,
		     RepRead, 1, A_computation_Descr, NULL);
    new_po_operation(fft_class,"A_convert",(po_opcode)A_convert,
		     ParWrite, 2, A_convert_Descr, NULL);
    new_po_operation(fft_class,"A_sum",(po_opcode)A_sum,
		     ParRead, 1, A_sum_Descr, NULL);
    new_po_operation(fft_class,"A_threshold",(po_opcode)A_threshold,
		     ParWrite, 1, A_threshold_Descr, NULL);
    new_po_operation(fft_class,"Get_Print_A",(po_opcode)Get_Print_A,
		     SeqRead, 0, 0, NULL);

    synch_handlers();
    return 0;
}


/* ======================================================= */
/* initialize the instance for fft_class                   */
/* ======================================================= */

instance_p fft_instance(int number)
{

    instance_p matrix;
    
    /* create an instance for the fft_class */
    matrix=new_instance(fft_class,&number,(void *)0);
    
    change_attribute(matrix, (po_opcode)A_computation, 
		     EXECUTION, e_parallel_control);
    change_attribute(matrix, (po_opcode)A_computation, 
		     TRANSFER, t_no_transfer);

    change_attribute(matrix, (po_opcode)A_sum, 
		     EXECUTION, e_parallel_consistent);
    change_attribute(matrix, (po_opcode)A_sum, 
		     TRANSFER, t_no_transfer);

    change_attribute(matrix, (po_opcode)A_convert, 
		     EXECUTION, e_parallel_consistent);
    change_attribute(matrix, (po_opcode)A_convert, 
		     TRANSFER, t_no_transfer);

    change_attribute(matrix, (po_opcode)A_threshold, 
		     EXECUTION, e_parallel_consistent);
    change_attribute(matrix, (po_opcode)A_threshold, 
		     TRANSFER, t_no_transfer);

    synch_handlers();
    return matrix;
}












