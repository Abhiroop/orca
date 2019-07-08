#include "condition.h"
#include "amoeba.h"
#include "module/mutex.h"
#include "precondition.h"
#include "misc.h"
#include <stdlib.h>

#define MODULE_NAME "CONDITION"

struct condition_s {
  mutex l;
};

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

/*========================================================================*/
int 
init_condition(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  return 0;
}

/*========================================================================*/
int 
finish_condition() {
  if (--initialized) return 0;
  return 0;
}

/*========================================================================*/
condition_p 
new_condition() {
  condition_p cond;

  precondition(initialized);
  cond=(condition_p )malloc(sizeof(condition_t));
  sys_error(cond==NULL);
  mu_init(&(cond->l));
  return cond;
}

/*========================================================================*/
int 
free_condition(condition_p cond) {
  precondition(cond!=NULL);
  free(cond);
  return 0;
}

/*========================================================================*/
int 
signal_condition(condition_p cond) {
  precondition(initialized);
  precondition(cond!=NULL);

  mu_unlock(&(cond->l));
  return 0;
}

/*========================================================================*/
/* This primitive awaits for a condition to be signaled for time untis
   of time. If after that amount of time, the condition has not been
   signaled, it returns a <0 value. Otherwise, it return a >=0
   value. If time=0, then the primitive does not block but returns
   immediately. */
/*========================================================================*/
int 
tawait_condition(condition_p cond, po_time_t time, unit_t unit) {
  precondition(initialized);
  precondition(cond!=NULL);
  
  if (unit==PO_SEC) 
    time=time*1000;
  else
    if (unit==PO_MICROSEC) 
      time=time/1000.0;
  return mu_trylock(&(cond->l),(int )time);
}

/*========================================================================*/
int 
await_condition(condition_p cond) {
  precondition(initialized);
  precondition(cond!=NULL);

  mu_lock(&(cond->l));
  return 0;
}

