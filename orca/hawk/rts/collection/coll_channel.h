#ifndef __coll_channel__
#define __coll_channel__

#include "sys_collection.h"
#include "lock.h"
#include "set.h"
#include "mp_channel.h"

struct coll_header_s {
  int sender;
  int channel_number;
  int opcode;
  int rf;
  int ticket;
  int caller;
};

/* Contains all descriptors that are modified from one operation to
   another except buffers */
struct coll_opdescr_s {
  int caller;
  set_p received;
  int num_received_from_children;         
  int num_received_from_roots;
  coll_operation_p coll_operation;
  reduction_function_p rf;
  int ticket;
  int rbuffer_set;
  int smessage_set;
  void *scratch;
};

/* Contains all temporary buffers and messages */
struct coll_buffers_s {
  void *sbuffer;
  int ssize;
  char *rbuffer;
  int rsize;
  /* message used to send data to parent or children */
  message_p smessage;
  /* message used to store data from roots or children temporarily */
  message_p *rmessage;
  int ticket;
};

/* Contains timers for ensuring operation reliability */
struct coll_timers_s {
  timeout_p coll_timer;  /* timer on an entire operation */
  timeout_p *proc_timer; /* timer on data sent by individual processors */
};
  
/* Description of channel */
struct coll_channel_s {
  collection_p coll;
  int ticket;
  po_lock_p lock;
  mp_channel_p fch;
  int channel_number;
  coll_opdescr_p op;
  coll_buffers_p buffers;
  coll_timers_p timers;
};

int init_coll_channel(int moi, int gsize, int pdebug);
int finish_coll_channel(void);

coll_channel_p new_coll_channel(collection_p coll);
int free_coll_channel(coll_channel_p ch);

coll_channel_p channel_pointer(int ch);
int coll_operation(coll_channel_p coll, int caller, coll_operation_p opcode,
		   void *sbuffer, int ssize,
		   char *rbuffer, int rsize, reduction_function_p rf);
int coll_channel_wait(coll_channel_p ch);
int coll_channel_wait_proc(coll_channel_p ch, int procnum);
int process_children_data(int sender, coll_channel_p ch, message_p message);
int process_roots_data(int sender, coll_channel_p ch, message_p message);
#endif

