#ifndef __pdg__
#define __pdg__

#include "sys_po.h"
#include "misc.h"
#include "graph.h"

struct pdg_s {
  graph_p graph;
  int with_rdeps; /* Number of partitions with remote dependencies. */
  int with_ldeps; /* Number of partitions with no remote dependencies. */
  int *nrdeps;    /* Number of remote dependencies for each partition. */
  int *ldeps;     /* Partitions that have local dependencies only. */
  int *rdeps;     /* Partitions that have remote dependencies. */
  boolean valid;  /* Indicates whether all platforms have the same pdg. */
};

int init_pdg(int moi, int gsize, int pdebug);
int finish_pdg(void);

pdg_p new_pdg(int num_partitions);
int free_pdg(pdg_p pdg);

int print_pdg(pdg_p pdg);

int add_edge(pdg_p pdg, int source, int sowner, int destination, int downer);
int remove_edge(pdg_p pdg, int source, int sowner, int destination, int downer);
int *get_remote_dependencies(pdg_p, int *with_rdeps);
int *get_local_dependencies(pdg_p, int *with_ldeps);
int analyze_pdg(pdg_p);
void build_deps(pdg_p pdg, int *owner);
void *marshall_pdg(pdg_p);
pdg_p unmarshall_pdg(void *buffer);

#endif
