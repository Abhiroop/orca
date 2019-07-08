#ifndef __space__
#define __space__

#include <stdlib.h>

/* Allocate space for partitioned objects (may use mmap instead of malloc). */

int init_space_module(int moi, int gsize, int pdebug, int *argc, char **argv);
int finish_space_module(void);
void *get_space(size_t size);
void free_space(void *p);

#endif
