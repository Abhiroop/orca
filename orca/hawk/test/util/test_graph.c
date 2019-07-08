#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "graph.h"

int 
main(int argc, char *argv[]) {

  graph_p g;

  init_graph(0,1,-1);
  g=new_graph(5);
  assert(g!=NULL);
  assert(new_edge(g,0,1)>=0);
  assert(new_edge(g,4,3)>=0);
  assert(new_edge(g,2,1)>=0);
  assert(new_edge(g,2,1)<0);
  assert(new_edge(g,3,4)>=0);
  assert(new_edge(g,4,4)>=0);
  assert(free_edge(g,4,4)>=0);
  assert(free_edge(g,4,4)<0);
  assert(is_edge(g,0,1)==TRUE);
  assert(is_edge(g,4,3)==TRUE);
  assert(is_edge(g,2,1)==TRUE);
  assert(is_edge(g,3,4)==TRUE);
  free_graph(g);
  finish_graph();
  fprintf(stderr,"%s: Test Finished Successefully\n", argv[0]);
  return 0;
}
