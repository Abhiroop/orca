/* Test pingpong latency for the group protocol.
 * Parameters:
 * mpingpong <my_platform n_platforms> <switches> msgs msg_size
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"

extern void sys_print_comm_stats(void);

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

pan_group_p		global_group;	/* For easy access in the debugger */


static pan_mutex_p	rcve_lock;
static pan_cond_p	my_turn;
static pan_cond_p	done;
static int		whose_turn = 0;
static int		msg_count = 0;
static int		total_msgs;

static int		max_msg_size;
static int    		poll_on_sync = FALSE;


extern unsigned int sleep(unsigned int);



static int
str2int(const char int_str[])
{
    int res;

    if (sscanf(int_str, "%d", &res) != 1) {
	fprintf(stderr, "cannot convert to int: \"%s\"\n", int_str);
	abort();
    }
    return(res);
}


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
receive(pan_msg_p msg)
{
    hdr_p   hdr;

    hdr = hdr_look(msg);
    assert(max_msg_size == hdr->size);

    pan_mutex_lock(rcve_lock);
    if (whose_turn == -1) {
	whose_turn = me + 1;
    } else {
	++whose_turn;
    }
    if (whose_turn == n_platforms) {
	whose_turn = 0;
    }
    if (whose_turn == me) {
	pan_cond_signal(my_turn);
    }

    ++msg_count;
    if (msg_count == total_msgs) {
	pan_cond_signal(done);
    }

    if (hdr->sender != me) {
	pan_msg_clear(msg);
    }
    pan_mutex_unlock(rcve_lock);
}


static void
do_pingpong(pan_group_p g, int msgs)
{
    pan_msg_p  msg;
    char      *str;
    int        i;
    pan_time_p start = pan_time_create();
    pan_time_p now   = pan_time_create();

    msg = pan_msg_create();

    str = (char*)hdr_push(msg, max_msg_size, me);

    pan_time_get(start);

    for (i = 0; i < msgs; i++) {
	pan_mutex_lock(rcve_lock);
	while (whose_turn != me) {
#ifdef POLL_ON_WAIT
	    if (poll_on_sync) {
		pan_mutex_unlock(rcve_lock);
		pan_poll();
		pan_mutex_lock(rcve_lock);
	    } else {
		pan_cond_wait(my_turn);
	    }
#else
	    pan_cond_wait(my_turn);
#endif
	}
	whose_turn = -1;		/* Avoid send without sync */
	pan_mutex_unlock(rcve_lock);

	pan_group_send(g, msg);

    }

    pan_time_get(now);
    pan_time_sub(now, start);

    printf("%2d:      total time  : %f in %d ticks\n",
	    me, pan_time_t2d(now), msg_count);

    if (msg_count > 0) {
	pan_time_div(now, msg_count);
	printf("%2d:      per tick    : %f\n", me, pan_time_t2d(now));
    }

    pan_mutex_lock(rcve_lock);
    while (msg_count < total_msgs) {
#ifdef POLL_ON_WAIT
	if (poll_on_sync) {
	    pan_mutex_unlock(rcve_lock);
	    pan_poll();
	    pan_mutex_lock(rcve_lock);
	} else {
	    pan_cond_wait(my_turn);
	}
#else
	pan_cond_wait(done);
#endif
    }
    pan_mutex_unlock(rcve_lock);

    pan_msg_clear(msg);

    pan_time_clear(start);
    pan_time_clear(now);
}




static void
usage(int argc, char *argv[])
{
     printf("usage: %s <cpu> <ncpu> <nr_msg> <msg_size> [options..]\n",
	     argv[0]);
     printf("options:\n");
     printf("   -poll	: poll during pan_cond_wait\n");
}


int
main(int argc, char** argv)
{
    pan_group_p g;
    int         i;
    int         option;
    int	        msgs = 0;		/* Defy gcc warnings */

    if (argc < 5) {
	usage(argc, argv);
	exit(1);
    }

    pan_init(&argc, argv);

    me          = pan_my_pid();
    n_platforms = pan_nr_platforms();

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_sync = TRUE;
	} else {
	    if (option == 0) {
		msgs         = str2int(argv[i]);
	    } else if (option == 1) {
		max_msg_size = str2int(argv[i]);
	    } else {
		printf("No such option: %s\n", argv[i]);
		usage(argc, argv);
		exit(5);
	    }
	    ++option;
	}
    }

    if (max_msg_size < 0) {
	srand(me);
    }

    rcve_lock  = pan_mutex_create();
    done       = pan_cond_create(rcve_lock);
    my_turn    = pan_cond_create(rcve_lock);
    msg_count  = 0;
    total_msgs = msgs * n_platforms;

    /* pan_mp_init(); */
    pan_group_init();

    pan_start();

    printf("%2d: on line...\n", me);

    g = pan_group_join("group_1", receive);
    global_group = g;

    pan_group_await_size(g, n_platforms);

    do_pingpong(g, msgs);

    pan_group_leave(g);

    pan_mutex_clear(rcve_lock);
    pan_cond_clear(done);
    pan_cond_clear(my_turn);

    pan_group_clear(g);

    pan_group_end();
    /* pan_mp_end(); */

    pan_end();

    return(0);
}
