#include <stdlib.h>
#include <unistd.h>

#include "grp_channel.h"
#include "precondition.h"
#include "sys_message.h"
#include "map.h"
#include "misc.h"
#include "amoeba.h"
#include "assert.h"
#include "group.h"
#include "thread.h"

#define MODULE_NAME "GRP_CHANNEL"
#define JOIN (group_size)
#define LEAVE (group_size+1)
#define EXPAND_BUFFER_SIZE (group_size+2)
#define MAXGROUPS 256
#define ALIVETIMEOUT 60000
#define REPLYTIMEOUT 6000
#define SYNCTIMEOUT 60000
#define STACKSIZE 64000

static  int me;
static  int group_size;
static  int proc_debug;
static  int initialized=0;

static  map_p gch_map;
static  capability *cap;

static  int create_grp(grp_channel_p gch);
static  int delete_grp_channel(grp_channel_p gch);
static  void grp_thread(char *c, int i);

struct grp_channel_s {
  int8 ch_number;
  int gd;
  set_p processors;
  port group;
  grpstate_p groupstate;
  void *buffer;
  int buffer_size;
};

/*=======================================================================*/
/* Initializes the module. */
/*=======================================================================*/ 
int 
init_grp_channel(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  cap=getcap("GROUPCAP");
  assert(cap!=NULL);
  gch_map=new_map();
  return 0;
}
  

/*=======================================================================*/
/* Finishes the module. Sends a group message that will force all grp
   threads to return. */
/*=======================================================================*/ 
int 
finish_grp_channel() { 

  if (--initialized) return 0;
  /* Free all channels created so far. */
/*
  if (me==0)
    forall(gch_map,gch) free_grp_channel((grp_channel_p )gch_map->items[gch]);
*/
  return 0;
}


/*=======================================================================*/
/* Creates a new grp_channel. It registers a receive function. All
   such functions must be register by all members in the same
   order. */
/*=======================================================================*/
grp_channel_p 
new_grp_channel(set_p processors) {
  grp_channel_p gch;
  int r;
  int i;

  precondition(initialized);

  gch=(grp_channel_p )malloc(sizeof(grp_channel_t));
  assert(gch!=NULL);
  gch->ch_number=insert_item(gch_map,(item_p)gch);
  assert(gch->ch_number>=0);
  gch->processors=duplicate_set(processors);
  assert(gch->processors!=NULL);
  gch->group=cap->cap_port;
  /* Modify capability in a deterministic manner so it need not be
     communicated to other processors. */
  for (i=0; i<(PORTSIZE-1); i++) 
    gch->group._portbytes[i]=gch->group._portbytes[i+1];
  gch->group._portbytes[PORTSIZE-2]=gch->group._portbytes[0];
  for (i=0; i<(PORTSIZE-2); i++) 
    gch->group._portbytes[i]=gch->group._portbytes[i+1];
  gch->group._portbytes[PORTSIZE-1]=gch->ch_number;
  gch->groupstate=(grpstate_t *)malloc(sizeof(grpstate_t));
  assert(gch->groupstate!=NULL);
  gch->buffer_size=1024;
  gch->buffer=(void *)malloc(gch->buffer_size);
  assert(gch->buffer!=NULL);
  /* If not member of the group, don't join. */
  if (!is_member(processors,me)) {
    return 0;
  }
  r=create_grp(gch);
  assert(r>=0);
  r=thread_newthread(grp_thread,STACKSIZE,(char *)gch, sizeof(grp_channel_p));
  assert(r>=0);
  return gch;
}

/*=======================================================================*/
/* Frees a grp channel. All channel must be freed in the same order
   by all members. */
/*=======================================================================*/
int 
free_grp_channel(grp_channel_p gch) {
  int r;
  header hdr;

  precondition(initialized);
  precondition(gch!=NULL);

  hdr.h_port=gch->group;
  hdr.h_command=LEAVE;
  r=grp_send(gch->gd,&hdr,"",0);
  return 0;
}

/*=======================================================================*/
/* Sends a message to all members of the group. Sets the h_extra field
   to the channel number (or receive function number). */
/*=======================================================================*/ 
int 
grp_channel_send(grp_channel_p gch, message_p message) { 
  int r;
  header hdr;
  int msg_size;
  void *buffer;

  precondition(initialized);
  precondition(gch!=NULL);

  hdr.h_port=gch->group;
  hdr.h_extra=gch->ch_number;
  msg_size = message->message_header->user_header_size +
    message->message_header->user_data_size+
      sizeof(message_t);
  buffer = message->message;
  if (msg_size > gch->buffer_size) {
    hdr.h_size=sizeof(int);
    hdr.h_command=EXPAND_BUFFER_SIZE;
    /* First send message to increase the receivers' buffer size. */
    r=grp_send(gch->gd,&hdr,(void *)(&msg_size), sizeof(int));
    assert(r>=0);
  }
  hdr.h_command=me;
  hdr.h_size=msg_size;
  r=grp_send(gch->gd,&hdr,buffer,msg_size);
  assert(r>=0);
  return 0;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

/*=======================================================================*/
/* Creates a new group given a capability, a set of processors and a
   maximum buffer size. Makes sure all members are alive before
   returning. */
/*=======================================================================*/
int 
create_grp(grp_channel_p gch) {
  int alive, r, bmm, num_trials;
  g_indx_t *memlist;
  header hdr;
  int *list;

  assert(gch!=NULL);
  assert(initialized);

  hdr.h_extra=me;
  hdr.h_port=gch->group;
  hdr.h_command=JOIN;
  num_trials=0;
  memlist=(g_indx_t *)
    malloc(sizeof(g_indx_t)*(gch->processors->max_members));
  assert(memlist!=NULL);
  list=list_of_members(gch->processors);
  /* list_of_members is a deterministic procedure so only one
     processor will create the group. */
  if (me==list[0]) { 
    gch->gd=grp_create(&(gch->group),0,max(2,gch->processors->max_members+1),10,16);
    if (gch->gd<0) fprintf(stderr,"%d: Error creating group: %d\n", me, gch->gd);
    assert(gch->gd>=0);
  }
  else { 
    while (((gch->gd=grp_join(&hdr))<0) && (num_trials<5)) { 
      num_trials++;
    }
    if (gch->gd<0) fprintf(stderr,"%d: Error joining group: %d\n", me, gch->gd);
    assert(gch->gd>=0);    
  }
  grp_set(gch->gd,&gch->group,SYNCTIMEOUT,REPLYTIMEOUT,ALIVETIMEOUT,2800);
  grp_info(gch->gd,&gch->group,gch->groupstate,memlist,
	   gch->processors->max_members);
  alive=gch->groupstate->st_total;
  while (alive<gch->processors->num_members) { 
    r=grp_receive(gch->gd,&hdr,0,0,&bmm);
    assert(r>=0);
    grp_info(gch->gd,&gch->group,gch->groupstate,memlist,
	     gch->processors->max_members);
    alive=gch->groupstate->st_total;
  }
  free(memlist);
  free(list);
  return 0;
}


/*=======================================================================*/
/* Frees all buffers allocated for a particular channel. */
/*=======================================================================*/
int 
delete_grp_channel(grp_channel_p gch) {

  assert(gch!=NULL);
  assert(initialized);

  remove_item(gch_map,gch->ch_number);
  free_set(gch->processors);
  free(gch->groupstate);
  if (gch->buffer_size>0) free(gch->buffer);
  return 0;
}
  
/*=======================================================================*/
/* Thread listening to group message. Keeps receiving messages. Uses
   the h_extra field to figure out which channel the message was
   received. */
/*=======================================================================*/
void 
grp_thread(char *c, int i)
{
  grp_channel_p gch;
  int bmm;
  int r;
  header hdr;
  message_handler_p handler;
  message_t message;

  assert(initialized);
  gch=(grp_channel_p )c;
  assert(gch!=NULL);
  hdr.h_port=gch->group;
  for (;;)
    { /* Block until message arrives. */
      r = grp_receive(gch->gd,&hdr,gch->buffer,gch->buffer_size,&bmm);
      if (r<0) continue;
      if (hdr.h_command==JOIN) continue;
      /* If leave message, just return. */
      if (hdr.h_command==LEAVE) { 
	if (gch!=NULL) {
	  delete_grp_channel(gch);
	  gch=NULL;
	}
	return;
      }
      /* If message is to expand the buffer size. */
      if (hdr.h_command==EXPAND_BUFFER_SIZE) {
	gch->buffer_size=*((int *)gch->buffer);
	gch->buffer=(void *)realloc(gch->buffer,gch->buffer_size);
	assert(gch->buffer!=NULL);
      }
      else {
	/* Call handler specified in message. */
	message_unmarshall(&message,gch->buffer);
	handler=message_get_handler(&message);
	assert(handler!=NULL) ;
	if ((*(handler))(&message)) {
	  fprintf(stderr, "%d: handler returns 1, not supported by the FLIP communication layer\n", me);
	}
      }
    }
}
