/* Test program for the group protocol.
 *
 * Latecy measurement: latency averaged over all senders, over all receivers.
 *
 * all to all pingpong fashion:
 * All hosts are in turn source. When I have become source, I send
 * n_platforms * turnsize group messages, and turn_size by turn_size each of
 * the others pingpongs back.
 *
 * Parameters:
 * a.out [my_platform n_platforms] <switches> turn_size msg_size
 *
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void srand(unsigned int seed);
extern int rand(void);

#include "pan_sys.h"

/*** for Parix:
#define printf pan_sys_printf

extern int pan_sys_printf(const char *, ...);
***/

#include "pan_mp.h"

#include "pan_group.h"


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


typedef struct HDR_T {
    short int sender;
    int       size;
} hdr_t, *hdr_p;


static int              me;
static int              n_platforms;

static pan_mutex_p	rcve_lock;

static pan_cond_p	rcve_one;
static int		msg_count;

static pan_cond_p	done;
static int              total_msgs;
static int              my_start;

#ifndef ALLOW_MCAST_FROM_UPCALL
static pan_cond_p	must_pong;
static int		my_pong;
#endif
static int		ponger;
static int		ping_count;

static int		msgs;
static int		max_msg_size;

static pan_group_p      group;

#ifdef VERBOSE
static int		verbose   = 0;
#endif

static pan_msg_p	reply;



static char *
hdr_push(pan_msg_p msg, int size, int sender)
{
    hdr_p hdr;
    char *data;

    size = ((size + alignof(hdr_t) - 1) / alignof(hdr_t)) * alignof(hdr_t);
    data = pan_msg_push(msg, sizeof(hdr_t) + size, alignof(hdr_t));
    hdr = (hdr_p)(data + size);
    hdr->size   = size;
    hdr->sender = sender;

    return data;
}


static hdr_p
hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(hdr_t), alignof(hdr_t));
}


static hdr_p
hdr_look(pan_msg_p msg)
{
    return pan_msg_look(msg, sizeof(hdr_t), alignof(hdr_t));
}



#ifndef ALLOW_MCAST_FROM_UPCALL

/*ARGSUSED*/
static
void pong_daemon(void *arg)
{
    int       pong_sent = 0;

    while (1) {
	pan_mutex_lock(rcve_lock);
	while (msg_count != total_msgs && pong_sent >= my_pong) {
	    pan_cond_wait(must_pong);
	}
	if (msg_count == total_msgs) {
	    break;
	}
	pan_mutex_unlock(rcve_lock);

#ifdef VERBOSE
	if (verbose >= 10) {
	    printf("%2d: pong %d\n", me, pong_sent);
	}
#endif

	pan_group_send(group, reply);

#ifdef VERBOSE
	if (verbose >= 10) {
	    printf("%2d: pong %d done\n", me, pong_sent);
	}
#endif

	++pong_sent;
    }

    pan_mutex_unlock(rcve_lock);

    pan_thread_exit();
}

#endif



static void
receive(pan_msg_p msg)
{
    hdr_p   hdr;

    hdr = hdr_look(msg);

    pan_mutex_lock(rcve_lock);

#ifdef VERBOSE
    if (verbose >= 20) {
	printf("%2d: message %3d received: size %d\n",
		me, msg_count, hdr->size);
    }
#endif

    msg_count++;

    if (msg_count & 0x1 == 1) {		/* Receive a ping */

	++ping_count;
	if (ping_count == msgs) {
	    ping_count = 0;
	    ++ponger;
	    if (ponger == n_platforms) {
		ponger = 0;
	    }
	}

	if (ponger == me) {
					/* Must pong */
#ifdef VERBOSE
	    if (verbose >= 10) {
		printf("%2d: will pong %d to ping %d\n",
			me, ping_count - 1, hdr->sender);
	    }
#endif
#ifndef ALLOW_MCAST_FROM_UPCALL
	    ++my_pong;
	    pan_cond_signal(must_pong);
	    pan_mutex_unlock(rcve_lock);
#else
	    pan_mutex_unlock(rcve_lock);
	    pan_group_send(group, reply);
#endif

	} else {
	    pan_mutex_unlock(rcve_lock);
	}
    } else {				/* Receive a pong */
	if (msg_count == my_start) {
					/* I am the sender or my turn */
#ifdef VERBOSE
	    if (verbose >= 10) {
		printf("%2d: signal to ping %d, count %d\n",
			me, me, my_start);
	    }
#endif

	    pan_cond_signal(rcve_one);
	}
	if (msg_count == total_msgs) {
					/* Done */
	    pan_cond_signal(done);
#ifndef ALLOW_MCAST_FROM_UPCALL
	    pan_cond_signal(must_pong);
#endif
	}
	pan_mutex_unlock(rcve_lock);
    }

    if (hdr->sender != me) {
	pan_msg_clear(msg);
    }
}




static void
usage(int argc, char *argv[])
{
     printf("usage: %s [<cpu> <ncpu>] <n_turn> <msg_size> [options..]\n",
	     argv[0]);
     printf("options:\n");
#ifdef VERBOSE
     printf("   -v <n>	: verbose level              (default 1)\n");
#endif
}


int
main(int argc, char** argv)
{
    int         i;
    int         pong;
    pan_time_p  start;
    pan_time_p  now;
    int         option;
    pan_msg_p   msg;
    pan_thread_p ponger;

    pan_init(&argc, argv);

    start = pan_time_create();
    now   = pan_time_create();

    me = pan_my_pid();
    n_platforms  = pan_nr_platforms();

    option = 0;
    for (i = 1; i < argc; i++) {
#ifdef VERBOSE
	if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
	} else
#endif
	{
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

#ifdef VERBOSE
    if (verbose >= 1) {
	printf("%2d: past pan_init\n", me);
    }
#endif

    srand(me);

    rcve_lock  = pan_mutex_create();
    rcve_one   = pan_cond_create(rcve_lock);
    done       = pan_cond_create(rcve_lock);
    msg_count  = 0;
#ifndef ALLOW_MCAST_FROM_UPCALL
    must_pong  = pan_cond_create(rcve_lock);
    my_pong    = 0;
#endif
    ponger     = 0;
    ping_count = 0;
    my_start   = 2 * me * msgs * n_platforms;
    total_msgs = 2 * n_platforms * msgs * n_platforms;

    msg        = pan_msg_create();
    hdr_push(msg, max_msg_size, me);

    reply      = pan_msg_create();
    hdr_push(reply, max_msg_size, me);

#ifndef ALLOW_MCAST_FROM_UPCALL
    ponger = pan_thread_create(pong_daemon, NULL, 0, pan_thread_maxprio(), 0);
#endif

    pan_mp_init();

    pan_group_init();

    pan_start();

#ifdef VERBOSE
    if (verbose >= 50) {
	printf("%2d: past pan_start\n", me);
    }
#endif

    group = pan_group_join("group_1", receive);

    pan_group_await_size(group, n_platforms);

#ifdef VERBOSE
    if (verbose >= 50) {
	printf("%2d: past pan_group_join\n", me);
    }
#endif

    pan_time_get(start);

					/* Await my turn */
    pan_mutex_lock(rcve_lock);
    while (msg_count < my_start) {
	pan_cond_wait(rcve_one);
    }
    pan_mutex_unlock(rcve_lock);

#ifdef VERBOSE
    if (verbose >= 5) {
	printf("%2d: my turn\n", me);
    }
#endif

    for (pong = 0; pong < n_platforms; pong++) {
	for (i = 0; i < msgs; i++) {

#ifdef VERBOSE
	    if (verbose >= 50) {
		printf("%2d: send ping %d; pong = %d\n",
			me, i + pong * msgs, pong);
	    }
#endif

	    pan_group_send(group, msg);

					/* Sync on receipt from pong */
	    pan_mutex_lock(rcve_lock);
	    my_start += 2;
	    while (msg_count != my_start) {
		pan_cond_wait(rcve_one);
	    }
	    pan_mutex_unlock(rcve_lock);
	}
    }

					/* Sync on receipt of all msgs */
    pan_mutex_lock(rcve_lock);
    while (msg_count != total_msgs) {
	pan_cond_wait(done);
    }
    pan_mutex_unlock(rcve_lock);

    pan_time_get(now);
    pan_time_sub(now, start);

    pan_group_leave(group);

#ifndef ALLOW_MCAST_FROM_UPCALL
    pan_thread_join(ponger);
#endif

    if ( me == 0
#ifdef VERBOSE
    || verbose >= 1
#endif
		) {
	printf("%2d:      total time  : %f in %d ticks\n",
	        me, pan_time_t2d(now), total_msgs);

        if (msg_count > 0) {
	    pan_time_div(now, total_msgs);
	    printf("%2d:      per tick    : %f\n", me, pan_time_t2d(now));
        }
    }

    pan_mutex_clear(rcve_lock);
    pan_cond_clear(rcve_one);
#ifndef ALLOW_MCAST_FROM_UPCALL
    pan_cond_clear(must_pong);
#endif

    pan_time_clear(start);
    pan_time_clear(now);

    pan_group_clear(group);

    pan_group_end();
    pan_mp_end();

    pan_end();

    return(0);
}
