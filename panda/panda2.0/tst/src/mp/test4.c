#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Sender and receiver do a receive before the send, so the reply
 * message is always buffered.
 */


static int nrmsg = 1000;
static int rpc_map;
static pan_time_p sleep_time;

static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message, reply;
    int i, entry, rec_entry;
    int *p, *q;

    printf("sender\n");
    pan_sleep(sleep_time);

    message = pan_msg_create();
    reply   = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    p = pan_msg_push(message, sizeof(int), alignof(int));

    pan_time_get(start);

    for(i = 0; i < nrmsg; i++){
	rec_entry = pan_mp_receive_message(rpc_map, reply, MODE_SYNC2);
	*p = i;
	entry = pan_mp_send_message(1, rpc_map, message, MODE_SYNC2);

	pan_mp_finish_receive(rec_entry);

	q = pan_msg_pop(reply, sizeof(int), alignof(int));
	if (*q != i){
	    printf("Message out of range: %d %d\n", *q, i);
	}
	
	pan_mp_finish_send(entry);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("Sender finished: %g\n", pan_time_t2d(stop));
}


static void
receiver(void)
{
    pan_msg_p message;
    pan_msg_p reply;
    int i, rec_entry;
    int *p, *q;

    printf("receiver\n");

    message = pan_msg_create();
    reply   = pan_msg_create();

    q = pan_msg_push(reply, sizeof(int), alignof(int));

    rec_entry = pan_mp_receive_message(rpc_map, message, MODE_SYNC2);
    for(i = 0; i < nrmsg; i++){
	pan_mp_finish_receive(rec_entry);
	
	p = pan_msg_pop(message, sizeof(int), alignof(int));
	if (*p != i){
	    printf("Message out of range: %d %d\n", *p, i);
	}	
	*q = i;

	rec_entry = pan_mp_receive_message(rpc_map, message, MODE_SYNC2);
	(void)pan_mp_send_message(0, rpc_map, reply, MODE_SYNC);
    }

    printf("Receiver finished\n");

}


int
main(int argc, char *argv[])
{
    pan_init(&argc, argv);

    if (argc > 2){
	fprintf(stderr, "Usage: %s [nr_msg]\n", argv[0]);
	exit(1);
    }

    if (argc == 2) nrmsg = atoi(argv[1]);

    pan_mp_init();
    pan_util_init();
    rpc_map = pan_mp_register_map();

    sleep_time = pan_time_create();
    pan_time_d2t(sleep_time, 5.0);
 
    pan_start();
    
    if (pan_my_pid() == 0){
	sender();
    }else{
	receiver();
    }

    pan_sleep(sleep_time);

    pan_util_end();
    pan_mp_end();
 
    pan_time_clear(sleep_time);
 
    pan_end();

    return 0;
}

