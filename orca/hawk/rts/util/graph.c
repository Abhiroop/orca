/*#####################################################################*/
/* Author: Saniya Ben Hassen 
   Updated by: Ceriel Jacobs 
   This modules implements graphs. Nodes are numbered from 0 to
   n-1. Module implements 
   - creation and deletion of graphs, 
   - insertion and deletion of edges, 
   - marshaling and unmarshaling of graphs, 
   - constructions of a list of edges of a graph. 
   - printing of a graph. */
/*#####################################################################*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "graph.h"
#include "misc.h"
#include "precondition.h"
#include "assert.h"

#define MODULE_NAME "graph"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

int
init_graph(int moi, int gsize, int pdebug) {
  if (initialized++) return 0;
  precondition((moi>=0)&&(moi<gsize));
  
  assert(sizeof(char)==1);
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  return 0;
}

int
finish_graph() {
  if (--initialized) return 0;
  return 0;
}

graph_p
new_graph(int num_nodes) {

  graph_p g;
  int alsz = (num_nodes + BYTESIZE - 1) & ~(BYTESIZE - 1);
  int i;

  precondition_p(initialized);
  precondition_p(num_nodes>0);
  
  g=(graph_p )malloc(sizeof(graph_t));
  assert(g!=NULL);
  g->num_nodes=num_nodes;
  /* Allocate graph for edges. */
  g->edges=(char *)malloc(2*ceiling(alsz*num_nodes,BYTESIZE));
  assert(g->edges!=NULL);
  memset(g->edges,0,2*ceiling(alsz*num_nodes,BYTESIZE));
  g->indices = (char **) malloc(num_nodes * sizeof(char *));
  g->rev_indices = (char **) malloc(num_nodes * sizeof(char *));
  assert(g->indices!=NULL);
  assert(g->rev_indices!=NULL);
  for (i = 0; i < num_nodes; i++) {
	g->indices[i] = &(g->edges[(alsz>>LOG_BYTESIZE) * i]);
	g->rev_indices[i] = &(g->edges[(alsz>>LOG_BYTESIZE) * (i+num_nodes)]);
  }
  return g;
}

int
free_graph(graph_p g) {
  
  precondition(initialized);
  precondition(g!=NULL);
  
  free(g->edges);
  free(g->indices);
  free(g->rev_indices);
  free(g);
  return 0;
}

int
new_edge(graph_p g, int source, int dest) {
  
  precondition(initialized);
  precondition(g!=NULL);
  precondition(source>=0);
  precondition(dest>=0);
  precondition(source<g->num_nodes);
  precondition(dest<g->num_nodes);

  if (is_edge(g, source, dest)) return -1;
  g->indices[source][dest >> LOG_BYTESIZE] =
	set_bit(g->indices[source][dest >> LOG_BYTESIZE], dest & (BYTESIZE-1));
  g->rev_indices[dest][source >> LOG_BYTESIZE] =
	set_bit(g->rev_indices[dest][source >> LOG_BYTESIZE], source & (BYTESIZE-1));

  return 0;
}

int
free_edge(graph_p g, int source, int dest) {
  
  precondition(initialized);
  precondition(g!=NULL);
  precondition(source>=0);
  precondition(dest>=0);
  precondition(source<g->num_nodes);
  precondition(dest<g->num_nodes);

  if (! is_edge(g, source, dest)) return -1;
  g->indices[source][dest >> LOG_BYTESIZE] =
	reset_bit(g->indices[source][dest >> LOG_BYTESIZE], dest & (BYTESIZE-1));
  g->rev_indices[dest][source >> LOG_BYTESIZE] =
	reset_bit(g->rev_indices[dest][source >> LOG_BYTESIZE], source & (BYTESIZE-1));
  return 0;
}

int *
get_edges(graph_p g, int source)
{
  int *list, *p;
  char *setp;
  int i;
  int alsz;

  precondition(initialized);
  precondition(g!=NULL);
  precondition(source>=0);
  precondition(source<g->num_nodes);

  alsz = (g->num_nodes + BYTESIZE - 1) >> LOG_BYTESIZE;
  setp = g->indices[source];
  list = p = malloc(g->num_nodes * sizeof(int));
  for (i = 0; i<alsz; i++) {
    if (*setp) {
      int j;
      for (j = 0; j < BYTESIZE; j++) {
	if (*setp & (1 << j)) {
	  *p++ = (i << LOG_BYTESIZE) + j;
	}
      }
    }
    setp++;
  }
  *p = -1;
  return list;
}

int
print_graph(graph_p g) {

  int i,j;
  int count;

  precondition(initialized);
  precondition(g!=NULL);

  count=0;
  for (i=0; i<g->num_nodes; i++)
    for (j=0; j<g->num_nodes; j++) {
      if (is_edge(g,i,j)) {
	printf("(%d,%d)\t",i,j);
	count++;
	if ((count % 10)==0) printf("\n");
      }
    }
  printf("\n");
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
void *
marshall_graph(graph_p graph) {
  char *buffer;
  int alsz;

  precondition(graph!=NULL);

  alsz = (graph->num_nodes + BYTESIZE - 1) & ~(BYTESIZE - 1);
  buffer=malloc(2*sizeof(int)+ceiling(alsz*graph->num_nodes,BYTESIZE));
  ((int *)buffer)[0]=2*sizeof(int)+ceiling(alsz*graph->num_nodes,BYTESIZE);
  ((int *)buffer)[1]=graph->num_nodes;
  memcpy(buffer+2*sizeof(int),graph->edges,ceiling(alsz*graph->num_nodes,BYTESIZE));
  return buffer;	    
}

/*=======================================================================*/
/*=======================================================================*/
graph_p 
unmarshall_graph(void *b) {
  graph_p graph;
  int mgraph_size;
  int alsz;
  char *buffer = b;

  precondition(buffer!=NULL);

  mgraph_size=((int *)buffer)[0];
  graph=new_graph(((int *)buffer)[1]);
  alsz = (graph->num_nodes + BYTESIZE - 1) & ~(BYTESIZE - 1);
  memcpy(graph->edges,buffer+2*sizeof(int),ceiling(alsz*graph->num_nodes,BYTESIZE));
  return graph;
}
