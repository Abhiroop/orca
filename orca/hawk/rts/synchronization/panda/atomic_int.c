/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 30, 1995
 *
 * Atomic integer module.
 *        Implements atomic integers on top of Panda primitives.
 */

#include "synchronization.h"	/* Part of the synchronization module */
#include "panda_atomic_int.h"
#include "pan_sys.h"

int 
init_atomic_int(int moi, int gsize, int pdebug) 
{
    return 0;
}

int 
finish_atomic_int(void) 
{
    return 0;
}

atomic_int_p 
new_atomic_int(int value)
{
  atomic_int_p a;

  a = pan_malloc(sizeof(atomic_int_t));

  a->value = value;
  a->lock  = pan_mutex_create();

  return a;
}

int
free_atomic_int(atomic_int_p a) 
{
    pan_mutex_clear(a->lock);
    pan_free(a);

    return 0;
}

int 
compare_and_swap(atomic_int_p a, int old_value, int new_value)
{
  int ret;

  pan_mutex_lock(a->lock);

  ret = a->value;
  if (a->value == old_value) a->value = new_value;

  pan_mutex_unlock(a->lock);

  return ret;
}

int 
value(atomic_int_p a) 
{
    int ret;

    pan_mutex_lock(a->lock);

    ret = a->value;

    pan_mutex_unlock(a->lock);

    return ret;
}

int 
set_value(atomic_int_p a, int value) 
{
    pan_mutex_lock(a->lock);

    a->value = value;

    pan_mutex_unlock(a->lock);

    return 0;
}
