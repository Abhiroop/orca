#ifndef __forest__
#define __forest__

#include "util.h"
#include "set.h"

typedef struct forest_s *forest_p, forest_t;

struct forest_s {
  int *children;
  int fanout;
  int parent;
  int *roots;
  int num_roots;
  set_p isroot;
  set_p ischild;
  float weight;
};

int init_forest(int me, int group_size, int proc_debug, int *argc, char **argv);
int finish_forest(void);

forest_p new_forest(set_p members);
int free_forest(forest_p forest);
int print_forest(forest_p forest);
#endif
