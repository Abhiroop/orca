#ifndef __collection__
#define __collection__

#include "set.h"
#include "reduction_function.h"

typedef struct collection_s *collection_p, collection_t;

/* =================================================================
   must be called by ALL processes.
   ================================================================= */
int init_collection(int moi, int gsize, int pdebug, int *argc, char **argv);
int finish_collection(void);

collection_p new_collection(set_p set);
int free_collection(collection_p coll);

/* =================================================================
   must be called only by the processes belonging to the collection.
   ================================================================= */
int gather(collection_p coll, int caller, void *sbuffer, int ssize,
	   void *rbuffer, int rsize);
int gather_all(collection_p coll, void *sbuffer, int ssize,
	       void *rbuffer, int rsize);
int gather_po(collection_p coll, void *data);
int reduce_all(collection_p coll, void *sbuffer, int ssize,
	       void *rbuffer, int rsize, reduction_function_p rf);
int reduce(collection_p coll, int caller, void *sbuffer, int ssize,
	       void *rbuffer, int rsize, reduction_function_p rf);
int barrier(collection_p coll);

int collection_wait(collection_p coll);

int send_exit(collection_p coll);

int coll_add_instance(collection_p coll, void *instance);

#endif
