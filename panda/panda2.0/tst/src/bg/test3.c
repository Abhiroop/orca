#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "pan_bg2.h"

/*
 * Usage: %s [nr_msg] [nr_threads]
 *
 * Latency test with multiple threads. One integer.
 */

#define MAX_THREADS 50
#define MAX_CPU     100

static int nr_msg     = 1000;
static int nr_threads = 3;

static int recv[MAX_CPU][MAX_THREADS];

typedef struct{
    int  seqno;
    int  pid;
    int  thrno;
}header_t, *header_p;

static void
sender(void *arg)
{
    pan_time_p start, stop;
    pan_msg_p message;
    header_p hdr;
    int i;

    printf("sender: %d number: %d\n", pan_my_pid(), nr_msg);
    sleep(2);

    message = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    hdr = pan_msg_push(message, sizeof(header_t), alignof(header_t));

    /* First send synchronization message */
    hdr->seqno = 0;
    hdr->pid = pan_my_pid();
    hdr->thrno = (int)arg;
    pan_bg_send_message(message, BG_MODE_SYNC);

    pan_time_get(start);

    for(i = 0; i < nr_msg; i++){
	hdr->seqno = i + 1;
	pan_bg_send_message(message, BG_MODE_SYNC);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("Sender finished: %g\n", pan_time_t2d(stop));

    pan_thread_exit();
}



static pan_mutex_p sync_lock;
static pan_cond_p  sync_cond;

static void
do_receive(pan_msg_p message)
{
    static int count;
    header_p hdr;

    hdr = pan_msg_pop(message, sizeof(header_t), alignof(header_t));

    if (recv[hdr->pid][hdr->thrno] != hdr->seqno){
	fprintf(stderr, "Message out of order: pid %d thread %d : %d %d\n", 
		hdr->pid, hdr->thrno, recv[hdr->pid][hdr->thrno], hdr->seqno);
    }
    
    recv[hdr->pid][hdr->thrno]++;

    count++;

    if (count == (nr_msg + 1) * nr_threads * pan_nr_platforms()){
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
    int i;

    pan_init(&argc, argv);

    if (argc > 3){
	fprintf(stderr, "Usage: %s [nr_msg] [nr_threads]\n", argv[0]);
	exit(1);
    }

    if (argc >= 2) nr_msg = atoi(argv[1]);
    if (argc >= 3) nr_threads = atoi(argv[2]);

    assert(nr_threads <= MAX_THREADS);
    assert(pan_nr_platforms() <= MAX_CPU);

    pan_bg_init();
    pan_bg_register(do_receive);
    
    pan_start();

    for(i = 0; i < nr_threads; i++){
	pan_thread_create(sender, (void *)i, 0L, 0, 1);
    }

    receiver();

    pan_bg_end();
    pan_end();

    return 0;
}

