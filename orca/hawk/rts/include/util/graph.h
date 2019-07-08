#ifndef __graph__
#define __graph__

#include "misc.h"

typedef struct graph_s *graph_p, graph_t;

struct graph_s {
  char *edges;
  int num_nodes;
  char **indices;
  char **rev_indices;
};

#define LOG_BYTESIZE	3
#define	BYTESIZE	(1 << LOG_BYTESIZE)

int init_graph(int moi, int gsize, int pdebug);
int finish_graph(void);
graph_p new_graph(int num_nodes);
int free_graph(graph_p g);
int new_edge(graph_p g, int source, int dest);
int free_edge(graph_p g, int source, int dest);
int print_graph(graph_p g);
int *get_edges(graph_p g, int source);	/* returns a -1 terminated list of
					   destinations.
					*/
void *marshall_graph(graph_p );
graph_p unmarshall_graph(void *buffer);

#define is_edge(p, src, dst) \
	(is_one((p)->indices[src][dst >> LOG_BYTESIZE], (dst & (BYTESIZE-1))))
/* boolean is_edge(graph_p g, int source, int dest); */


#define edge_loop(g, source, dst, code) \
	{  int __alsz = (g->num_nodes + BYTESIZE - 1) >> LOG_BYTESIZE; \
	   char *__setp = g->indices[source]; \
	   int __i, __j; \
	   for (__i = 0; __i<__alsz; __i++, __setp++) if (*__setp) { \
	     for (__j = 0; __j < BYTESIZE; __j++) { \
	       if (*__setp & (1 << __j)) { \
		 dst = (__i << LOG_BYTESIZE) + __j; \
		 code \
	       } \
	     } \
	   } \
	}

#define rev_edge_loop(g, source, dst, code) \
	{  int __alsz = (g->num_nodes + BYTESIZE - 1) >> LOG_BYTESIZE; \
	   char *__setp = g->rev_indices[source]; \
	   int __i, __j; \
	   for (__i = 0; __i<__alsz; __i++, __setp++) if (*__setp) { \
	     for (__j = 0; __j < BYTESIZE; __j++) { \
	       if (*__setp & (1 << __j)) { \
		 dst = (__i << LOG_BYTESIZE) + __j; \
		 code \
	       } \
	     } \
	   } \
	}

#endif
