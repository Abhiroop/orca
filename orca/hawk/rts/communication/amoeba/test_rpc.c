#include <stdio.h>
#include <stdlib.h>
#include "rpc_channel.h"
#include "rts_init.h"

#define NUMI 10

rpc_channel_p rch;
message_p message;
message_p rmessage;
condition_p end_test;

static int group_size;
static int me;
static int proc_debug;

static void user_server_handler(message_p message);
static void user_client_handler(message_p message);

void 
user_server_handler(message_p message) {
  int *i;

  i=(int *)(message_get_header(message));
  printf("%d:\t received request %d\n", me, *i);
  message_set_header(rmessage,i,sizeof(int));
  message_set_handler(rmessage,user_client_handler);
  rpc_reply(rch,message,rmessage);
  if ((*i)==(NUMI-1)) signal_condition(end_test);
  return;
}

void 
user_client_handler(message_p message) {
  int *i;

  i=(int *)(message_get_header(message));
  printf("%d:\t received reply %d\n", me, *i);
  return;
}
  
int 
main(int argc, char *argv[]) {
  int i;

  me=atoi(argv[1]);
  group_size=atoi(argv[2]);
  if (argc>2) proc_debug=atoi(argv[3]);

  init_rpc_channel(me,group_size,proc_debug);
  
  rch=new_rpc_channel(NULL);
  message_register_handler(user_server_handler);
  message_register_handler(user_client_handler);
  rpc_register_handler(user_server_handler);
  rpc_register_handler(user_client_handler);
  end_test=new_condition();
  message=new_message(sizeof(int)*2,0);
  rmessage=new_message(sizeof(int)*2,0);
  if (me==0) {
    sleep(2);
    for (i=0; i<NUMI; i++) { 
      message_set_handler(message,user_server_handler);
      message_set_header(message,&i,sizeof(int));
      rpc_request(rch,1,message);
      printf("\n%d:\t Sent request %d\n", me, i);
      printf("%d:\t Waiting reply %d\n", me, i);
      rpc_wait_reply(rch);
    }
  }
  else {
    await_condition(end_test);
    await_condition(end_test);
    sleep(2);
  }
  return 0;
}


      
  
  
