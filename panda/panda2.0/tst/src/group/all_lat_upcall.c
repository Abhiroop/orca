/* Test program for the group protocol.
 *
 * Latecy measurement: latency averaged over all senders, over all receivers.
 *
 * all to all/2 pingpong fashion:
 * All hosts are in turn source. When I have become source, I send
 * (my_pid - 1) * turnsize group messages, and turn_size by turn_size each of
 * the the lower-numbered hosts pingpongs back.
 *
 * Version where all pongs and pings are done from the upcall.
 * The first ping is done from the main thread to start things off.
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


#define FIRST_PINGER	1
#define FIRST_PONGER	0
#define FIRST_CYCLE	0


static int              me;
static int              n_platforms;

static pan_msg_p	reply;
static pan_msg_p	msg;

static pan_mutex_p	rcve_lock;
static pan_cond_p       group_inited;

static pan_cond_p	done;
static int		msg_count;
static int              total_msgs;


static int		msgs;
static int		msg_size;

static pan_group_p      group;

static pan_time_p      *indiv_ppong;
static pan_time_p	t_indiv;


#ifdef VERBOSE
static int		verbose   = 0;
#endif


#ifdef STATISTICS

#define STATINC(cnt)	(++(cnt))

static int		send_ping = 0;
static int		send_pong = 0;
static int		rcve_ping = 0;
static int		rcve_pong = 0;

#else

#define STATINC(cnt)

#endif



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




static void
handle_ping(hdr_p hdr)
{
    static int ping_count = 0;
    static int ponger = FIRST_PONGER;

    ++ping_count;
							STATINC(rcve_ping);
    if (ping_count == msgs) {
						/* Find out the new ponger */
	ping_count = 0;
	++ponger;
	if (ponger == hdr->sender) {
	    ponger = 0;
	}
#ifdef VERBOSE
	if (verbose >= 20) {
	    printf("%2d:         New ponger is %d\n", me, ponger);
	}
#endif
    }

    if (ponger == me) {
					/* Must pong */
#ifdef VERBOSE
	if (verbose >= 40) {
	    printf("%2d:         send pong to ping %d\n",
		    me, hdr->sender);
	}
#endif
						    STATINC(send_pong);
	pan_group_send(group, reply);
    }
}



static void
handle_pong(void)
{
    static int pong_count = 0;
    static int pong_cycle = 0;
    static int n_cycles = FIRST_CYCLE;
    static int ponger = FIRST_PONGER;
    static int pinger = FIRST_PINGER;

    ++pong_count;
							STATINC(rcve_pong);
    if (pong_count == msgs) {
					/* pingpong cycle finished */

	if (pinger == me) {
	    pan_time_get(indiv_ppong[ponger]);
	    pan_time_sub(indiv_ppong[ponger], t_indiv);
	}
					/* Find out the new ponger */
	++ponger;
	if (ponger == me) {
	    ponger = 0;
	}

	if (pong_cycle == n_cycles) {
					/* Find out the new pinger */
	    ++n_cycles;
	    ++pinger;
#ifdef VERBOSE
	    if (verbose >= 20) {
		printf("%2d:         New pinger is %d\n", me, pinger);
	    }
#endif
	}

	++pong_cycle;
	pong_count = 0;

	if (pinger == me) {
	    pan_time_get(t_indiv);
	}
    }

    if (pinger == me) {
#ifdef VERBOSE
	if (verbose >= 40) {
	    printf("%2d:     ping %d, count %d\n", me, ponger, pong_count);
	}
#endif
						    STATINC(send_ping);
	pan_group_send(group, msg);
    }
}



static void
receive(pan_msg_p msg)
{
    hdr_p   hdr;

    hdr = hdr_look(msg);

#ifdef VERBOSE
    if (verbose >= 60) {
	printf("%2d: message %3d received: size %d\n",
		me, msg_count, hdr->size);
    }
#endif

    pan_mutex_lock(rcve_lock);
    while (group == NULL) {
	pan_cond_wait(group_inited);
    }

    msg_count++;
    pan_mutex_unlock(rcve_lock);
						/* Group upcall is the only
						 * thread that writes msg_count
						 * so may release the lock to
						 * read it */

    if ((msg_count & 0x1) == 1) {		/* Receive a ping */
	handle_ping(hdr);
    } else {					/* Receive a pong */
	handle_pong();
	if (msg_count == total_msgs) {
						/* Done */
#ifdef VERBOSE
	    if (verbose >= 10) {
		printf("%2d:     signal done (count = %d)\n",
			me, msg_count);
	    }
#endif
	    pan_mutex_lock(rcve_lock);
	    pan_cond_signal(done);
	    pan_mutex_unlock(rcve_lock);
	}
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
     printf("   -v <n>\t\t: verbose level\t\t(default 1)\n");
#endif
}


int
main(int argc, char** argv)
{
    int         i;
    pan_time_p  start;
    pan_time_p  now;
    int         option;

    pan_init(&argc, argv);

    start = pan_time_create();
    now   = pan_time_create();

    me = pan_my_pid();
    n_platforms  = pan_nr_platforms();

    option = 0;
    for (i = 1; i < argc; i++) {
	if (FALSE) {
#ifdef VERBOSE
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = atoi(argv[i]);
#endif
	} else {
	    if (option == 0) {
		msgs         = atoi(argv[i]);
		++option;
	    } else if (option == 1) {
		msg_size = atoi(argv[i]);
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
    group_inited = pan_cond_create(rcve_lock);
    done       = pan_cond_create(rcve_lock);
    msg_count  = 0;

    total_msgs = n_platforms * msgs * (n_platforms - 1);

    msg        = pan_msg_create();
    hdr_push(msg, msg_size, me);

    reply      = pan_msg_create();
    hdr_push(reply, msg_size, me);

    indiv_ppong = pan_malloc(n_platforms * sizeof(pan_time_p));
    for (i = 0; i < n_platforms; i++) {
	indiv_ppong[i] = pan_time_create();
	pan_time_d2t(indiv_ppong[i], 0.0);
    }
    t_indiv = pan_time_create();

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

    pan_mutex_lock(rcve_lock);
    pan_cond_signal(group_inited);
    pan_mutex_unlock(rcve_lock);


#ifdef VERBOSE
    if (verbose >= 10) {
	printf("%2d: msgs per exchange %d total msgs %d\n",
		me, msgs, total_msgs);
    }
#endif

    pan_time_get(start);

						/* Start the first ping */
    if (me == FIRST_PINGER) {
	pan_time_get(t_indiv);
	pan_group_send(group, msg);
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

#ifdef VERBOSE
    if (me == 0 || verbose >= 1) {
	printf("%2d:      total time  : %f in %d ticks\n",
	        me, pan_time_t2d(now), total_msgs);

        if (msg_count > 0) {
	    pan_time_div(now, total_msgs);
	    printf("%2d:      per tick    : %f\n", me, pan_time_t2d(now));
        }
    }
#endif

#ifdef STATISTICS
    printf("%2d: send %6s %6s rcve %6s %6s\n"
	   "%2d:      %6d %6d      %6d %6d\n",
	   me, "ping", "pong", "ping", "pong",
	   me, send_ping, send_pong, rcve_ping, rcve_pong);
#endif

    if (me > 0) {
	printf("%2d: %d msgs exchanged with : ", me, 2 * msgs);
	for (i = 0; i < me; i++) {
	    pan_time_div(indiv_ppong[i], 2 * msgs);
	    printf("%d: %f; ", i, pan_time_t2d(indiv_ppong[i]));
	}
	printf("\n");
    }

    for (i = 0; i < n_platforms; i++) {
	pan_time_clear(indiv_ppong[i]);
    }
    pan_free(indiv_ppong);
    pan_time_clear(t_indiv);

    pan_group_clear(group);

    pan_group_end();
    pan_mp_end();

    pan_cond_clear(done);
    pan_cond_clear(group_inited);
    pan_mutex_clear(rcve_lock);

    pan_time_clear(start);
    pan_time_clear(now);

    pan_end();

    return(0);
}
