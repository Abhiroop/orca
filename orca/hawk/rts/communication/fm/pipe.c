#include <stdlib.h>
#include <assert.h>
#include "amoeba.h"
#include "semaphore.h"
#include "mutex.h"
#include "pipe.h"
#include "misc.h"
#include "precondition.h"
#include "sys_message.h"

#define MODULE_NAME "PIPE"

typedef struct list_header_s list_header_t, *list_header_p;

struct pipe_s {
  mutex lock;
  semaphore full;
  struct FM_buffer *root;
  struct FM_buffer *tail;
  int last_key;
};

struct list_header_s {
  struct FM_buffer *next;
  struct FM_buffer *pred;
};

static int me;
static int group_size;
static int proc_debug;
static boolean initialized=FALSE;


static struct FM_buffer *get_pred(struct FM_buffer *b);
static struct FM_buffer *get_next(struct FM_buffer *b);
static int set_next(struct FM_buffer *b, struct FM_buffer *bnext);
static int set_pred(struct FM_buffer *b, struct FM_buffer *bpred);
static int get_key(struct FM_buffer *b);
static int remove_buffer(pipe_p l, struct FM_buffer *b);
static struct FM_buffer *get_buffer(pipe_p l, int key);

/*=======================================================================*/
/* Initialized module. */
/*=======================================================================*/
int init_pipe(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<0) return r;
  if (initialized) return 0;
  initialized=TRUE;
  return 0;
}

/*=======================================================================*/
/* Finishes module. */
/*=======================================================================*/
int finish_pipe() {
  initialized=FALSE;
  return 0;
}

/*=======================================================================*/
/* Creates a new pipe. */
/*=======================================================================*/
pipe_p 
new_pipe() {
  pipe_p l;

  precondition(initialized);
  l=(pipe_p )malloc(sizeof(pipe_t));
  sys_error(l==NULL);
  l->root=NULL;
  l->tail=NULL;
  l->last_key=0;
  sema_init(&(l->full),0);
  mu_init(&(l->lock));
  return l;
}

/*=======================================================================*/
/* Frees a pipe. */
/*=======================================================================*/
int
free_pipe(pipe_p p) {
  void *data;

  precondition(initialized);
  precondition(p!=NULL);
  
  while (p->root!=NULL) {
    data=pipe_retrieve(p);
    FM_free_buf(data);
  }
  free(p);
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
int pipe_append(pipe_p l, struct FM_buffer *buffer) {
  precondition(initialized);
  precondition(l!=NULL);

  mu_lock(&(l->lock));
  if (l->root==NULL) {
    /* First element in list. */ 
    l->root=buffer;
    l->tail=l->root;
    set_next(buffer,NULL);
    set_pred(buffer,NULL);
  }
  else {
    /* Not first element. */
    assert(l->root!=NULL);
    assert(l->tail!=NULL);
    set_next(l->tail,buffer);
    set_next(buffer,NULL);
    set_pred(buffer,l->tail);
    l->tail=buffer;
  }
  mu_unlock(&(l->lock));
  if (get_key(buffer)==(l->last_key+1))
    sema_up(&(l->full));
  return 0;
}

/*=======================================================================*/
/*=======================================================================*/
struct FM_buffer *
pipe_retrieve(pipe_p l) {
  int i;
  struct FM_buffer *buffer, *b;

  precondition_p(initialized);
  precondition_p(l!=NULL);
  sema_down(&(l->full));
  assert(l->root!=NULL);
  assert(l->tail!=NULL);
  /* Search for the next element that has key last_key +1. */
  mu_lock(&(l->lock));
  buffer=get_buffer(l,l->last_key+1);
  assert(buffer!=NULL);
  remove_buffer(l,buffer);
  (l->last_key)++;
  i=l->last_key+1;
  while ((b=get_buffer(l,i))!=NULL) { 
    sema_up(&(l->full)); 
    i++;
  }
  mu_unlock(&(l->lock));
  return buffer;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

/*=======================================================================*/
/* Sets the 'next' field in a buffer. */
/*=======================================================================*/
int 
set_next(struct FM_buffer *b, struct FM_buffer *bnext) {
  list_header_p h;

  h=(list_header_p )b;
  h->next=bnext;
  return 0;
}

/*=======================================================================*/
/* Sets the 'pred' field in a buffer. */
/*=======================================================================*/
int 
set_pred(struct FM_buffer *b, struct FM_buffer *bpred) {
  list_header_p h;

  assert(b!=NULL);
  h=(list_header_p )b;
  h->pred=bpred;
  return 0;
}

/*=======================================================================*/
/* Returns the 'next' field in a buffer. */
/*=======================================================================*/
struct FM_buffer *
get_next(struct FM_buffer *b) {
  list_header_p h;

  h=(list_header_p )b;
  return h->next;
}

/*=======================================================================*/
/* Returns the 'pred' field in a buffer. */
/*=======================================================================*/
struct FM_buffer *
get_pred(struct FM_buffer *b) {
  list_header_p h;

  assert(b!=NULL);
  h=(list_header_p )b;
  return h->pred;
}

/*=======================================================================*/
/* Gets the key in a buffer. */
/*=======================================================================*/
int
get_key(struct FM_buffer *b) {
  message_header_p h;

  assert(b!=NULL);
  h=(message_header_p)(&(b->fm_buf[0]));
  return h->seqno;
}

/*=======================================================================*/
/*=======================================================================*/
int
remove_buffer(pipe_p l, struct FM_buffer *b) {
  struct FM_buffer *next, *pred;

  assert(l!=NULL);
  assert(b!=NULL);
  if ((l->root==b) && (l->tail==b)) {
    /* only that element is in the list. */
    l->root=NULL;
    l->tail=NULL;
    return 0;
  }
  if (l->root==b) {
    /* buffer is at root. */
    l->root=get_next(b);
    return 0;
  }
  if (l->tail==b) {
    /* buffer is at tail. */
    l->tail=get_pred(b);
    set_next(l->tail,NULL);
    return 0;
  }
  /* Buffer is in middle of list. */
  pred=get_pred(b);
  next=get_next(b);
  set_next(pred,next);
  set_pred(next,pred);
  return 0;
}

/*=======================================================================*/
/* Returns a pointer to the buffer with the specified key if it exists
   in the pipe. */
/*=======================================================================*/
struct FM_buffer *
get_buffer(pipe_p l, int key) {
  struct FM_buffer *b;

  assert(l!=NULL);
  assert(key>l->last_key);
  b=l->root;
  while ((b!=NULL) && (get_key(b)!=key)) b=get_next(b);
  assert((b==NULL) || (get_key(b)==key));
  return b;
}

