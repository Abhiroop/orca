#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <pan_sys.h>
#include <pan_group.h>

#include "grp_channel.h"
#include "map.h"
#include "misc.h"
#include "sys_message.h"

static  int me;
static  int group_size;
static  int proc_debug;
static  boolean initialized;

static pan_group_p saniya_group;
static pan_cond_p grp_cond;
static pan_mutex_p grp_mutex;

static pan_mutex_p msg_hndlr_mutex;
static pan_cond_p msg_hndlr_cond;
static pan_thread_p msg_hndlr_thread;
static int msg_hndlr_done = 0;
static int grpno = 0;

static struct queue {
  struct queue *next;
  pan_msg_p msg;
} *msg_hndlr_queue, *msg_hndlr_free, *msg_hndlr_tail;

struct grp_channel_s {
  int grp_number;
  set_p group;
};

#define MAX_GRP 100
static grp_channel_t groups[MAX_GRP+1];

static void mc_receive(pan_msg_p);

static void process_message(pan_msg_p msg)
{
  	void *data;
  	int sender;
	int sz;
	int grpid;
	message_handler_p f;
	message_p m;

  	data = pan_msg_pop(msg, sizeof(int), alignof(int));
  	grpid = *(int *) data;
  	data = pan_msg_pop(msg, sizeof(int), alignof(int));
  	sender = *(int *) data;
	if (is_member(groups[grpid].group, me)) {
  		data = pan_msg_pop(msg, sizeof(int), alignof(int));
  		sz = *(int *) data;
		data = pan_msg_pop(msg, sz, alignof(double));
		m = new_message(0, sz);
		memcpy(m->message, data, sz);
		m->user_data = m->user_header+m->message_header->user_header_size;
		m->max_user_data_size = sz - m->message_header->user_header_size;
		f = message_get_handler(m);
  		if (f!=NULL)
			if (! (*(f))(m)) {
				free_message(m);
			}
		else {
			fprintf(stderr, "%d: message deleted, no handler\n", me);
		}
	}
  	pan_msg_clear(msg);
  	if (me == sender) {
		pan_mutex_lock(grp_mutex);
		pan_cond_signal(grp_cond);
		pan_mutex_unlock(grp_mutex);
  	}
}

static void
grp_msg_hndlr(void *arg)
{
  pan_mutex_lock(msg_hndlr_mutex);
  while (! msg_hndlr_done) {
    if (msg_hndlr_queue) {
	void *msg = msg_hndlr_queue->msg;
	struct queue *n = msg_hndlr_queue->next;

	msg_hndlr_queue->next = msg_hndlr_free;
	msg_hndlr_free = msg_hndlr_queue;
	msg_hndlr_queue = n;
	if (! n) {
		msg_hndlr_tail = 0;
	}
	pan_mutex_unlock(msg_hndlr_mutex);
	process_message(msg);
	pan_mutex_lock(msg_hndlr_mutex);
	continue;
    }
    pan_cond_wait(msg_hndlr_cond);
  }
  pan_thread_exit();
}


/*=======================================================================*/
/* Initializes the module. */

int init_grp_channel(int member_number, int num_members, int pdebug)
{
  if ((initialized) && (me==member_number) && (num_members==group_size)) 
    return 0;
  assert(!initialized);

  grp_mutex = pan_mutex_create();
  grp_cond = pan_cond_create(grp_mutex);

  msg_hndlr_mutex = pan_mutex_create();
  msg_hndlr_cond = pan_cond_create(msg_hndlr_mutex);
  msg_hndlr_thread = pan_thread_create(grp_msg_hndlr, (void *) 0, 0, 0, 1);

  me=member_number;
  group_size=num_members;
  proc_debug=pdebug;

  saniya_group = pan_group_join("saniya_group", mc_receive);
  pan_group_await_size(saniya_group, num_members);

  initialized=TRUE;
  return 0;
}


/*=======================================================================*/
/* Finishes the module. */
 
int finish_grp_channel()
{
  if (!initialized) return 0;
  pan_mutex_lock(msg_hndlr_mutex);
  msg_hndlr_done = 1;
  pan_cond_signal(msg_hndlr_cond);
  pan_mutex_unlock(msg_hndlr_mutex);
  pan_cond_clear(msg_hndlr_cond);
  pan_mutex_clear(msg_hndlr_mutex);
  pan_group_leave(saniya_group);
  pan_group_clear(saniya_group);
  pan_mutex_lock(grp_mutex);
  pan_cond_broadcast(grp_cond);
  pan_mutex_unlock(grp_mutex);
  pan_cond_clear(grp_cond);
  pan_mutex_clear(grp_mutex);

  return 0;
}


/*=======================================================================*/
/* Thread listening to group message. */

static void mc_receive(pan_msg_p msg)
{
  struct queue *n;

  pan_mutex_lock(msg_hndlr_mutex);
  if (! msg_hndlr_free) {
	n = pan_malloc(sizeof(struct queue));
  }
  else {
	n = msg_hndlr_free;
	msg_hndlr_free = n->next;
  }
  if (msg_hndlr_tail) {
	msg_hndlr_tail->next = n;
  }
  else	msg_hndlr_queue = n;
  msg_hndlr_tail = n;
  n->next = 0;
  n->msg = msg;
  pan_cond_signal(msg_hndlr_cond);
  pan_mutex_unlock(msg_hndlr_mutex);
}

/*=======================================================================*/
/* Creates a new grp_channel. */
/*=======================================================================*/

grp_channel_p new_grp_channel(set_p processors)
{
  grp_channel_p grp;
  
  if (grpno >= MAX_GRP) return NULL;
  grp = &groups[grpno];
  grp->grp_number = grpno;
  grpno++;
  if (processors == NULL) {
	grp->group = new_set(group_size);
	full_set(grp->group);
  }
  else {
	grp->group = duplicate_set(processors);
  }
  return grp;
}

/*=======================================================================*/
/* Frees a grp channel. All channel must be freed in the same order
   by all members. */

int free_grp_channel(grp_channel_p gch)
{
  assert(gch!=NULL);

  free_set(gch->group);
  return 0;
}
  

/*=======================================================================*/
/* Sends a message to all members of the group. */
 
int grp_channel_send(grp_channel_p gch, message_p m)
{
  void *p;
  pan_msg_p msg = pan_msg_create();
  int sz = m->message_header->user_data_size+
	   m->message_header->user_header_size+
	   sizeof(message_header_t);

  assert(gch!=NULL);
  
  p = pan_msg_push(msg, sz, alignof(double));
  memcpy(p, m->message, sz);
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = sz;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = me;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = gch->grp_number;
  pan_mutex_lock(grp_mutex);
  pan_group_send(saniya_group, msg);
  pan_cond_wait(grp_cond);
  pan_mutex_unlock(grp_mutex);
  return 0;
}
