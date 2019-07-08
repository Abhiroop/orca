#ifndef __atomic_int__
#define __atomic_int__

/* This module defines an abstract data type that encapsulates an
   integer. This integer can be updated atomically using the
   compare_and_swap procedure. */

typedef struct atomic_int_s *atomic_int_p, atomic_int_t;

int init_atomic_int(int me, int group_size, int proc_debug);
int finish_atomic_int(void);

atomic_int_p new_atomic_int(int value);
int free_atomic_int(atomic_int_p );

int compare_and_swap(atomic_int_p aint, int old_value, int new_value);
int value(atomic_int_p aint);
int set_value(atomic_int_p aint, int value);

#endif
