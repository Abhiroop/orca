#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "stderr.h"
#include "thread.h"
#include "module/signals.h"
#include "semaphore.h"
#include "demultiplex.h"
#include "fm.h"
#include "sys_message.h"
#include "pipe.h"
#include "misc.h"
#include "precondition.h"
#include "map.h"
#include "po_timer.h"
#include "assert.h"

#define MODULE_NAME "DEMULTIPLEX"
#define STACKSIZE 64000

static int me;
static int group_size;
static int proc_debug;
static boolean initialized=FALSE;
static map_p protocols;
static semaphore message_arrived;
static po_timer_p at_receive;

extern void _await_intr();
extern void FM_enable_intr(void);
extern void FM_set_parameter();

static void demultiplexer_thread(char *c, int i);
static void my_sys_catcher(signum sig, thread_ustate *us, void *extra);

/*=======================================================================*/
/* Initializes module. */
/*=======================================================================*/
int 
init_demultiplex(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<0) return r;
  if (initialized) return 0;
  FM_set_parameter(FM_QUIET_START);
  FM_initialize();
  protocols=new_map();
  init_pipe(me,group_size,proc_debug);
  init_po_timer(me,group_size,proc_debug);
  at_receive=new_po_timer("receive");
  thread_enable_preemption();
  sema_init(&message_arrived,0);
  r=thread_newthread(demultiplexer_thread,STACKSIZE,NULL,0);
  assert(r>=0);
  initialized=TRUE;
  return 0;
}

/*=======================================================================*/
/* Finishes module. */
/*=======================================================================*/
int
finish_demultiplex() {
  if (!initialized) return -1;
  free_map(protocols);
  initialized=FALSE;
  return 0;
}

/*=======================================================================*/
/* Creates a new structure for a protocol. If the protocol requires
   messages to be polled, it creates a pipe on which seperate thread
   handlers can block. */
/*=======================================================================*/
protocol_p
new_protocol() {
  protocol_p p;

  precondition(initialized);
  p=(protocol_p )malloc(sizeof(protocol_t));
  assert(p!=NULL);
  p->pid=insert_item(protocols,(item_p)p);
  p->pipe=new_pipe();
  assert(p->pipe!=NULL);
  return p;
}

/*=======================================================================*/
/* Frees a structure associated with a protocol. */
/*=======================================================================*/
int 
free_protocol(protocol_p p) {
  precondition(initialized);
  precondition(p!=NULL);
  if (remove_item_p(protocols,(item_p)p)<0) return -1;
  free_pipe(p->pipe);
  free(p);
  return 0;
}

/*=======================================================================*/
/* Handles incoming messages. If the message must be handled by an
   upcall, this handler makes the upcall, which is not supposed to
   block. Otherwise, it puts the buffer in a pipe that must be handled
   by another thread. */
/*=======================================================================*/
void receive_buffer(struct FM_buffer *buffer, int size) {
  message_t message;
  message_handler_p handler;
  protocol_p protocol;
  
  message_unmarshall(&message,buffer->fm_buf);
  /* Check which procotol the message belongs to. */
  if (message.message_header->protocol==UPCALL) {
    handler=message_get_handler(&message);
    assert(handler!=NULL);
    if ((*handler)(&message)) {
	fprintf(stderr, "%d: handler returns 1, not supported by FM communication layer\n", me);
    }
    FM_free_buf(buffer);
  }
  else {  
    protocol=get_item(protocols,message.message_header->protocol);
    assert(protocol!=NULL);
    pipe_append(protocol->pipe,buffer);
  }
  return;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/
 
/*=======================================================================*/
/* Forces an arriving message to be processed upon message arrival
   signal catch. */
/*=======================================================================*/
void
my_sys_catcher(signum sig, thread_ustate *us, void *extra) {
  FM_extract();
  FM_enable_intr();
}

/*=======================================================================*/
/* Thread that gets interrupted upon message arrival. */
/*=======================================================================*/
void 
demultiplexer_thread(char *c, int size) {
  sig_catch(-123, my_sys_catcher, (void *) NULL);
  _await_intr(0,-123);
  FM_enable_intr();
  for (;;) {
    sema_down(&message_arrived);
  }
}
