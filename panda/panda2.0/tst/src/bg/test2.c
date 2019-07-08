#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "pan_bg2.h"

/*
 * Usage: %s [nr_msg] [msg_size] [sender]
 *
 * Throughput and debugging. One integer + msg_size.
 */

#define CHECK

static int nr_msg   = 100;
static int msg_size = 20000;
static int send_pid = 0;

static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message;
    int i;
    int *p;
    char *q;

    printf("sender: %d number: %d size: %d\n", pan_my_pid(), nr_msg, msg_size);
    sleep(2);

    message = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    q = pan_msg_push(message, msg_size, 1);
#ifdef CHECK
    for(i = 0; i < msg_size; i++){
        q[i] = i % 64;
    }
#endif

    p = pan_msg_push(message, sizeof(int), alignof(int));

    /* First send synchronization message */
    *p = -1;
    pan_bg_send_message(message, BG_MODE_SYNC);

    pan_time_get(start);

    for(i = 0; i < nr_msg; i++){
	*p = i;
	pan_bg_send_message(message, BG_MODE_SYNC);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("Sender finished: %g\n", pan_time_t2d(stop));
}



static pan_mutex_p sync_lock;
static pan_cond_p  sync_cond;

static void
do_receive(pan_msg_p message)
{
    int *p;
#ifdef CHECK
    int count, i;
#endif
    char *q;

    p = pan_msg_pop(message, sizeof(int), alignof(int));
    q = pan_msg_pop(message, msg_size, 1);

#ifdef CHECK
    printf("Received: %d\n", *p);
 
    count = 0;
    for(i = 0; i < msg_size; i++){
        if (i % 64 != q[i]) count ++;
    }
    if (count) printf("Found %d errors in message\n", count);
    assert(!count);
#endif

    if (*p == nr_msg - 1 && pan_my_pid() != send_pid){
	pan_cond_signal(sync_cond);
    }

    pan_msg_clear(message);
}

static void
receiver(void)
{
    printf("receiver\n");

    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);

    pan_mutex_lock(sync_lock);
    pan_cond_wait(sync_cond);
    pan_mutex_unlock(sync_lock);

    printf("Receiver finished\n");
}


int
main(int argc, char *argv[])
{
    pan_init(&argc, argv);

    if (argc > 4){
	fprintf(stderr, "Usage: %s [nr_msg] [msg_size] [sender]\n", argv[0]);
	exit(1);
    }

    if (argc >= 2) nr_msg   = atoi(argv[1]);
    if (argc >= 3) msg_size = atoi(argv[2]);
    if (argc == 4) send_pid = atoi(argv[3]);

    pan_bg_init();
    pan_bg_register(do_receive);
    
    pan_start();

    if (pan_my_pid() == send_pid){
	sender();
    }else{
	receiver();
    }

    sleep(2);
    pan_bg_end();
    pan_end();

    return 0;
}

