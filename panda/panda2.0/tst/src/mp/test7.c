#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Sender does a receive before the send, so the reply message is always
 * buffered. Asynchronous receiver. Sender sends entry point for receive
 * message.
 */


static int nrmsg = 1000;
static int rpc_map;
static pan_time_p sleep_time;

static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message, reply;
    int *p, i, entry, rec_entry;

    printf("sender\n");
    pan_sleep(sleep_time);

    message = pan_msg_create();
    reply   = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    p = pan_msg_push(message, sizeof(int), alignof(int));

    pan_time_get(start);

    for(i = 0; i < nrmsg; i++){
	rec_entry = pan_mp_receive_message(DIRECT_MAP, reply, MODE_SYNC2);
	*p = rec_entry;
	entry = pan_mp_send_message(1, rpc_map, message, MODE_SYNC2);

	pan_mp_finish_receive(rec_entry);
	pan_mp_finish_send(entry);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("Sender finished: %g\n", pan_time_t2d(stop));
}

static pan_mutex_p sync_lock;
static pan_cond_p sync_cond;
 
static void
do_receive(int map, pan_msg_p message)
{
    static int count = 0;
    pan_msg_p reply;
    int *rep_entry;

    reply = pan_msg_create();

    rep_entry = pan_msg_pop(message, sizeof(int), alignof(int));
    (void)pan_mp_send_message(0, *rep_entry, reply, MODE_ASYNC);

    if (count++ == nrmsg - 1){
	pan_cond_signal(sync_cond);
    }
}

static void
receiver(void)
{
    printf("receiver\n");
 
    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);
 
    pan_mp_register_async_receive(rpc_map, do_receive);
 
    pan_mutex_lock(sync_lock);
    pan_cond_wait(sync_cond);
    pan_mutex_unlock(sync_lock);
 
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

