#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pan_sys.h"
#include "slid.h"

static char *progname;
static int   nr;
static int   size;

				/* Synchronize with receiver */
static pan_mutex_p   mutex;
static pan_cond_p    cond;

				/* Measure time to send */
static pan_time_p    start;
static pan_time_p    stop;


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

    printf("Sending messages\n");
    for(i = 0; i < nr; i++){
	pan_time_get(start);

	pan_saw_send(msg);

	pan_time_get(stop);
	pan_time_sub(stop, start);
	printf("Time to send: %g\n", pan_time_t2d(stop));
    }

    pan_msg_clear(msg);
}

static void
upcall(pan_msg_p msg)
{
    printf("Message received\n");

    pan_mutex_lock(mutex);
    if (--nr == 0){
	pan_cond_signal(cond);
    }
    pan_mutex_unlock(mutex);

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
	start = pan_time_create();
	stop  = pan_time_create();

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
