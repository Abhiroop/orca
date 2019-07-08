#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pan_sys.h"
#include "saw.h"

/*
 * Test stop and wait protocol. Timings are per message. With CHECK on,
 * no timing results are given.
 */

/*#define CHECK*/

static char *progname;
static int   nr;
static int   size;

				/* Synchronize with receiver */
static pan_mutex_p   mutex;
static pan_cond_p    cond;

				/* Measure time to send */
#ifndef CHECK 
static pan_time_p    start;
static pan_time_p    stop;
#endif

static void
usage(void)
{
    fprintf(stderr, "Usage: %s <nr messages> <message size>\n", progname);
    exit(1);
}
    

static void
sender(void)
{
    pan_msg_p msg;
    char *data;
    int i;

    msg  = pan_msg_create();
    data = (char *)pan_msg_push(msg, size, 1);
#ifdef CHECK
    for(i = 0; i < size; i++){
	data[i] = (char)(i & 0xff);
    }
#endif 

    printf("Sending messages\n");
    for(i = 0; i < nr; i++){
#ifndef CHECK
	pan_time_get(start);
#endif

	pan_saw_send(msg);

#ifndef CHECK
	pan_time_get(stop);
	pan_time_sub(stop, start);
	printf("Time to send: %g\n", pan_time_t2d(stop));
#endif
    }

    (void)pan_msg_pop(msg, size, 1);
}

static void
upcall(pan_msg_p msg)
{
#ifdef CHECK
    char *data;
    int i;
#endif

    printf("Message received\n");

    pan_mutex_lock(mutex);
    if (--nr == 0){
	pan_cond_signal(cond);
    }
    pan_mutex_unlock(mutex);

#ifdef CHECK
    data = (char *)pan_msg_pop(msg, size, 1);
    for(i = 0; i < size; i++){
	if (data[i] != (char)(i & 0xff)){
	    printf("Error: %d %d %d\n", i, data[i], i & 0xff);
	}
    }
#endif

    pan_msg_clear(msg);
}

int
main(int argc, char *argv[])
{
    /* Initialize system layer */
    pan_init(&argc, argv);

    progname = argv[0];

    if (argc != 3){
	usage();
    }

    nr   = atoi(argv[1]);
    size = atoi(argv[2]);

    /* Initialize saw module and register upcall */
    pan_saw_start();
    pan_saw_register(upcall);

    /* Initialize synchronization variables */
    mutex = pan_mutex_create();
    cond  = pan_cond_create(mutex);

    /* Start receive daemons */
    pan_start();

    if (pan_my_pid() == 0){
#ifndef CHECK
	start = pan_time_create();
	stop  = pan_time_create();
#endif
	sender();

	printf("Sender ready\n");
    }else{
	/* Wait for all messages to be received */
	pan_mutex_lock(mutex);
	pan_cond_wait(cond);
	pan_mutex_unlock(mutex);

	printf("Receiver ready\n");
    }

    /* End all modules */
    pan_saw_end();
    pan_end();

    exit(0);
}
