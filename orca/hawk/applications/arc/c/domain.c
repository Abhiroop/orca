
/*==================================================*/
/*  domain1.c 24 Jul 1995                            */
/* graph solution                                   */
/*==================================================*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "po_invocation.h"
#include "po_rts_object.h"
#include "domain.h"
#include "arc.h"

int me, group_size, proc_debug;
int a;                /* number of values for each avriable */
char * reltemp;
po_p domain_class;             /* pointer to the domain class */

extern char *name_file;

/* ======================================================= */
/* auxiliary functions                                     */
/* ======================================================= */

/* =============== */

void MaxOutArgum(POUT_ARGUM v1,  POUT_ARGUM v2)
{

    v1 -> no_solution |= v2 -> no_solution;
    v1 -> nr_modif += v2 -> nr_modif;
    v1 -> con_checks += v2 -> con_checks;
    v1 -> modif |= v2 -> modif;

}


/* ========================================================== */
/* Builds the pdg associated with the function Spac_Loop.     */
/* ========================================================== */

void pdg_Spac_Loop(instance_p instance, po_opcode opcode)
{
    int i, j, n;
    int p_source, p, p_other; 
 
    pdg_p pdg;
    int *remote_deps;
    boolean *deps;
    precondition(instance!=NULL);
    precondition(opcode!=NULL);

    Init_constr(&n, &a, name_file);
    deps=(boolean *)malloc(sizeof(boolean)*instance->state->num_parts);
    memset(deps,0,sizeof(boolean)*(instance->state->num_parts));
    remote_deps
      =(int *)malloc(sizeof(int)*(instance->state->num_parts+1));
    memset(remote_deps,0,sizeof(int)*(instance->state->num_parts+1));

    pdg=new_pdg(instance->state->num_parts);

    /* for each owned partition */

    for (p=0; p<instance->state->num_owned_parts; p++)
      {
	  /* for each variable */
	  
	  for (i = instance->state->owned_partitions[p]->start[0]; 
	       i <= instance->state->owned_partitions[p]->end[0]; i++)
	    {
		p_source=partition(instance, &i);

		/* for each other variable */
		for(j = 0; j < n;j++)
		  {  
		      if(i != j)
			{
			    if(test_constr(i,j, n))
			      {
				  p_other  = partition(instance, &j);
				  if ((p_other>=0) && (p_other!=p_source) && 
				      (instance->state->owner[p_other]!=me))
				    {
					add_edge(pdg,p_source,p_other);
					deps[p_other]=TRUE;
				    }
			      }
			}
		  }
	    }
      }

    
    i=1;
    j=instance->state->num_owned_parts;
    for (p=0; p<instance->state->num_owned_parts; p++)
      {
	p_source=instance->state->owned_partitions[p]->part_num;
	if (deps[p_source])
	  {
	    remote_deps[j]=p_source;
	    j--;
	  }
	else
	  {
	    remote_deps[i]=p_source;
	    i++;
	  }
      }
    
    remote_deps[0]=i-1;
    free(deps);

    insert_pdg(instance,(po_opcode)Spac_Loop,pdg,remote_deps);
    insert_pdg(instance,(po_opcode)Actual_D,pdg,remote_deps);

}

/*************************************************************/
/* sprevise  - make consistency checks & remove values       */
/*************************************************************/
BOOL 
sprevise(int i, int j, int n, PELEM pold, PELEM pnew, 
	 int * pnr_modif, 
	 int * pcon_checks, int * pno_solution) {
  BOOL support, solutie, modif;   /* check if one value 
				     supports another */
  int k,l;                        /* counters */
  
  solutie = FALSE;
  modif = FALSE;
  /* Check each value of variable i with each value of variable j   */
  for ( k=0 ; k < a ; k++ ) 
    {
      if(pnew -> val[k])    /* the value still exists */
	{
	  support = FALSE;
	  for ( l=0; (l<a) && (!support) ; l++ )
	    {
	      if(pold -> val[l])
		{
		  (*pcon_checks)++;
		  support = test(i, k, j, l, n, a); 
		}
	    }
	  if (!support) 
	    {
	      pnew -> val[k] = FALSE;
	      (*pnr_modif)++;
	      modif = TRUE;
	    }
	  else
	    solutie = TRUE;
	}
    }
  
  if(!solutie)
    *pno_solution = TRUE;
  return  modif;
}


/* ======================================================= */
/* functions for operations                                 */
/* ======================================================= */

/* the domain is initialized:
   - all variables has all values;
   - all variables must be checked again;
   - there was no modification yet
*/


void Init_D(int sender, instance_p instance, void **args) {
  PELEM matrix;
  int i, j, n;
  char *pname;

  /* read the relations and the constraints */
  pname=args[0];
  Init_constr(&n, &a, pname);
  matrix=instance->state->data;
  for(i=instance->state->start[0]; i <= instance->state->end[0]; i++) {
    for(j = 0; j < a; j++)
      matrix ->val[j] = TRUE;   /* all values exists */ 
    matrix -> work = TRUE;      /* all values must be tested */
    matrix -> modif = FALSE;    /* there were no modif yet */
    matrix++;
  }
}

/* 
   the loop for checking the variables. For the moment there is only
   one pass trough the variable for each iteration, because there is 
   no other way to exchange info between parts of the object (after each
   pass through a variable if anything is changed the other variables which 
   have predicates in commun with that variable must be checked again).

*/

void 
Spac_Loop(int part_num, instance_p instance, void **args) {
  int n;                 /* number of variables */
  int a;                 /* namber of values */
  PIN_ARGUM pin;
  POUT_ARGUM pout;
  int i, j;
  OUT_ARGUM ploc;
  BOOL i_change, el1;
  PELEM matrix, value;
  partition_p ppartition;   
  
  pin=args[0];
  pout=args[1];
  n = pin -> n;
  a = pin -> a;
  if (pin -> verbose) printf("\nSpac_loop started for %i\n", me);
  matrix=(PELEM)(instance->state->data);   /* the old values */
  pout -> no_solution = FALSE;
  pout -> nr_modif = 0;
  pout -> con_checks = 0;
  pout -> modif = FALSE;
  
  ploc.con_checks = 0;
  ploc.nr_modif = 0;
  ploc.modif = FALSE;
  ploc.no_solution=FALSE;
  
  ppartition = instance -> state -> partition[part_num];
  value = ppartition->elements;  /* the new values */
  /* for each element from partition (variable) */
  for (i = ppartition->start[0];(i <= ppartition->end[0]); i++) {
    i_change = FALSE;
    if (matrix[i].work)  /* is something to be done ? */ {
      /* the new values */
      for(j = 0; j < a; j++) value -> val[j] = matrix[i].val[j];
      value -> modif  = matrix[i].modif;
      value -> work = FALSE;/* i will be solved */
      
      /* start checking */
      for(j = 0; ((j < instance -> state -> length[0]) &&
		  !ploc.no_solution);j++) 
	if(i != j) { 
	  /* there is an edge */
	  if(test_constr(i,j, n)) {
	    el1=sprevise(i, j, n, &matrix[j],value,&ploc.nr_modif, 
			 &ploc.con_checks, &ploc.no_solution);
	    i_change = el1 | i_change;
	  }
	}
    }
    
    if(i_change) {
      value  -> modif = TRUE;
      ploc.modif = TRUE;
    }
    
    value++;
    MaxOutArgum(pout,&ploc);
  }
}

/*
   For each variable (i) the test if there is something to be done is made.
   If between i and another variable (j) there is a predicat and 
   for j something was changed then there is something to be done on i
*/


void 
Actual_D(int part_num, instance_p instance, void **args) {
  int i, j, n;
  PELEM matrix, value;
  partition_p ppartition;   
  
  n = instance -> state -> length[0];
  matrix=(PELEM)(instance->state->data);   /* the old values */
  ppartition=instance->state->partition[part_num];
  value = ppartition->elements;  /* the new values */	
  /* for each element from partition (variable) */
  for(i = ppartition->start[0];i <= ppartition->end[0]; i++) {
    for(j = 0; j < instance -> state -> length[0];j++) { 
      if(i != j)  {
	/* there is an edge */
	if(test_constr(i,j, n)) {
	  if(matrix[j].modif)
	    value -> work = TRUE;
	}
      }
    }		      
    value -> modif = FALSE;
    value++;
  }
}


void 
Get_Print_D(int sender, instance_p instance, void **args) {
  PIN_ARGUM pin;
  PELEM matrix;
  int ind, i;

  pin=args[0];
  matrix = (PELEM)(instance->state->data);   /* the old values */
  if (pin -> verbose) {
    for (ind = 0; ind <instance->state->length[0]; ind++)  {
      printf("\n for variable %i, the values left are: ", ind);
      for (i = 0; i < pin -> a; i++)
	if(matrix[ind].val[i])printf("%i ", i);
      printf("\n");
    }
  }
}

static po_pardscr_t Spac_loopDescr[] = {
  { sizeof(IN_ARGUM), IN_PARAM, 0, 0, 0},
  { sizeof(OUT_ARGUM), OUT_PARAM, REDUCE,  (reduction_function)MaxOutArgum, 0 }
};
static po_pardscr_t Init_DDescr[] = {
  { 80, IN_PARAM, 0, 0, 0 }
};

static po_pardscr_t Get_Print_DDescr[] = {
  { sizeof(IN_ARGUM), IN_PARAM, 0, 0, 0}
};

/* ======================================================= */
/* initialize the new_solution class                       */
/* ======================================================= */

int 
new_domain_class(int moi, int gsize, int pdebug) {
  precondition(moi>=0);
  precondition(moi<gsize);
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  register_reduction_function((reduction_function)MaxOutArgum);
  /* get the pointer to the new class */
  domain_class=new_po("DOMAIN", 1, sizeof(ELEM),NULL);
  assert(domain_class!=NULL);
  new_po_operation(domain_class,"Init_D",(po_opcode)Init_D,
		   SeqInit, 1, Init_DDescr, NULL);
  new_po_operation(domain_class,"Spac_Loop",(po_opcode)Spac_Loop,
		   ParWrite,2,Spac_loopDescr, (build_pdg_p)pdg_Spac_Loop);
  new_po_operation(domain_class,"Actual_D",(po_opcode)Actual_D,
		   ParWrite, 0,0, (build_pdg_p)pdg_Spac_Loop);
  new_po_operation(domain_class,"Get_Print_D",(po_opcode)Get_Print_D,
		   SeqRead, 1, Get_Print_DDescr, NULL);
  synch_handlers();
  return 0;
}


/* ======================================================= */
/* initialize the instance for domain_class                */
/* ======================================================= */
instance_p 
domain_instance(int number, handler_p handler) {
  instance_p matrix;  
  /* create an instance for the no_op_class */
  matrix=do_new_instance(domain_class,&number,(int *)0);
  assert(matrix!=NULL);
  if ((handler==e_parallel_blocking) || (handler==e_parallel_control)) {
    do_change_attribute(matrix, (po_opcode)Actual_D, EXECUTION, handler);
    do_change_attribute(matrix, (po_opcode)Actual_D, TRANSFER, noop);
    do_change_attribute(matrix, (po_opcode)Spac_Loop, EXECUTION, handler);
    do_change_attribute(matrix, (po_opcode)Spac_Loop, TRANSFER, noop);
  } 
  else {
    assert(handler==e_parallel_nonblocking);
    do_change_attribute(matrix, (po_opcode)Actual_D, EXECUTION, handler);
    do_change_attribute(matrix, (po_opcode)Actual_D, TRANSFER, t_sender);
    do_change_attribute(matrix, (po_opcode)Spac_Loop, EXECUTION, handler);
    do_change_attribute(matrix, (po_opcode)Spac_Loop, TRANSFER, t_sender);
  } 
  return matrix;
}



