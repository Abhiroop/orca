#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pdg.h"
#include "precondition.h"

#define MODULE_NAME "pdg"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

/*=======================================================================*/
/* Initialized the module. */
/*=======================================================================*/
int 
init_pdg(int moi, int gsize, int pdebug) {
  if (initialized++) return 0;
  precondition((moi>=0)&&(moi<gsize));
  
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_graph(me, group_size, proc_debug);
  return 0;
}

/*=======================================================================*/
/* Finishes the module. */
/*=======================================================================*/
int
finish_pdg(void) 
{
  if (--initialized) return 0;
  finish_graph();
  return 0;
}


/*=======================================================================*/
/* Create a new partition dependency graph. */
/*=======================================================================*/

pdg_p 
new_pdg(int num_partitions)
{
  pdg_p pdg;

  precondition(num_partitions>=1);

  pdg=(pdg_p)malloc(sizeof(pdg_t));
  assert(pdg!=NULL);

  /* Create a graph. */
  pdg->graph=new_graph(num_partitions);
  assert(pdg->graph!=NULL);
  /* Create an array to store remote dependencies. */
  pdg->nrdeps=(int *)malloc(sizeof(int)*pdg->graph->num_nodes);
  assert(pdg->nrdeps!=NULL);
  memset(pdg->nrdeps,0,sizeof(int)*pdg->graph->num_nodes);
  pdg->with_rdeps=0;
  pdg->with_ldeps=0;
  pdg->ldeps=NULL;
  pdg->rdeps=NULL;
  pdg->valid=FALSE;
  return pdg;
}

/*=======================================================================*/
/* Frees a pdg structure. */
/*=======================================================================*/
int 
free_pdg(pdg_p pdg)
{
  precondition(pdg!=NULL);
  precondition(pdg->graph!=NULL);

  free_graph(pdg->graph);
  if (pdg->nrdeps!=NULL) free(pdg->nrdeps);
  if (pdg->ldeps!=NULL) free(pdg->ldeps);
  if (pdg->rdeps!=NULL) free(pdg->rdeps);
  free(pdg);
  return 0;
}

/*=======================================================================*/
/* Adds an edge to a graph.  */
/*=======================================================================*/
int 
add_edge(pdg_p pdg, int source, int sowner, int destination, int downer)
{
  precondition(pdg!=NULL);
  precondition(pdg->graph!=NULL);
  precondition(source>=0);
  precondition(destination>=0);
  precondition(source<pdg->graph->num_nodes);
  precondition(destination<pdg->graph->num_nodes);
  
  if (destination==source) return 0;
  if (is_edge(pdg->graph,source,destination)) return 0;

  new_edge(pdg->graph,source,destination);
  if (sowner!=downer) {
    pdg->nrdeps[source]++;
  }
  return 0;
}

/*=======================================================================*/
/* Removes an edge to a graph. */
/*=======================================================================*/

int 
remove_edge(pdg_p pdg, int source, int sowner, int destination, int downer)
{
  precondition(pdg!=NULL);
  precondition(pdg->graph!=NULL);
  precondition(source>=0);
  precondition(destination>=0);
  precondition(source<pdg->graph->num_nodes);
  precondition(destination<pdg->graph->num_nodes);

  if (!is_edge(pdg->graph,source,destination)) return 0;
  free_edge(pdg->graph,source,destination);
  if (sowner!=downer) {
    pdg->nrdeps[source]--;
  }
  return 0;
}

/*=======================================================================*/
/* Prints the graphs under the form of a list (source,destination). */
/*=======================================================================*/

int 
print_pdg(pdg_p pdg) {
  precondition(pdg!=NULL);
  
  printf("Partition Dependency Graph:\n");
  print_graph(pdg->graph);
  return 0;
}


/*=======================================================================*/
/* Returns 'rdeps' that is used to execute parallel operations more
   efficiently. */
/*=======================================================================*/

int *
get_remote_dependencies(pdg_p pdg, int *with_rdeps) {

  precondition(pdg!=NULL);

  *with_rdeps=pdg->with_rdeps;
  return pdg->rdeps;
}

/*=======================================================================*/
/* Returns 'ldeps' that is used to execute parallel operations more
   efficiently. */
/*=======================================================================*/

int *
get_local_dependencies(pdg_p pdg, int *with_ldeps) {

  precondition(pdg!=NULL);

  *with_ldeps=pdg->with_ldeps;
  return pdg->ldeps;
}


/*=======================================================================*/
/* Builds the 'ldeps' and 'rdeps' of a pdg. These are arrays that
   contain the partitions that have respectively local and remote
   dependencies. */
/*=======================================================================*/

void
build_deps(pdg_p pdg, int *owner) {
  int i, j, k;

  precondition(pdg!=NULL);

  for (i=0; i<pdg->graph->num_nodes; i++)
    if ((owner[i]==me) && pdg->nrdeps[i]) 
      pdg->with_rdeps++;
    else if (owner[i]==me)
      pdg->with_ldeps++;

  if (pdg->with_rdeps!=0) {
    pdg->rdeps=(int *)malloc(sizeof(int)*pdg->with_rdeps);
    assert(pdg->rdeps!=NULL);
    memset(pdg->rdeps,0,sizeof(int)*pdg->with_rdeps);
  }
  if (pdg->with_ldeps!=0) {
    pdg->ldeps=(int *)malloc(sizeof(int)*(pdg->graph->num_nodes-pdg->with_rdeps));
    assert(pdg->ldeps!=NULL);
  }
  for (i=0, j=0, k=0; i<pdg->graph->num_nodes; i++)
    if ((owner[i]==me) && (pdg->nrdeps[i])) {
      pdg->rdeps[j]=i;
      j++;
    }
    else if (owner[i]==me) {
      pdg->ldeps[k]=i;
      k++;
    }
}

/*=======================================================================*/
/* Checks for the total number of dependencies in the pdg.
   If it is too large, return -1. */
/*=======================================================================*/
int
analyze_pdg(pdg_p pdg)
{
  int ndps = 0;
  int i;

  for (i=0; i<pdg->graph->num_nodes; i++) {
    ndps += pdg->nrdeps[i];
  }
  if (ndps > 2 * pdg->graph->num_nodes) {
    /* On average, every partition depends on more than two other
       partitions (on other machines). In this case, a gather-all may
       be better.
    */
    return -1;
  }
  return 0;
}

/*=======================================================================*/
/* Marshalls a PDG into a buffer. The buffer consists of the size of
   the marshalled pdg, followed by the fields of the pdg structure
   except the graph, followed by the marshalled graph. */
/*=======================================================================*/
void *
marshall_pdg(pdg_p pdg) {
  char *buffer, *b;
  void *mgraph;
  int *mgraph_size;

  precondition(pdg!=NULL);
  
  mgraph=marshall_graph(pdg->graph);
  mgraph_size=(int *)mgraph;
  buffer=malloc((*mgraph_size)+sizeof(int)*(2+pdg->graph->num_nodes));
  ((int *)buffer)[0]=(*mgraph_size)+sizeof(int)*(2+pdg->graph->num_nodes);
  ((int *)buffer)[1]=pdg->valid;
  b=buffer+2*sizeof(int);
  memcpy(b,mgraph,*mgraph_size);
  b+=(*mgraph_size);
  free(mgraph);
  memcpy(b,pdg->nrdeps,sizeof(int)*pdg->graph->num_nodes);
  return buffer;
}

/*=======================================================================*/
/* Unmarshalls a PDG from a buffer. */
/*=======================================================================*/
pdg_p
unmarshall_pdg(void *b) {
  graph_p graph;
  int graph_size;
  pdg_p pdg;
  char *buffer = b;

  graph=unmarshall_graph(buffer+2*sizeof(int));
  graph_size=((int *)buffer)[2];
  pdg=new_pdg(graph->num_nodes);
  free_graph(pdg->graph);
  pdg->graph=graph;
  pdg->valid=((int *)buffer)[1];
  buffer=buffer+2*sizeof(int)+graph_size;
  pdg->nrdeps=(int *)malloc(sizeof(int)*graph->num_nodes);
  memcpy(pdg->nrdeps,buffer,sizeof(int)*graph->num_nodes);
  return pdg;
}
