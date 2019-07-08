/* This version of SOR uses red/black partitioning. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grid.h"
#include "misc.h"
#include "po_invocation.h"
#include "po_rts_object.h"
#include "assert.h"

#define element(matrix,i,j,num_cols) ((matrix)[(i)*(num_cols)+(j)])
#define right_color(i,j,color) ((color)==(((i)+(j))&1))
#define myfabs(v) (((v)>=0.0) ? (v) : (-(v)))
 
double omega=1.945254;

static int group_size;
static int me;
static int proc_debug;
static po_p grid_class;

/* =============== */
void
MaxDouble(double *v1, double *v2) {
  if ((*v1)<(*v2)) (*v1)=(*v2);
}

/* ======================================================= */
/* Builds the pdg associated with the function UpdateGrid. */
/* ======================================================= */

pdg_p 
pdg_UpdateGrid(instance_p instance) {
  int dest[2];
  int row, col;
  int psource, p, pdest;
  pdg_p pdg;

  pdg=new_pdg(instance->state->num_parts);
  for (p=0; p<instance->state->num_parts; p++)
    for (row=instance->state->partition[p]->start[0]; 
	 row<=instance->state->partition[p]->end[0]; row++)
      for (col=instance->state->partition[p]->start[1]; 
	   col<=instance->state->partition[p]->end[1]; col++)
	{
	  psource=instance->state->partition[p]->part_num;

	  dest[0]=row+1;
	  dest[1]=col;
	  pdest=partition(instance,dest);
	  if (pdest>=0) 
	    add_edge(pdg,psource,instance->state->owner[psource],
		     pdest,instance->state->owner[pdest]);

	  dest[0]=row-1;
	  dest[1]=col;
	  pdest=partition(instance,dest);
	  if (pdest>=0) 
	    add_edge(pdg,psource,instance->state->owner[psource],
		     pdest,instance->state->owner[pdest]);
	  
	  dest[0]=row;
	  dest[1]=col-1;
	  pdest=partition(instance,dest);
	  if (pdest>=0) 
	    add_edge(pdg,psource,instance->state->owner[psource],
		     pdest,instance->state->owner[pdest]);
	  
	  dest[0]=row;
	  dest[1]=col+1;
	  pdest=partition(instance,dest);
	  if (pdest>=0) 
	    add_edge(pdg,psource,instance->state->owner[psource],
		     pdest,instance->state->owner[pdest]);
	}
  return pdg;
}

/* =============== */
void InitGrid(int sender, instance_p instance, void **args)
{
  int row;
  int col;
  double *matrix;

  matrix=instance->state->data;
  for (row=0; row<instance->state->length[0]; row++) {
    for (col=0; col<instance->state->length[1]; col++) {
      if (row==0) (*matrix)=4.56;
      else if (row==(instance->state->length[0]-1)) (*matrix)=9.85;
      else if (col==0) (*matrix)=7.32;
      else if (col==(instance->state->length[1]-1)) (*matrix)=6.88;
      else (*matrix)=0.0;
      matrix++;
    }
  }
}

/* ======================================================= */
/* ======================================================= */

void PrintGrid(int sender, instance_p instance, void **args)
{
  register int i;
  register int j;
  double *matrix;

  matrix=(double *)(instance->state->data);
  for (i=0; i<instance->state->length[0]; i++)
    {
      for (j=0; j<instance->state->length[1]; j++)
	printf("%5.3f ", element(matrix,i,j,instance->state->length[1]));
      printf("\n");
    }
  printf("\n");
}


/* =============== */
void 
UpdateGrid(int part_num, instance_p instance, void **args) {
  int row;
  int col;
  int length;
  double *matrix;
  double *value;
  double diff;
  partition_p part;
  int ps0, pe0, ps1, pe1;
  int se0, se1;
  double oldel, newel;

  int color = (*(int *)(args[0]));
   double *maxdiff = args[1]; 

  part = instance->state->partition[part_num];
  matrix=(instance->state->data);
  value = (double *)(part->elements);
  length = instance->state->length[1];

  /* The RTS fumbles in an extra parameter, in this case args[2].
     It is set to 1 the first time the operation gets called, so it can be
     used to initialize the reduced output parameter(s).
  */
   if (*(int *) (args[2])) { 
 	*(int *) (args[2]) = 0; 
 	*maxdiff = 0.0; 
   }  
  ps0 = part->start[0];
  pe0 = part->end[0];
  ps1 = part->start[1];
  pe1 = part->end[1];
  se0 = instance->state->end[0];
  se1 = instance->state->end[1];
  
  for (row=ps0; row<=pe0; row++) {
    if (row > 0 && row < se0) {
      for (col=ps1; col<=pe1; col++, value++) {
	if (right_color(row,col,color) && col > 0 && col < se1) {
	  oldel = element(matrix, row, col, length); 
	  newel = (element(matrix, row-1, col, length) + 
		   element(matrix, row+1, col, length) +
		   element(matrix, row, col-1, length) + 
		   element(matrix, row, col+1, length))/4.0; 
	  diff=myfabs(newel - oldel);
	  MaxDouble(args[1], &diff); 
	  (*value)=(oldel + omega*(newel-oldel));
	}
      }
    }
    else
      value += part->length[1];
  }
}

/* ======================================================= */
/* ======================================================= */

static po_pardscr_t UpdateDescr[] = {
	{ sizeof(boolean), IN_PARAM, 0, 0, 0},
	{ sizeof(double),  OUT_PARAM , REDUCE , (reduction_function_p)MaxDouble, 0} 
};

int 
new_grid_class(int moi, int gsize, int pdebug) {
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;

  grid_class=new_po("Grid", 2, sizeof(double), (void *) 0);
  if (grid_class==NULL) return -1;

  register_reduction_function((reduction_function_p)MaxDouble);
  
  new_po_operation(grid_class,"UpdateGrid",(po_opcode)UpdateGrid,
		   ParWrite,2,UpdateDescr, (build_pdg_p)pdg_UpdateGrid); 
  new_po_operation(grid_class,"PrintGrid",(po_opcode)PrintGrid,
		   SeqRead,0,0,NULL);
  new_po_operation(grid_class,"InitGrid",(po_opcode)InitGrid,
		   SeqInit,0,0,NULL);
  synch_handlers();
  return 0;
}
  
/* ======================================================= */
/* ======================================================= */

instance_p 
grid_instance(int num_rows, int num_cols, handler_p handler) {
  int length[2];
  instance_p grid;

  length[0]=num_rows;
  length[1]=num_cols;

  grid=do_new_instance(grid_class,length, (int *) 0);
  assert(grid!=NULL);

  if (handler==e_parallel_blocking)
    {
      do_change_attribute(grid, (po_opcode)UpdateGrid, EXECUTION, handler);
      do_change_attribute(grid, (po_opcode)UpdateGrid, TRANSFER, noop);
    } 
  else
    if (handler==e_parallel_nonblocking)
      {
	do_change_attribute(grid, (po_opcode)UpdateGrid, EXECUTION, handler);
	do_change_attribute(grid, (po_opcode)UpdateGrid, TRANSFER, t_sender);
      } 
    else if (handler==e_parallel_consistent)
           {
	     do_change_attribute(grid, (po_opcode)UpdateGrid, EXECUTION, 
				 handler);
	     do_change_attribute(grid, (po_opcode)UpdateGrid, TRANSFER, 
				 t_collective_all);
           } 
         else {
                assert(handler==e_parallel_control);
                do_change_attribute(grid, (po_opcode)UpdateGrid, 
				    EXECUTION, handler);
                do_change_attribute(grid, (po_opcode)UpdateGrid, TRANSFER, 
				    noop);
              } 
  return grid;
}
