#include "message.h"
#include "configuration.h"
#include "assert.h"
#include <stdio.h>
#include "string.h"

static int me;
static int group_size;
static int proc_debug;
static message_p message;

void 
handle_message(message_p message) {
  return;
}

int
main(int argc, char *argv[]) {
  int h;
  char *s;

  get_group_configuration(argc, argv, &me, &group_size, &proc_debug);
  init_message(me,group_size,proc_debug);
  
  message=new_message(sizeof(int),0);
  assert(message!=NULL);
  printf("Testing message_set_data\n");
  assert(message_set_header(message,&group_size,sizeof(int))>=0);
  assert(message_set_data(message,"string",7)>=0);
  h=*((int *)(message_get_header(message)));
  assert(h==group_size);
  s=((char *)(message_get_data(message)));
  assert(memcmp(s,"string",7)==0);

  printf("Testing message_set_handler\n");
  assert(message_register_handler(handle_message)>=0);
  assert(message_set_handler(message,handle_message)>=0);
  assert(message_get_handler(message)==handle_message);

  printf("Testing message_append_data\n");
  assert(message_clear_data(message)>=0);
  assert(message_append_data(message,"long_",5)>=0);
  assert(message_append_data(message,"string",7)>=0);
  h=*((int *)(message_get_header(message)));
  assert(h==group_size);
  s=((char *)(message_get_data(message)));
  assert(memcmp(s,"long_string",12)==0);

  printf("%s: test finished successfully\n", argv[0]);
  return 0;
}
