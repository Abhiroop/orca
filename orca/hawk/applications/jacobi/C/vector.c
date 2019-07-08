#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "vector.h"
#include "misc.h"
#include "po_timer.h"
#include "po_invocation.h"
#include "po_rts_object.h"

#define MAX_VALUE  4.0

static int group_size;
static int me;
static int proc_debug;

static po_p vector_class;

static double *a, *b;

/* =============== */
void 
MaxDouble(double *v1, double *v2)
{
  if ((*v1)<(*v2))
    (*v1)=(*v2);
}


/* ===================================================== */
void 
check_result(double *a, double *x, double *b, int vector_size)
{

  int i,j;
  int correct=1;
  double res, err;

  for (i=0; i<vector_size; i++)
    { res=0;
      err=0;
      for (j=0; j< vector_size; j++)
	{
	  res=res+a[i*vector_size+j]*x[j];
	  err=err+a[i*vector_size+j]*PRECISION;
        }
      if (fabs(res - b[i]) > err)
	{
	  correct=0;
	  fprintf(stderr,"%g \t %g\n", res, fabs(res - b[i]));
	}
    }
  if (!correct)
    fprintf(stderr,"!!!!!!!!!! result is incorrect !!!!!!!!!!!!\n");
  fprintf(stderr,"\n");
}


/* ===================================================== */
void 
do_check_result(int sender, instance_p vector, void **args)
{
  double *x;

  x=(double *)(vector->state->data);

  if (me==sender)
    check_result(a,x,b,vector->state->length[0]);

}


/* ============================================================= */
void 
print_system(int sender, instance_p vector, void **args)
{
  int i;
  double *x;

  x=(double *)(vector->state->data);

  if (me==sender)
    {
      printf("%d: ", me);
      for (i=0; i< vector->state->length[0]; i++) 
	printf("%g ", x[i]);
      printf("\n");
    }
}


/* ============================================================= */
void 
init_system(int sender, instance_p vector, void **args)
{
  int i,j;
  double sumrow;
  double *x;

  x=(double *)(vector->state->data);

  a=(double *)malloc(sizeof(double)*vector->state->length[0]*vector->state->length[0]);
  b=(double *)malloc(sizeof(double)*vector->state->length[0]);

  for (i=0; i<vector->state->length[0]; i++)
    { sumrow=0.0;
      for (j=0; j< vector->state->length[0]; j++) 
	  if (i!=j) 
	    { a[i*vector->state->length[0]+j]=(double )(i+j)/MAX_VALUE;
	      sumrow = sumrow + a[i*vector->state->length[0]+j];
	    }
      x[i] = (double )i/MAX_VALUE;
      a[i*vector->state->length[0]+i]=sumrow+(double )i;
      b[i] = (double)(i+2)/MAX_VALUE;
    }
}

/* ============================================================= */
void 
update_x(int part_num, instance_p vector, void **args)
{
  double *maxdiff = args[0];
  double *value;
  double diff;
  partition_p partition = vector->state->partition[part_num];
  int i, j;
  double *x;

  x=(double *)(vector->state->data);

  if (* (int *)(args[1]) == 1) {
	* (int *)(args[1]) = 0;
	*maxdiff = 0;
  }

  partition=vector->state->partition[part_num];
  value=partition->elements;
      
  for (i=partition->start[0]; i<=partition->end[0]; i++)
  {
	  int m = i*vector->state->length[0];
	  double v;
	  v=b[i];
	  for (j=0; j<i; j++)
	    {
/* 	      check_element(vector,&j); */
	      v=v-x[j]*a[m+j];
	    }
	  for (j=i+1; j<vector->state->length[0]; j++)
	    {
/* 	      check_element(vector,&j); */
	      v=v-x[j]*a[m+j];
	    }
	  v=v/a[m+i];
	  diff=fabs(v-x[i]);
	  MaxDouble(maxdiff,&diff);
	  *value=v;
	  value++;
  }
}


/* ======================================================= */
/* ======================================================= */

static po_pardscr_t update_x_descr[] = {
	{ sizeof(double), OUT_PARAM, REDUCE, (reduction_function)MaxDouble, 0}
};

int 
init_vector_class(int moi, int gsize, int pdebug)
{
  assert(moi>=0);
  assert(moi<gsize);
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;

  vector_class=new_po("Vector",1, sizeof(double), (void *) 0);
  assert(vector_class!=NULL);
  
  register_reduction_function((reduction_function)MaxDouble);
  
  new_po_operation(vector_class,"update_x",
		   (po_opcode)update_x,ParWrite,1,update_x_descr,NULL);
  new_po_operation(vector_class,"print_system",
		   (po_opcode)print_system,SeqRead,0,NULL,NULL);
  new_po_operation(vector_class,"do_check_result",
		   (po_opcode)do_check_result,SeqRead,0,NULL,NULL);
  new_po_operation(vector_class,"init_system",
		   (po_opcode)init_system,SeqWrite,0,NULL,NULL);
  synch_handlers();
  return 0;
}

int
finish_vector_class(void)
{
    free_po_operation(vector_class, (po_opcode)update_x);
    free_po_operation(vector_class, (po_opcode)print_system);
    free_po_operation(vector_class, (po_opcode)do_check_result);
    free_po_operation(vector_class, (po_opcode)init_system);

    release_reduction_function_p((reduction_function)MaxDouble);

    free_po(vector_class);
    return 0;
}

/* ======================================================= */
/* ======================================================= */

instance_p 
vector_instance(int vector_size)
{
  instance_p vector;

  assert(vector_size>0);

  vector=do_new_instance(vector_class,&vector_size,(int *) 0);
  assert(vector!=NULL);

  do_change_attribute(vector, (po_opcode) update_x, TRANSFER, t_collective_all);
  do_change_attribute(vector, (po_opcode) update_x, EXECUTION, e_parallel_consistent);
  return vector;
}
