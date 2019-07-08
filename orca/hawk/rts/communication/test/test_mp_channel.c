#include "mp_channel.h"
#include "message.h"
#include "configuration.h"
#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "set.h"

static int me;
static int group_size;
static int proc_debug;
static message_p message;
static int num_messages=0;

extern sleep(); 

void 
handle_message(message_p message) {
  int sender;
  char *s;

  /* Sender is stored in header and is 0. */
  printf("%d:\t %s: received one message \n", me, "test_mp_message");
  sender=*((int *)message_get_header(message));
  assert(sender==0);
  s=(char *)(message_get_data(message));
  assert(memcmp(s,"string",7)==0);
  num_messages++;
  if (num_messages==2) {
    printf("%d:\t %s: test finished successfully\n", me, "test_mp_message");
    sleep(4);
    exit(0);
  }
}

int
main(int argc, char *argv[]) {
  mp_channel_p gch;
  set_p processors;

  get_group_configuration(argc, argv, &me, &group_size, &proc_debug);
  assert(init_message(me,group_size,proc_debug)>=0);
  assert(init_mp_channel(me,group_size,proc_debug)>=0);
  processors=new_set(group_size);
  assert(processors!=NULL);
  assert(full_set(processors)>=0);
  assert(message_register_handler(handle_message)>=0);
  printf("%d:\t Creating channel\n",me);
  gch=new_mp_channel(processors);

  if (me==0) {
    int i;
    message=new_message(2*sizeof(int),0);
    assert(message!=NULL);
    assert(message_set_handler(message,handle_message));
    assert(message_set_header(message,&me,sizeof(int))>=0);
    assert(message_set_data(message,"string",7)>=0);
    printf("%d:\t Sending unicasts\n", me); 
    for (i=0; i<group_size; i++) 
      if (i!=me)
	assert(mp_channel_unicast(gch,i,message)>=0);
    printf("%d:\t Sending multicast\n", me); 
    assert(mp_channel_multicast(gch,message)>=0);
  }
  printf("%d:\t Sleeping\n", me);
  sleep(20);
  printf("%d:\t Exiting program\n", me);
  return 0;
}  
