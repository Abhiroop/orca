#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "pan_sys.h"
#include "lat.h"

static char *progname;
static int   nr, nr_msgs;
static int   tests;
				/* Synchronize with receiver */
static pan_mutex_p   mutex;
static pan_cond_p    cond;

				/* Measure time to send */
static pan_time_p    start;
static pan_time_p    stop;


static void
usage(void)
{
    fprintf(stderr, "Usage: %s <nr messages> <nr tests>\n", progname);
    exit(1);
}
    

static void
sender(void)
{
    header_t header;
    int i;

    header.data = 3465;

    pan_time_get(start);

    for(i = 0; i < nr; i++){
	pan_small_send(&header);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);
    printf("Time to send: %g\n", pan_time_t2d(stop));
}

static void
upcall(header_p header)
{
    assert(header->data == 3465);

    if (--nr == 0){
	/* Should be reasonably save */
	pan_mutex_lock(mutex);
	pan_cond_signal(cond);
	pan_mutex_unlock(mutex);
	nr = nr_msgs;
    }
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

    nr = nr_msgs = atoi(argv[1]);
    tests        = atoi(argv[2]);

    /* Initialize small module and register upcall */
    pan_small_start();
    pan_small_register(upcall);

    /* Initialize synchronization variables */
    mutex = pan_mutex_create();
    cond  = pan_cond_create(mutex);

    /* Start receive daemons */
    pan_start();

    start = pan_time_create();
    stop  = pan_time_create();

    while(tests--){
	if (pan_my_pid() == 0){
	    sender();
	}else{
	    /* Wait for all messages to be received */
	    pan_mutex_lock(mutex);
	    pan_cond_wait(cond);
	    pan_mutex_unlock(mutex);
	    printf("Received\n");
	}
    }

    /* End all modules */
    pan_small_end();
    pan_end();

    exit(0);
}
