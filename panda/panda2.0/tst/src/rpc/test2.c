#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_rpc.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
   Test rpc implementation with a large RPC request message.
*/

/* static variables */
static int            msg_size = 20000;
static pan_mutex_p    sync_lock;
static pan_cond_p     sync_cond;
static pan_time_p     stime;


/*
 * sender:
 *                 Send nr_msg empty messages
 */

static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message;
    pan_msg_p reply;

    printf("sender\n");

    pan_sleep(stime);

    message = pan_msg_create();
    start   = pan_time_create();
    stop    = pan_time_create();

    /* Synchronize with receiver before starting timer */
    pan_rpc_trans(0, message, &reply);
    pan_msg_clear(reply);

    (void)pan_msg_push(message, msg_size, 1);

    pan_time_get(start);

    pan_rpc_trans(0, message, &reply);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_msg_clear(reply);
    printf("Message sent: %g\n", pan_time_t2d(stop));
    
    pan_msg_clear(message);
}



/*
   do_receive:
                   Function called by the RPC module to handle a request
                   message. It sends a reply back. The reply messsage is
                   cleared by the RPC layer.
*/

static void
do_receive(pan_upcall_p upcall, pan_msg_p message)
{
    static int count = 0;
    pan_msg_p reply;

    pan_msg_clear(message);

    reply = pan_msg_create();
    pan_rpc_reply(upcall, reply);

    if (++count == 2){
	pan_cond_signal(sync_cond);
    }
}




/*
 * receiver:
 *                 Initializes the RPC layer. The upcall handler
 *                 (do_receive) is registered.
 */

static void
receiver(void)
{
    printf("receiver\n");

    pan_rpc_register(do_receive);

    pan_mutex_lock(sync_lock);
    pan_cond_wait(sync_cond);

    pan_mutex_unlock(sync_lock);

    printf("Receiver finished\n");

    pan_sleep(stime);
}


/*
 * main:
 *                 Startup and shutdown
 */

int
main(int argc, char *argv[])
{
    pan_init(&argc, argv);

    if (argc > 2){
	fprintf(stderr, "Usage: %s [msg_size]\n", argv[0]);
	exit(1);
    }

    if (argc == 2) msg_size = atoi(argv[1]);

    sync_lock = pan_mutex_create();
    sync_cond = pan_cond_create(sync_lock);

    pan_mp_init();
    pan_rpc_init();
    pan_util_init();

    stime = pan_time_create();
    pan_time_set(stime, 5L, 0UL);

    pan_start();

    if (pan_my_pid() != 0){
	sender();
    }else{
	receiver();
    }

    pan_util_end();
    pan_rpc_end();
    pan_mp_end();

    pan_time_clear(stime);

    pan_cond_clear(sync_cond);
    pan_mutex_clear(sync_lock);

    pan_end();

    return 0;
}
