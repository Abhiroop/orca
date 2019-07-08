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
extern int FM_pending(void);


#include "pan_sys.h"		/* Provides a system interface */

#ifdef STATISTICS
#  include "pan_sync.h"		/* Use fast macros in system layer */
#endif
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_fragment_hdr.ci"
#include "pan_buffer.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_nsap.h"

#include "pan_timer.h"

#include "pan_trace.h"


#define DAEMON_STACK_SIZE 65536     /* bytes */


trc_event_t	trc_start_upcall;
trc_event_t	trc_end_upcall;


#ifdef IDLE_THREAD
static pan_thread_p     network_daemon;
static int		network_poll = 1;
#endif



#ifdef STATISTICS
pan_mutex_p pan_comm_statist_lock;

#define STATINC(counter) \
	do { \
	    pan_mutex_lock(pan_comm_statist_lock); \
	    ++(counter); \
	    pan_mutex_unlock(pan_comm_statist_lock); \
	} while (0)

static int n_ucast_frag_rcve	= 0;
static int n_ucast_small_rcve	= 0;
static int n_ucast_frag_send	= 0;
static int n_ucast_small_send	= 0;

static int n_mcast_frag_rcve	= 0;
static int n_mcast_small_rcve	= 0;
static int n_mcast_frag_send	= 0;
static int n_mcast_small_send	= 0;

#ifdef IDLE_THREAD
static int n_idle_polls		= 0;
#endif
#else

#define STATINC(counter)

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


#ifdef FM_NO_INTERRUPTS
void
pan_poll(void)
{
    FM_extract();
}
#endif


#ifdef IDLE_THREAD
static void
idle_loop(void *arg)
{
	trc_new_thread(0, "idle loop");
	while ( network_poll) {
#ifdef STATISTICS
		++n_idle_polls;
#endif
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
    return (2 + 1 + 1 + 6 + 10 * (1 + 6) + 3) * 2;
#else
    return 0;
#endif
}

void
pan_sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
    sprintf(buf, "%2d: sys %6s %6s %6s %6s %6s %6s %6s %6s\n",
	    pan_my_pid(),
	    "ucst/f", "urcv/f", "ucst/s", "urcv/s",
	    "mcst/f", "mrcv/f", "mcst/s", "mrcv/s");
    buf = strchr(buf, '\0');
    sprintf(buf, "%2d:     %6d %6d %6d %6d %6d %6d %6d %6d\n",
	    pan_my_pid(),
	    n_ucast_frag_send, n_ucast_frag_rcve,
	    n_ucast_small_send, n_ucast_small_rcve,
	    n_mcast_frag_send, n_mcast_frag_rcve,
	    n_mcast_small_send, n_mcast_small_rcve);
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
}


void
pan_sys_comm_wakeup(void)
{
    sys_stat_init();
#ifdef NDEBUG
    FM_set_parameter(FM_QUIET_START);
#endif
    FM_initialize();

#ifdef IDLE_THREAD
    network_daemon = pan_thread_create(idle_loop, (void *)0, 0, 0, 0);
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

    sys_print_stats();
    sys_stat_end();
}



				/* Unicast receive */


static void
comm_ucast_handler(struct FM_buffer *data, int size)
{
    pan_nsap_p     nsap;
    struct pan_fragment frag;

#ifdef NEVER
printf("%2d: receive %d bytes\n", pan_my_pid(), size);
#endif

    pan_fragment_restore(&frag, data, size);
    nsap = fragment_nsap(&frag);

    trc_event(trc_start_upcall, &nsap);

    if (nsap->type & PAN_NSAP_SMALL) {
	STATINC(n_ucast_small_rcve);

	nsap->rec_small(fragment_data(&frag));
	FM_free_buf(data);
    } else {
	STATINC(n_ucast_frag_rcve);

	nsap->rec_frag(&frag);

	if (fragment_buffer(&frag) != NULL) {
	    assert((struct FM_buffer *)fragment_buffer(&frag) == data);
	    FM_free_buf(buffer2fm(fragment_buffer(&frag)));
	}
    }

    trc_event(trc_end_upcall, &nsap);
}



				/* Unicast send */


void
pan_comm_unicast_fragment(int dest, pan_fragment_p frag)
{
    int size;
    struct FM_buffer *fm_frag;

    STATINC(n_ucast_frag_send);

    size = pan_sys_fragment_nsap_store(frag);

#ifdef NEVER
printf("%2d: send %d bytes to %d\n", pan_my_pid(), size, dest);
#endif

    if (dest == pan_my_pid()) {
	fm_frag = FM_alloc_buf(size);
	memcpy(fm_frag->fm_buf, fragment_data(frag), size);
	comm_ucast_handler(fm_frag, size);
    } else {
	FM_send_buf(dest, comm_ucast_handler, fragment_data(frag), size);
    }
}


void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
    char small[do_align(MAX_SMALL_SIZE, MAX_COMM_HDR_ALIGN) + NSAP_HDR_SIZE];
    int  size;
    struct FM_buffer *fm_small;

    STATINC(n_ucast_small_send);

    memcpy(small, data, nsap->data_len);
    size = do_align(nsap->data_len, COMM_HDR_ALIGN(nsap)) +
	   XCAST_HDR_SIZE(nsap);
    *(short *)(small + size) = pan_sys_nsap_index(nsap);
    size += NSAP_HDR_SIZE;

#ifdef NEVER
printf("%2d: send small %d bytes to %d\n", pan_my_pid(), size, dest);
#endif

    if (dest == pan_my_pid()) {
	fm_small = FM_alloc_buf(size);
	memcpy(fm_small->fm_buf, small, size);
	comm_ucast_handler(fm_small, size);
    } else {
	FM_send_buf(dest, comm_ucast_handler, small, size);
    }
}




			/*---- Multicast receive ----*/


static void
pan_mcast_handler(struct FM_buffer *data, int size)
{
    pan_nsap_p     nsap;
    struct pan_fragment frag;

    pan_fragment_restore(&frag, data, size);
    nsap = fragment_nsap(&frag);

    trc_event(trc_start_upcall, &nsap);

    if (nsap->type & PAN_NSAP_SMALL) {
	STATINC(n_mcast_small_rcve);

	nsap->rec_small(fragment_data(&frag));
	FM_free_buf(data);
    } else {
	STATINC(n_mcast_frag_rcve);

	nsap->rec_frag(&frag);

	if (fragment_buffer(&frag) != NULL) {
	    assert((struct FM_buffer *)fragment_buffer(&frag) == data);
	    FM_free_buf(buffer2fm(fragment_buffer(&frag)));
	}
    }

    trc_event(trc_end_upcall, &nsap);
}





			/*---- Multicast send ----*/


void
pan_comm_multicast_fragment(pan_pset_p rcvr, pan_fragment_p frag)
{
    int             size;
#ifndef BCAST_SKIP_HOME
    struct FM_buffer *fm_frag;
#endif

    STATINC(n_mcast_frag_send);

    size = pan_sys_fragment_nsap_store(frag);

    if (pan_nr_platforms() > 1) {
#ifdef NEVER
printf("%2d: broadcast %d bytes\n", pan_my_pid(), size);
#endif
	FM_broadcast(pan_mcast_handler, fragment_data(frag), size);
    }

#ifndef BCAST_SKIP_HOME
    if (pan_pset_ismember(rcvr, pan_my_pid())) {
	fm_frag = FM_alloc_buf(size);
	memcpy(fm_frag->fm_buf, fragment_data(frag), size);
	pan_mcast_handler(fm_frag, size);
    }
#endif
}


void
pan_comm_multicast_small(pan_pset_p rcvr, pan_nsap_p nsap, void *data)
{
    char  small[do_align(MAX_SMALL_SIZE, BCAST_COMM_HDR_ALIGN) + NSAP_HDR_SIZE];
    int   size;
#ifndef BCAST_SKIP_HOME
    struct FM_buffer *fm_small;
#endif

    STATINC(n_mcast_small_send);

    memcpy(small, data, nsap->data_len);
    size = do_align(nsap->data_len, BCAST_COMM_HDR_ALIGN) + BCAST_HDR_SIZE;
    *(short *)(small + size) = pan_sys_nsap_index(nsap);
    size += NSAP_HDR_SIZE;

    if (pan_nr_platforms() > 1) {
#ifdef NEVER
printf("%2d: broadcast small %d bytes\n", pan_my_pid(), size);
#endif
	FM_broadcast(pan_mcast_handler, small, size);
    }

#ifndef BCAST_SKIP_HOME
    if (pan_pset_ismember(rcvr, pan_my_pid())) {
	fm_small = FM_alloc_buf(size);
	memcpy(fm_small->fm_buf, small, size);
	pan_mcast_handler(fm_small, size);
    }
#endif
}


int
FM_pending(void)
{
    return 1;
}
