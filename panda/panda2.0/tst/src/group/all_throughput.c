/* Test program for the group protocol.
 *
 * Throughput measurement.
 *
 * All hosts are in turn source. When I have become source, I send a
 * group message, wait for the message toi be recieved and send the next msg.
 *
 * Parameters:
 * a.out [my_platform n_platforms] nr_msgs msg_size
 *
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"

/*** for Parix:
#define printf pan_sys_printf

extern int pan_sys_printf(const char *, ...);
***/

#include "pan_mp.h"

#include "pan_group.h"

#include "pan_util.h"


static int              me;
static int              n_platforms;

static pan_mutex_p	rcve_lock;

static pan_cond_p	go_arrived;
static int		go_msg_arrived = 0;

static pan_cond_p	rcve_one;
static int		msg_count;
static int              total_msgs;
static int              my_start;

static int		msgs;
static int		max_msg_size;

static pan_group_p      group;

static int		verbose   = 0;




static void
receive(pan_msg_p msg)
{
    char   *str = NULL;
    int     size;
    int     sender;

    tm_pop_int(msg, &sender);

    tm_pop_int(msg, &size);

    if (size != 0) {
	str = pan_msg_look(msg, size, alignof(char));
    }

    if (! go_msg_arrived) {
	assert(strcmp(str, "GO!") == 0);

	pan_mutex_lock(rcve_lock);
	go_msg_arrived = 1;
	pan_cond_signal(go_arrived);
	pan_mutex_unlock(rcve_lock);

	pan_msg_clear(msg);
	return;
    }

    pan_mutex_lock(rcve_lock);
    msg_count++;

    if (verbose >= 20) {
	if (size == 0) {
	    printf("%2d: message %3d received:  <empty>\n", me, msg_count);
	} else {
	    printf("%2d: message %3d received:  \"%s\"\n", me, msg_count,
		    str);
	}
    }

    if (msg_count == my_start) {
					/* I am the sender or my turn */
	pan_cond_signal(rcve_one);
    }
    if (msg_count == total_msgs) {
					/* Done */
	pan_cond_signal(rcve_one);
    }

    pan_mutex_unlock(rcve_lock);
    pan_msg_clear(msg);
}




static void
handle_go_msg(pan_group_p group, int go_sender)
{
    pan_msg_p  msg;
    char      *str;

					/* Await join of all members */
    if (me == go_sender) {
	pan_group_await_size(group, n_platforms);
	msg = pan_msg_create();
	str = (char *)pan_msg_push(msg, strlen("GO!") + 1, alignof(char));
	sprintf(str, "GO!");
	tm_push_int(msg, strlen("GO!") + 1);
	tm_push_int(msg, me);

	pan_group_send(group, msg);
    }

    pan_mutex_lock(rcve_lock);
    while (! go_msg_arrived) {
	pan_cond_wait(go_arrived);
    }
    pan_mutex_unlock(rcve_lock);

}


static void
usage(int argc, char *argv[])
{
     printf("usage: %s [<cpu> <ncpu>] <n_turn> <msg_size> [options..]\n",
	     argv[0]);
     printf("options:\n");
     printf("   -v <n>	: verbose level              (default 1)\n");
}


int
main(int argc, char** argv)
{
    int         i;
    pan_time_p  start = pan_time_create();
    pan_time_p  now   = pan_time_create();
    int         option;
    pan_msg_p   msg;

    pan_init(&argc, argv);

    pan_util_init();

    me = pan_my_pid();
    n_platforms  = pan_nr_platforms();

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
	} else {
	    if (option == 0) {
		msgs         = atoi(argv[i]);
		++option;
	    } else if (option == 1) {
		max_msg_size = atoi(argv[i]);
		++option;
	    } else {
		printf("No such option: %s\n", argv[i]);
		usage(argc, argv);
		return 5;
	    }
	}
    }

    if (option != 2) {
	printf("Not enough arguments\n");
	usage(argc, argv);
	return 5;
    }

    if (verbose >= 1) {
	printf("%2d: past pan_init\n", me);
    }

    rcve_lock    = pan_mutex_create();
    rcve_one     = pan_cond_create(rcve_lock);
    go_arrived   = pan_cond_create(rcve_lock);
    msg_count    = 0;
    my_start     = (me-1) * msgs;
    total_msgs   = (n_platforms - 1) * msgs;

    pan_mp_init();

    pan_group_init();

    pan_start();

    if (verbose >= 50) {
	printf("past pan_start\n");
    }

    group = pan_group_join("group_1", receive);

    if (verbose >= 50) {
	printf("past pan_group_join\n");
    }

    handle_go_msg(group, 0);

					/* Await my turn */
    if (me > 0) {	/* exclude sequencer because of flow control problems */
        pan_mutex_lock(rcve_lock);
        while (msg_count < my_start) {
	    pan_cond_wait(rcve_one);
        }
        pan_mutex_unlock(rcve_lock);

        if (verbose >= 5) {
	    printf("%2d: my turn\n", me);
        }

    	pan_time_get(start);
	for (i = 0; i < msgs; i++) {
	    msg = pan_msg_create();
	    tm_push_int(msg, max_msg_size);
	    tm_push_int(msg, me);
	    pan_msg_push(msg, max_msg_size, alignof(char));

	    pan_group_send(group, msg);

	    pan_mutex_lock(rcve_lock);
	    my_start += 1;
	    while (msg_count != my_start) {
		pan_cond_wait(rcve_one);
	    }
	    pan_mutex_unlock(rcve_lock);
	}
    	pan_time_get(now);
    	pan_time_sub(now, start);
	printf("group throughput from member %d: %g\n",
	       me, (msgs*max_msg_size) / pan_time_t2d(now));
    }

					/* Sync on receipt of all msgs */
    pan_mutex_lock(rcve_lock);
    while (msg_count != total_msgs) {
	pan_cond_wait(rcve_one);
    }
    pan_mutex_unlock(rcve_lock);

    pan_group_leave(group);

    pan_mutex_clear(rcve_lock);
    pan_cond_clear(rcve_one);
    pan_cond_clear(go_arrived);

    pan_time_clear(start);
    pan_time_clear(now);

    pan_group_clear(group);

    pan_group_end();
    pan_mp_end();
    pan_util_end();

    pan_end();

    return(0);
}
