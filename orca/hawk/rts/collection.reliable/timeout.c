#include <stdlib.h>
#include "assert.h"
#include "timeout.h"
#include "condition.h"

struct timeout_s {
  condition_p timeout_locked;
};

static int initialized;

int 
init_timeout(int moi, int gsize, int pdebug, int *argc, char **argv){
  if (initialized++) return 0;
  init_condition(moi,gsize,pdebug);
  return 0;
}

int 
finish_timeout(void) {
  
  if (--initialized) return 0;
  finish_condition();
  return 0;
}


/*=======================================================================*/
/* Creates and initializes a new timeout_s structure. */
/*=======================================================================*/

timeout_p 
new_timeout(int mint, int intt, int maxt) {
  timeout_p t;

  t=(timeout_p)malloc(sizeof(timeout_t));
  assert(t!=NULL);

  t->timeout_locked=new_condition();
  return t;
}


/*=======================================================================*/
/* Frees a timeout structure. */
/*=======================================================================*/

int 
free_timeout(timeout_p t) {
  free_condition(t->timeout_locked);
  free(t);
  return 0;
}


/*=======================================================================*/
/* Blocks for at most (min_timeout+sum_timeout) milliseconds. Then adds
   interv_timeout to sum_timeout. If the procedure does not block for the
   entire time, it returns 0, otherwise, it returns sum_timeout.
   (The reliable-communication version just blocks).
/*=======================================================================*/

int 
do_timeout(timeout_p t) {
  await_condition(t->timeout_locked);
  signal_condition(t->timeout_locked);
  return 0;
}


/*=======================================================================*/
/* Reset the value of sum_timeout in a timeout and unlocks the timeout. */
/*=======================================================================*/

int 
cancel_timeout(timeout_p t) {
  signal_condition(t->timeout_locked);
  return 0;
}


/*=======================================================================*/
/* Blocks until timeout is released and then locks the timeout. */
/*=======================================================================*/

int 
start_timeout(timeout_p t) {
  await_condition(t->timeout_locked);
  return 0;
}

int
reset_timeout(timeout_p t) {
  return 0;
}

int
getmaxt(timeout_p t) {
  return 1;
}
