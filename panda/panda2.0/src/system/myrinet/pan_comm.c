#include <amoeba.h>
#include <semaphore.h>

#include <limits.h>
#include <string.h>
#include <stderr.h>

#include "fm.h"			/* Fast Messages for Myrinet */

/* These prototypes have not made it to the public fm.h yet: */
extern void FM_print_lcp_mcgroup(int);
extern void FM_print_mc_credit(void);
extern void _await_intr(int, int);
extern void FM_enable_intr(void);
extern void FM_disable_intr(void);
extern void FM_set_parameter(int, ...);


#include "pan_sys_msg.h"		/* Provides a system interface */

#ifdef STATISTICS
#  include "pan_sync.h"		/* Use fast macros in system layer */
#endif
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_msg_hdr.ci"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_nsap.h"

#include "pan_mcast_global.h"

#include "pan_timer.h"

#include "pan_trace.h"


#define DAEMON_STACK_SIZE 65536     /* bytes */


trc_event_t	trc_start_upcall;
trc_event_t	trc_end_upcall;

#ifdef IDLE_THREAD
static pan_thread_p     network_daemon;
static int		network_poll = 1;
#endif


static pan_timer_p      ucast_rcve_timer;
static pan_timer_p      ucast_send_timer;
static pan_timer_p      ucast_round_timer;
static pan_timer_p      extract_timer;


#ifdef STATISTICS
pan_mutex_p pan_comm_statist_lock;

static int pan_n_ucast_rcve_data  = 0;
static int pan_n_ucast_rcve_small = 0;
static int pan_n_ucast_send_data  = 0;
static int pan_n_ucast_send_small = 0;

int pan_n_mcast_rcve_data   = 0;
int pan_n_mcast_rcve_small  = 0;
int pan_n_mcast_send_data   = 0;
int pan_n_mcast_send_small  = 0;
#endif

#ifdef KOEN
pan_time_p up_start, up_stop, up_total;
int nr_up;

pan_time_p raoul_down_start, raoul_down_stop, raoul_down_total;
int raoul_nr_down;

static pan_time_p start, stop, total;
static int nr_timings = 0;
static int valid = 0;
#endif


#ifdef IDLE_THREAD
static void
idle_loop(void *arg)
{
	trc_new_thread(0, "idle loop");
	while ( network_poll) {
		pan_timer_start(extract_timer);
		FM_extract();		/* poll network for incoming messages */
		pan_timer_stop(extract_timer);
		pan_thread_yield();
	}
	pan_thread_exit();
}
#endif


/**************/
/* Statistics */
/**************/

static void
sys_stat_init(void)
{
#ifdef KOEN
    up_start = pan_time_create();
    up_stop = pan_time_create();
    up_total = pan_time_create();
    pan_time_set(up_total, 0, 0);

    raoul_down_start = pan_time_create();
    raoul_down_stop = pan_time_create();
    raoul_down_total = pan_time_create();
    pan_time_set(raoul_down_total, 0, 0);

/*
    start = pan_time_create();
    stop = pan_time_create();
    total = pan_time_create();
    pan_time_set(total, 0, 0);
*/
#endif
#ifdef STATISTICS
    pan_comm_statist_lock = pan_mutex_create();
#endif
}

static void
sys_stat_end(void)
{
#ifdef KOEN
    if (raoul_nr_down>0)
    	printf( "%d: total downcall time %f in %d ticks, per request %f\n",
    	        pan_my_pid(), pan_time_t2d(raoul_down_total), raoul_nr_down,
	        pan_time_t2d(raoul_down_total)/raoul_nr_down);
    pan_time_clear(raoul_down_start);
    pan_time_clear(raoul_down_stop);
    pan_time_clear(raoul_down_total);

    if (nr_up > 0)
    	printf( "%d: total upcall time %f in %d ticks, per reply %f\n",
    	        pan_my_pid(), pan_time_t2d(up_total), nr_up,
	        pan_time_t2d(up_total)/nr_up);
    pan_time_clear(up_start);
    pan_time_clear(up_stop);
    pan_time_clear(up_total);

/*
    if (nr_timings>0)
    	printf( "%d: total response time %f in %d ticks, per message %f\n",
    	        pan_my_pid(), pan_time_t2d(total), nr_timings,
	        pan_time_t2d(total)/nr_timings);
    pan_time_clear(start);
    pan_time_clear(stop);
    pan_time_clear(total);
*/
#endif
#ifdef STATISTICS
    pan_mutex_clear(pan_comm_statist_lock);
#endif
}


int
pan_sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return (2 + 1 + 1 + 6 + 8 * (1 + 6) + 3) * 2;
#else
    return 0;
#endif
}

void
pan_sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
    sprintf(buf, "%2d: system %6s %6s %6s %6s %6s %6s %6s %6s\n",
	    pan_my_pid(),
	    "ucst/d", "ucst/s", "urcv/d", "urcv/s",
	    "mcst/d", "mcst/s", "mrcv/d", "mrcv/s");
    buf = strchr(buf, '\0');
    sprintf(buf, "%2d: system %6d %6d %6d %6d %6d %6d %6d %6d\n",
	    pan_my_pid(),
	    pan_n_ucast_send_data, pan_n_ucast_send_small,
	    pan_n_ucast_rcve_data, pan_n_ucast_rcve_small,
	    pan_n_mcast_send_data, pan_n_mcast_send_small,
	    pan_n_mcast_rcve_data, pan_n_mcast_rcve_small);
#endif
}

static void
sys_print_stats(void)
{
#ifdef STATISTICS
	char *buf;

	buf = pan_malloc( pan_sys_sprint_comm_stat_size());
	pan_sys_sprint_comm_stats( buf);
	printf( buf);
	pan_free(buf);
#endif
}


void
pan_sys_comm_start(void)
{
    trc_start_upcall = trc_new_event(2999, sizeof(pan_nsap_p), "start upcall",
				     "start upcall nsap 0x%p");
    trc_end_upcall   = trc_new_event(2999, sizeof(pan_nsap_p), "end upcall",
				     "end upcall nsap 0x%p");
    ucast_round_timer = pan_timer_create();
    ucast_send_timer  = pan_timer_create();
    ucast_rcve_timer  = pan_timer_create();
    extract_timer     = pan_timer_create();
}


void
pan_sys_comm_wakeup(void)
{
    sys_stat_init();
    FM_set_parameter(FM_QUIET_START);
    FM_initialize();
    pan_mcast_init();

#ifdef IDLE_THREAD
				/* Choose prio 0, which is less than
				 * minprio(). */
    network_daemon = pan_thread_create(idle_loop, (void *)0, 8 * 1024, 0, 0);
#endif

#ifndef FM_NO_INTERRUPTS
    pan_thread_start_interrupts();
#endif
}


void
pan_sys_comm_end(void)
{
#ifdef IDLE_THREAD
    network_poll = 0;
    pan_thread_join(network_daemon);
    pan_thread_clear(network_daemon);
#endif

    pan_mcast_end();

    sys_print_stats();
    sys_stat_end();
#ifndef FM_NO_INTERRUPTS
    if (pan_comm_verbose) FM_print_stats();
#endif

    pan_timer_print(ucast_round_timer, "ucast rnd trip");
    pan_timer_print(ucast_send_timer,  "ucast send");
    pan_timer_print(ucast_rcve_timer,  "ucast rcve");
    pan_timer_print(extract_timer,     "FM extract");

    pan_timer_clear(ucast_round_timer);
    pan_timer_clear(ucast_send_timer);
    pan_timer_clear(ucast_rcve_timer);
    pan_timer_clear(extract_timer);
}



				/* RECEIVE */


static void
comm_ucast_handler(struct FM_buffer *data, int size)
{
    pan_msg_p  msg;

    SYS_STATINC(pan_n_ucast_rcve_data);

    pan_timer_stop(ucast_round_timer);
    pan_timer_start(ucast_rcve_timer);

    msg = pan_msg_restore(data, size);
    assert(! (msg_nsap(msg)->type & PAN_NSAP_SMALL));

    msg_nsap(msg)->rec_msg(msg);

    pan_timer_stop(ucast_rcve_timer);
}



static void
comm_ucast_small_handler(struct FM_buffer *data, int size)
{
    pan_msg_p  msg;

    SYS_STATINC(pan_n_ucast_rcve_small);

    pan_timer_stop(ucast_round_timer);
    pan_timer_start(ucast_rcve_timer);

    msg = pan_msg_restore(data, size);

    assert(msg_nsap(msg)->type & PAN_NSAP_SMALL);
    assert(msg_offset(msg) == msg_nsap(msg)->data_len);

		/*
		 * Lifetime of data is lifetime of upcall.
		 */

    msg_nsap(msg)->rec_small(msg_data(msg));

    FM_free_buf(data);

    pan_timer_stop(ucast_rcve_timer);
}


				/* SEND */

pan_msg_p last_ucast_msg;

void
pan_comm_unicast_msg(int dest, pan_msg_p msg, pan_nsap_p nsap)
{
    int size;

    size = pan_sys_msg_nsap_store(msg, nsap);

    pan_timer_start(ucast_send_timer);
    pan_timer_start(ucast_round_timer);

    SYS_STATINC(pan_n_ucast_send_data);

last_ucast_msg = msg;
    FM_send_buf(dest, comm_ucast_handler, msg_data(msg), size);

    pan_timer_stop(ucast_send_timer);
}


void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
    char small[do_align(MAX_SMALL_SIZE, MAX_COMM_HDR_ALIGN) + NSAP_HDR_SIZE];
    int  size;

    SYS_STATINC(pan_n_ucast_send_small);

    memcpy(small, data, nsap->data_len);
    size = do_align(nsap->data_len, XCAST_HDR_ALIGN(nsap)) +
	   XCAST_HDR_SIZE(nsap);
    *(short *)(small + size) = pan_sys_nsap_index(nsap);
    size += NSAP_HDR_SIZE;
    FM_send_buf(dest, comm_ucast_small_handler, small, size);
}
