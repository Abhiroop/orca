/*=====================================================================*/
/*   This module implements the compare&swap operation on integers using
   locks. */
/*=====================================================================*/

#include "atomic_int.h"
#include "precondition.h"
#include "misc.h"
#include "stdio.h"
#include "lock.h"
#include <stdlib.h>

#define MODULE_NAME "ATOMIC_INT"

struct atomic_int_s {
  int value;      /* Value of the atomic integer. */
  po_lock_p vlock;   /* Lock used to implement atomicity. */
};

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

/*=====================================================================*/
/* This procedure initializes the value of an atomic int on which
   compare&swap will be applied. It is necessary to call this
   procedure before using the atomic int. */
/*=====================================================================*/
int 
init_atomic_int(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<0) return r;
  init_lock(me,group_size,proc_debug);
  return 0;
}

/*=====================================================================*/
int 
finish_atomic_int() {
  if (--initialized) return 0;
  finish_lock();
  return 0;
}

/*=====================================================================*/
atomic_int_p 
new_atomic_int(int value)
{
  atomic_int_p aint;

  precondition(initialized);

  aint=(atomic_int_p)malloc(sizeof(atomic_int_t));
  sys_error(aint==NULL);
  aint->value=value;
  aint->vlock=new_lock();
  return aint;
}

/*=======================================================================*/
int
free_atomic_int(atomic_int_p aint) {
  precondition(initialized);
  precondition(aint!=NULL);

  free_lock(aint->vlock);
  free(aint);
  return 0;
}

/*=======================================================================*/
/* This procedure implements compare&swap atomically: if the atomic
   int is equal to 'old_value', it assigns 'new_value' to it. In any
   case, it returns the old value of the atomic int. */
/*=======================================================================*/
int 
compare_and_swap(atomic_int_p aint, int old_value, int new_value)
{
  int value_save;

  precondition(initialized);
  precondition(aint!=NULL);

  lock(aint->vlock);
  value_save=aint->value;
  if (aint->value==old_value)
    aint->value=new_value;
  unlock(aint->vlock);
  return value_save;
}

/*=======================================================================*/
/* Returns the value of an atomic int. */
/*=======================================================================*/
int 
value(atomic_int_p aint) {
  precondition(aint!=NULL);
  return aint->value;
}

/*=======================================================================*/
/* Sets the value of an atomic int. */
/*=======================================================================*/
int 
set_value(atomic_int_p aint, int value) {
  precondition(aint!=NULL);
  aint->value=value;
  return value;
}
