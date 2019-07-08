#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grp_channel.h"
#include "message.h"
#include "configuration.h"
#include "assert.h"
#include "set.h"

static int me;
static int group_size;
static int proc_debug;
static message_p message;
static int received=0;

extern sleep();

void 
handle_message(message_p message) {
  int sender;
  char *s;
  
  received++;
  sender=*((int *)message_get_header(message));
  s=(char *)(message_get_data(message));
  if (sender==0)
    assert(memcmp(s,"string0",7)==0);
  else
    assert(memcmp(s,"string1",7)==0);
  printf("%d:\t Received message from %d\n", me, sender);
  if (received==group_size) {
    printf("%d:\t %s: test finished successfully\n", me, "test_grp_message");
    sleep(4);
    exit(0);
  }
}

int
main(int argc, char *argv[]) {
  grp_channel_p gch;
  set_p processors;
  int r;

  get_group_configuration(argc, argv, &me, &group_size, &proc_debug);
  assert(init_message(me,group_size,proc_debug)>=0);
  assert(init_grp_channel(me,group_size,proc_debug)>=0);
  processors=new_set(group_size);
  assert(processors!=NULL);
  assert(full_set(processors)>=0);
  r=message_register_handler(handle_message);
  assert(r>=0);
  gch=new_grp_channel(processors);

  message=new_message(2*sizeof(int),0);
  assert(message!=NULL);
  assert(message_set_handler(message,handle_message));
  assert(message_set_header(message,&me,sizeof(int))>=0);
  if (me==0)
    assert(message_set_data(message,"string0",8)>=0);
  else
    assert(message_set_data(message,"string1",8)>=0);
  grp_channel_send(gch,message);
  sleep(10);
  return 0;
}  
