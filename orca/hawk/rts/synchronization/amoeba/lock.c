#include <stdlib.h>
#include "lock.h"
#include "misc.h"
#include "precondition.h"
#include "amoeba.h"
#include "module/mutex.h"

#define MODULE_NAME "LOCK"

struct po_lock_s {
  mutex mu;
};

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

/*==========================================================*/
int 
init_lock(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  return 0;
}

/*==========================================================*/
int 
finish_lock() {
  if (--initialized) return 0;
  return 0;
}

/*==========================================================*/
po_lock_p 
new_lock() {
  po_lock_p l;

  precondition_p(initialized);
  l=(po_lock_p )malloc(sizeof(po_lock_t));
  sys_error(l==NULL);
  mu_init(&(l->mu));
  return l;
}

/*==========================================================*/
int 
free_lock(po_lock_p l) {
  precondition(l!=NULL);
  free(l);
  return 0;
}

/*==========================================================*/
int 
lock(po_lock_p l) {
  precondition(initialized);
  precondition(l!=NULL);
  mu_lock(&(l->mu));
  return 0;
}

/*==========================================================*/
int 
unlock(po_lock_p l) {
  precondition(initialized);
  precondition(l!=NULL);
  mu_unlock(&(l->mu));
  return 0;
}

