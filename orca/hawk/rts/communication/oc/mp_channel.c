#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pan_sys.h>
#include <pan_mp.h>
#include <pan_group.h>

#include "mp_channel.h"
#include "misc.h"
#include "map.h"
#include "sys_message.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized=FALSE;
static int ch_num;

static void multi_receive(pan_msg_p);
static void mp_receive(int, pan_msg_p);

struct mp_channel_s {
  int ch_number;
  set_p group;
  pan_group_p grp;
  char name[128];
  int mp_map;
};

#define MAX_FCH 20
static struct mp_channel_s channels[MAX_FCH+1];

static pan_mutex_p mp_mutex;
static pan_cond_p mp_cond;
static pan_thread_p mp_thread;
static int mp_done;

static struct queue {
  struct queue *next;
  pan_msg_p msg;
} *mp_queue, *mp_free, *mp_tail;


static void process_mp_message(pan_msg_p msg)
{
  void *data; 
  int sz, sender, chn; 
  mp_channel_p ch; 
  message_handler_p f; 
  message_p m; 
 
  data = pan_msg_pop(msg, sizeof(int), alignof(int)); 
  chn = *(int *) data; 
  ch = &channels[chn]; 
  data = pan_msg_pop(msg, sizeof(int), alignof(int)); 
  sender = *(int *) data; 
  if (sender != me) {
  	data = pan_msg_pop(msg, sizeof(int), alignof(int)); 
  	sz = *(int *) data; 
  	data = pan_msg_pop(msg, sz, alignof(double)); 
  	m = new_message(0, sz); 
  	memcpy(m->message, data, sz); 
  	m->user_data = m->user_header+m->message_header->user_header_size; 
  	m->max_user_data_size = sz - m->message_header->user_header_size; 
  	f = message_get_handler(m); 
  	if (! (*f)(m)) {
		free_message(m);
	}
  }
  pan_msg_clear(msg); 
}

static void
mp_msg_hndlr(void *arg)
{
  pan_mutex_lock(mp_mutex);
  while (! mp_done) {
    if (mp_queue) {
      pan_msg_p msg = mp_queue->msg;
      struct queue *n = mp_queue->next;

      mp_queue->next = mp_free;
      mp_free = mp_queue;
      mp_queue = n;
      if (! n) mp_tail = 0;
      pan_mutex_unlock(mp_mutex);
      process_mp_message(msg);
      pan_mutex_lock(mp_mutex);
      continue;
    }
    pan_cond_wait(mp_cond);
  }
  pan_thread_exit();
}

/*=======================================================================*/

/* Initializes the module. */

int init_mp_channel(int moi, int gsize, int pdebug)
{
  int i;
  if ((initialized) && (me==moi) && (gsize==group_size)) return 0;


  me=moi;
  group_size=gsize;
  proc_debug=pdebug;

  mp_mutex = pan_mutex_create();
  mp_cond = pan_cond_create(mp_mutex);
  mp_thread = pan_thread_create(mp_msg_hndlr, (void *) 0, 0, 0, 1);

  for (i = 0; i <= MAX_FCH; i++) {
	sprintf(channels[i].name, "multi_group%d", i);
        channels[i].mp_map = pan_mp_register_map();
        pan_mp_register_async_receive(channels[i].mp_map, mp_receive);
  }

  initialized=TRUE;
  return 0;
}

/*=======================================================================*/
/* Finishes */
/*=======================================================================*/

int finish_mp_channel()
{
  int i;

  if (!initialized) return 0;
  pan_mutex_lock(mp_mutex);
  mp_done = 1;
  pan_cond_signal(mp_cond);
  pan_mutex_unlock(mp_mutex);
  pan_cond_clear(mp_cond);
  pan_mutex_clear(mp_mutex);
  for (i = 0; i <= MAX_FCH; i++) {
	pan_mp_clear_map(channels[i].mp_map, pan_msg_clear);
	pan_mp_free_map(channels[i].mp_map);
  }
  initialized=FALSE;
  return 0;
}


/*=======================================================================*/
/* Creates a new channel on which only members specified in "group"
   can send and receive message. For each message received on this
   channel, the corresponding handler will be called by the Panda
   upcall. All channels must be registered in the same order on all
   processors. */
/*=======================================================================*/

mp_channel_p new_mp_channel(set_p group)
{
  mp_channel_p ch;
  int sz;

  if (ch_num >= MAX_FCH) return NULL;

  ch= &channels[ch_num];
  ch->ch_number = ch_num;
  ch_num++;
  if (group==NULL)
    {
      ch->group=new_set(group_size);
      full_set(ch->group);
      sz = group_size;
    }
  else {
    int i;
    sz = 0;
    for (i = 0; i < group_size; i++) {
	if (is_member(group, i)) sz++;
    }
    ch->group=duplicate_set(group);
  }
  
  if (is_member(ch->group,me))
    {
      ch->grp = pan_group_join(ch->name, multi_receive);
      pan_group_await_size(ch->grp, sz);
    }
  else ch->grp = 0;
  return ch;
}
  

/*=======================================================================*/
/* Unregisters MP addresses corresponding to the channel and cleans
   up all data structures allocated for the channel. */
/*=======================================================================*/

int free_mp_channel(mp_channel_p ch)
{
  if (is_member(ch->group, me)) {
	pan_group_leave(ch->grp);
	pan_group_clear(ch->grp);
  }
  return 0;
}
 
 
/*=======================================================================*/


/*=======================================================================*/
/* Sends a message to a node. */

int mp_channel_unicast(mp_channel_p ch, int receiver, message_p m)
{
  void *p;
  pan_msg_p msg = pan_msg_create();
  int sz = m->message_header->user_data_size+
	   m->message_header->user_header_size+
	   sizeof(message_header_t);

  p = pan_msg_push(msg, sz, alignof(double));
  memcpy(p, m->message, sz);
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = sz;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = me;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = ch->ch_number;

  pan_mp_send_message(receiver, ch->mp_map, msg, MODE_ASYNC);
  return 0;
}
 
 
/*=======================================================================*/
/* Multicasts a message to all nodes. */

int mp_channel_multicast(mp_channel_p ch, message_p m)
{
  void *p;
  pan_msg_p msg = pan_msg_create();
  int sz = m->message_header->user_data_size+
	   m->message_header->user_header_size+
	   sizeof(message_header_t);
 
  p = pan_msg_push(msg, sz, alignof(double));
  memcpy(p, m->message, sz);
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = sz;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = me;
  p = pan_msg_push(msg, sizeof(int), alignof(int));
  *(int *) p = ch->ch_number;
  pan_group_send(ch->grp, msg);
  return 0;
}

/*=======================================================================*/
/* Receivers */

#define rcv(msg) \
  struct queue *n; \
  pan_mutex_lock(mp_mutex); \
  if (! mp_free) { \
	n = pan_malloc(sizeof(struct queue)); \
  } \
  else { \
	n = mp_free; \
	mp_free = n->next; \
  } \
  if (mp_tail) mp_tail->next = n; else mp_queue = n; \
  mp_tail = n; \
  n->next = 0; n->msg = msg; \
  pan_cond_signal(mp_cond); \
  pan_mutex_unlock(mp_mutex);


static void multi_receive(pan_msg_p msg)
{
	rcv(msg);
}

static void mp_receive(int nap, pan_msg_p msg)
{
	rcv(msg);
}
