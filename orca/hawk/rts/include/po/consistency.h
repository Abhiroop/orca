#ifndef __consistency__
#define __consistency__

#include "sys_po.h"
#include "atomic_int.h"
#include "timeout.h"
#include "instance.h"
#include "pdg.h"
#include "message.h"

struct consistency_s
{
  instance_p instance;
  boolean color;                   /* Color of the current invocation. */
  condition_p locked[2];	   /* Lock on the state of the object for
				      a particular color. */
  atomic_int_p status[2];          /* Gives the status of the state of
				      the object for each color. */
  atomic_int_p *part_status[2];    /* Same as above, but for
				      individual partitions. The
				      status of a partition p is given
				      by part_status[color][i]. */
  timeout_p *timeouts[2];          /* Used for a sender-initiated
				      transfer strategy. */
};

int init_consistency_module(int me, int gsize, int pdebug, int *argc, char **argv);
int finish_consistency_module(void);

consistency_p new_consistency(instance_p instance);
int free_consistency(instance_p instance);

int invalidate_all(instance_p instance);

int create_gather_channel(instance_p instance);

int fetch_all(instance_p instance);
int access_all(instance_p instance);
int check_element(instance_p instance, int *element);
int wait_part(instance_p instance, int part_num);
int pdg_fetch(instance_p instance, po_opcode opcode);
int pdg_fetch_part(instance_p instance, po_opcode opcode, int source);
int pdg_send(instance_p instance, po_opcode opcode);
int pdg_wait_part(instance_p instance, po_opcode opcode, int part_num);
int pdg_wait_all(instance_p instance, po_opcode opcode);
int multicast_part(instance_p instance, int part_num);
int send_part(instance_p instance, int dest, int part_num, boolean color);
int pdg_send_part(instance_p instance, po_opcode opcode, int part);
int fetch_part(instance_p instance, int part_num);
boolean isconsistent(instance_p instance, int partnum);
#endif
