#include <stdlib.h>
#include <string.h>

#include "mp_channel.h"
#include "sys_message.h"
#include "precondition.h"
#include "po_timer.h"
#include "misc.h"
#include "map.h"
#include "amoeba.h"
#include "module/rawflip.h"
#include "thread.h"
#include "assert.h"

#define MODULE_NAME "MP_CHANNEL"
#define MAX_MP_CHANNEL 256

typedef struct ep_info_s ep_info_t, *ep_info_p;

struct ep_info_s {
  int ep;
  int mep;
  adr_t private_address;
  adr_t *public_address;
};

struct mp_channel_s {
  int ch_number;
  set_p group;
  ep_info_p ep_info;     
  void **sender_to_message;
  int *message_size;  
  int *last_messid_received;   
  int *last_fragment_received; 
};

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;
static capability *c;      
static map_p ch_map;
static int ifno;
static ep_info_p generic_ep_info=NULL;

static int num_packets_discarded;
static int num_packets_notdelivered;
static boolean first=TRUE;

static void received(long ident, char *pkt, adr_p dstaddr,
		    adr_p srcaddr, f_msgcnt_t messid,
		    f_size_t offset, f_size_t length,
		    f_size_t total, int flag);
static void notdelivered(long ident, char *pkt,
			 adr_p dstaddr, f_msgcnt_t messid,
			 f_size_t offset, f_size_t length,
			 f_size_t total, int flag);
static ep_info_p create_addresses(set_p group, int ch_number);

/*=======================================================================*/
/* Initializes the module. Gets all flip addresses necessary to send
   and receive message. */
/*=======================================================================*/
int 
init_mp_channel(int moi, int gsize, int pdebug) {
  int r;

  if ((r=init_module())<=0) return r;
  c=(capability *)getcap("GROUPCAP");
  assert(c!=NULL);
  num_packets_discarded=0;
  num_packets_notdelivered=0;
  init_map(moi,gsize,pdebug);
  ch_map=new_map();
  assert(ch_map!=NULL);
  /* Initialize raw_flip */
  ifno=flip_init(1726,received,notdelivered);
  assert(ifno>=0);
  return 0;
}

/*=======================================================================*/
/* Finishes Flip. */
/*=======================================================================*/
int 
finish_mp_channel() {
  if (--initialized) return 0;
  flip_end(ifno);
  free_map(ch_map);
  finish_map();
  return 0;
}


/*=======================================================================*/
/* Creates a new channel on which only members specified in "group"
   can send and receive message. For each message received on this
   channel, the corresponding handler will be called by the rawflip
   upcall. All channels must be registered in the same order on all
   processors. */
/*=======================================================================*/
mp_channel_p 
new_mp_channel(set_p group) {

  int i, ch_number;
  mp_channel_p ch;

  precondition_p(num_items(ch_map)<MAX_MP_CHANNEL);

  ch=(mp_channel_p )malloc(sizeof(mp_channel_t));
  assert(ch!=NULL);
  /* This prevents messages to be processed before the channel is
     entirely initialized. */
  ch->ch_number=-1;
  ch_number=insert_item(ch_map,ch);
  if (ch_number<0) {
    free(ch);
    return NULL;
  }
  if (group==NULL)
    {
      /* The default for the processor set is the entire set of
         processors. */
      ch->group=new_set(group_size);
      full_set(ch->group);
    }
  else
    ch->group=duplicate_set(group);
  /* Only members of the group must register the addresses. */
  ch->ep_info=create_addresses(group, ch_number);
  if (is_member(ch->group,me)) {
    ch->last_fragment_received=(int *)malloc(group_size*sizeof(int));
    ch->last_messid_received=(int *)malloc(group_size*sizeof(int));
    ch->message_size=(int *)malloc(group_size*sizeof(int));
    for (i=0; i<group_size; i++)
      { ch->last_fragment_received[i]=-1;
      ch->last_messid_received[i]=-1;
      ch->message_size[i]=0;
      }
    ch->sender_to_message=(void **)malloc(group_size*sizeof(void *));
    for (i=0; i<group_size; i++)
      ch->sender_to_message[i]=NULL;
  }
  ch->ch_number=ch_number;
  return ch;
}
  

/*=======================================================================*/
/* Unregisters FLIP addresses corresponding to the channel and cleans
   up all data structures allocated for the channel. */
/*=======================================================================*/
int 
free_mp_channel(mp_channel_p ch) {
  int i;

  precondition(initialized);
  precondition(ch!=NULL);

  remove_item(ch_map,ch->ch_number);
  if (is_member(ch->group,me)) {
    if (ch->ep_info!=generic_ep_info) {
      free(ch->ep_info->public_address);
      flip_unregister(ifno,ch->ep_info->ep);
      flip_unregister(ifno,ch->ep_info->mep);
    }
    if (ch->sender_to_message!=NULL) {
      for (i=0; i<group_size; i++) 
	if (ch->sender_to_message[i]!=NULL) free(ch->sender_to_message[i]);
      free(ch->sender_to_message);
    }
    if (ch->message_size==NULL) free(ch->message_size);
    if (ch->last_fragment_received!=NULL) free(ch->last_fragment_received);
    if (ch->last_messid_received!=NULL) free(ch->last_messid_received);
  }
  free_set(ch->group);
  free(ch);
  return 0;
}
 
/*=======================================================================*/
/* Sends a message to a member of a channel. */
/*=======================================================================*/
int 
mp_channel_unicast(mp_channel_p ch, int receiver, message_p message) {
  int res;
  int count=0;
  int buffer_size;

  precondition(initialized);
  precondition(is_member(ch->group,me));
  precondition(is_member(ch->group,receiver));
  precondition(message!=NULL);
  precondition(message->message_header!=NULL);

  buffer_size=message->message_header->user_data_size+
    message->message_header->user_header_size+
      sizeof(message_header_t);
  while (((res=flip_unicast(ifno,message->message,
                            FLIP_SYNC,&(ch->ep_info->public_address[receiver]),
                            ch->ep_info->ep,buffer_size,10000))==FLIP_NOPACKET)
         &&(count<5))
    count ++;
  if (res<0) {
    fprintf(stderr,"%d:\t Error sending message to %d\n", me, receiver);
    fflush(stderr);
  }
  return res;
}
 
 
/*=======================================================================*/
/* Mulitcasts a message to all nodes. */
/*=======================================================================*/
int 
mp_channel_multicast(mp_channel_p ch, message_p message) {
  int res;
  int count=0;
  int buffer_size;
  int FLIP_FLAG = FLIP_SYNC | FLIP_SKIP_SRC;
 
  precondition(initialized);
  precondition(is_member(ch->group,me));
  precondition(message!=NULL);
  precondition(message->message_header!=NULL);

  buffer_size=message->message_header->user_data_size+
    message->message_header->user_header_size+
      sizeof(message_header_t);
  while (((res=flip_multicast(ifno,message->message,
                              FLIP_FLAG,&(ch->ep_info->public_address[group_size]),
                              ch->ep_info->ep,buffer_size,
			      ch->group->num_members,10000))==FLIP_NOPACKET)
         &&(count<5)) {
    count ++;
  }
  return res;
}

int
print_ch_number(mp_channel_p ch) {
  fprintf(stderr, "%d:\t finishing rts ch_num =%d\n", me, ch->ch_number);
  fflush(stderr);
  return 0;
}
 
/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/
 
/*=======================================================================*/
/* Assembles fragments and calls the appropriate handlers. */
/*=======================================================================*/
void 
received(long ident, char *pkt, adr_p dstaddr,
	 adr_p srcaddr, f_msgcnt_t messid,
	 f_size_t offset, f_size_t length,
	 f_size_t total, int flag)
{
  int sender;
  int ch_number;  
  mp_channel_p ch;
  long old;
  long max;
  int i;
  message_t message;
  message_handler_p handler;

  if (first) {
    first=FALSE;
    thread_enable_preemption();
    thread_get_max_priority(&max);
    thread_set_priority(max, &old); /* set priority to max */
  }
  ch_number=srcaddr->a_space;
  ch=get_item(ch_map,ch_number);
  if (ch==NULL) return;  /* Channel has not been initialized 
			    on this processor. */
  for (i=0; i<group_size; i++)
    if (memcmp((char *)(&(ch->ep_info->public_address[i])),
	       (char *)srcaddr,FLIP_ADR_SIZE)==0) break;
  if (i>=group_size) return;
  sender=i;
  if (sender==me) return; /* Don't process messages sent to myself. */
  if (ch->ch_number!=ch_number) return; /* Sender is not register yet
					   on this processor. */
  if (total==length) { 
    /* Message fits in one packet. */
    message_unmarshall(&message,pkt);
    handler=message_get_handler(&message);
    if (handler==NULL) {
      fprintf(stderr,"%d:\t NULL Message handler\n", me);
      return;
    }
    if ((*handler)(&message)) {
	  fprintf(stderr, "%d: handler returns 1, not supported by the FLIP communication layer\n", me);
    }
    return;
  }
  if (offset==0) { 
    /* first packet of a message */
    if (ch->message_size[sender]<total) {
      /* Increase local buffer size. */
      if (ch->sender_to_message[sender]!=NULL)
	free(ch->sender_to_message[sender]);
      ch->sender_to_message[sender]=(void *)malloc(total);
      ch->message_size[sender]=total;
    }
    /* Commented out this test. I am not sure that it is valid. (Ceriel).
    if (messid!=(ch->last_messid_received[sender]+1)) {
      int diff;
      diff = messid - ch->last_messid_received[sender];
      fprintf(stderr,"%d:\t %d message%c lost\n", me, diff, (diff==2 ? ' ' : 's'));
      fflush(stderr);
    }
    */
    ch->last_messid_received[sender]=messid;
  }
  else {
    /* Was any fragment lost? discard. */
    if ((messid!=ch->last_messid_received[sender]) || 
	(ch->last_fragment_received[sender]!=(offset))) {
      if ((messid!=-1)&&(ch->last_fragment_received[sender]!=-1)) {
	num_packets_discarded++;
	fprintf(stderr,"%d:\t Discarding fragment\n", me);
      }
      /* The first message is often lost when there is a large
	 number of processors. So, don't count that one in the
	 statistics. */
      return;
    }
  }
  memcpy(ch->sender_to_message[sender]+offset,pkt,length);
  /* Is message complete? Call appropriate handler. */
  if (total==(offset+length)) {
    message_unmarshall(&message,ch->sender_to_message[sender]);
    handler=message_get_handler(&message);
    if (handler==NULL) return;
    if ((*handler)(&message)) {
	  fprintf(stderr, "%d: handler returns 1, not supported by the FLIP communication layer\n", me);
    }
  }
  ch->last_fragment_received[sender]=offset+length;
  return;
}


/*=======================================================================*/
/* If a message fails to be sent, nothing is done. The higher-level
   protocol will recover from the failure anyway. */
/*=======================================================================*/
void 
notdelivered(long ident, char *pkt,
	     adr_p dstaddr, f_msgcnt_t messid,
	     f_size_t offset, f_size_t length,
	     f_size_t total, int flag) {

  num_packets_notdelivered++;
  fprintf(stderr,"%d:\t Not delivered\n", me);
  return;
}
 

/*=======================================================================*/
/* Returns all addresses (private and public) needed to send messages
   among the members of a given group of processors. */
/*=======================================================================*/
ep_info_p 
create_addresses(set_p group, int ch_number) {
  char grpkey[FLIP_ADR_SIZE-1];
  ep_info_p epinf;
  int i;
  
  /* Only one ep_info structure is used for all groups that include
     all processors. */
  if ((generic_ep_info!=NULL) && (group==NULL)) {
    return generic_ep_info;
  }
  if ((generic_ep_info!=NULL) && (group->num_members==group_size)) {
    return generic_ep_info;
  }
  /* Create a new group. */
  if (is_member(group,me))
    {
      epinf=(ep_info_p)malloc(sizeof(ep_info_t));
      assert(epinf!=NULL);

      /* EP address contains the group capability (with the channel
	 number as the last byte. The a_space is the process number. */
      memcpy(grpkey,c,PORTSIZE);
      memcpy(epinf->private_address.a_abytes,grpkey,min(PORTSIZE,FLIP_ADR_SIZE-1));
      epinf->private_address.a_abytes[FLIP_ADR_SIZE-2]=group_size;
      epinf->private_address.a_space = ch_number;
      epinf->mep = flip_register(ifno, &epinf->private_address); 
      assert(epinf->mep>=0);
      /* Unicast address has address space me. That's how the flip
         upcall knows who is the sender of a message. */
      epinf->private_address.a_abytes[FLIP_ADR_SIZE-2]=me;
      epinf->ep = flip_register(ifno, &epinf->private_address); 
      assert(epinf->ep>=0);
      
      epinf->public_address=(adr_t *)malloc(sizeof(adr_t)*(group_size+1));
      epinf->private_address.a_space = ch_number;
      for (i=0; i<=group_size; i++)
	if ((is_member(group,i))||(i==group_size))
	  { 
	    epinf->private_address.a_abytes[FLIP_ADR_SIZE-2]=i;
	    flip_oneway(&epinf->private_address,&(epinf->public_address[i]));
	  }
      if ((generic_ep_info==NULL) && (group->num_members==group_size))
	generic_ep_info=epinf;
      return epinf;
    }
  else
    return NULL;
}
