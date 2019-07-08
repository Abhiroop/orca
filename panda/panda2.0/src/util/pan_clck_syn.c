/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <math.h>
#include <stdarg.h>

#ifdef OS_FRAGMENTS
#include "pan_sys_msg.h"
#else
#include "pan_sys.h"
#endif

#include "pan_util.h"

#include "pan_stddev.h"
#include "pan_clock_sync.h"
#include "pan_clock_msg.h"


/*
 * We do not implement the time server algorithm that is as simpleminded as
 * is possible:
 * the other hosts assume the time of the time server; in turn,		! NOT !
 * each of the other hosts exchanges time with the time server.		! NOT !
 *									! NOT !
 * A time exchange consists of 2 unicasts:				! NOT !
 *									! NOT !
 *  Time server     Client						! NOT !
 *									! NOT !
 *                  t1							! NOT !
 *          <-----							! NOT !
 *      t2								! NOT !
 *          ----->							! NOT !
 *                  t3							! NOT !
 *									! NOT !
 * Assume we have:							! NOT !
 *									! NOT !
 *   Real time between t1 and t2 = real time between t2 and t3 = d2	! NOT !
 *      because much the same thing happens: a message is sent,		! NOT !
 *      upcall, sync with cond var and mutex, etc.			! NOT !
 *									! NOT !
 * Clock difference = d0.						! NOT !
 *									! NOT !
 * Equations:								! NOT !
 * t2      = t1 - d + d2				(2)		! NOT !
 *			((2) - (1): t2 - t0 = d1 + d2)		(3)	! NOT !
 * t3 - d  = t2 + d2					(4)		! NOT !
 * 			(4) - (2): t1 + t3 - 2 t2 = 2 d		(5)	! NOT !
 *									! NOT !
 * The time server does not know the time shift.			! NOT !
 * The other host calculates its time shift from Eq. (5).		! NOT !
 */


/* An alternative implementation ensures that the times need not be the same,
 * and a first step in averaging is done automatically at the cost of one
 * more unicast and some delay; moreover, there is less contention for the
 * time server as _it_ takes the initiative:
 */
/*
 * Time server algorithm is nearly as simpleminded as is possible:
 * In turn, the time server exchanges time with each of the other hosts.
 * Half a time exchange consists of 2 unicasts:
 *
 *  Time server    Client	Exchange:	Server -Remember- Client:
 *      t0
 *          ----->		t0
 *                  t1						  t0,t1
 *          <-----		t1
 *      t2					t2
 *
 * Second half of time exchange is mirrored:
 *
 *                  t3						  t0,t1,t3
 *          <-----		t3
 *      t4
 *          ----->		t2,t4
 *                  t5
 *
 * And now we have:
 *
 *   Real time between t0 and t1 = real time between t3 and t4 = d1
 *   ----------------- t1 --- t2 = ----------------- t4 --- t5 = d2
 *
 * Clock difference = d.
 *
 * Equations:
 * t0 + d1 = t1 - d                                     (1)
 * t2      = t1 - d + d2                                (2)
 *                      ((2) - (1): t2 - t0 = d1 + d2)          (3)
 * t3 + d1 = t4 + d                                     (4)
 * t5      = t4 + d + d2                                (5)
 *                      (4) - (1): t3 - t0 = t4 - t1 + 2d       (6a)
 *                      (5) - (2): t5 - t2 = t4 - t1 + 2d       (6b)
 *
 * The time server derives the time shift from Eq. 6a, the other host averages
 * between this value and Eq. 6b.
 *
 *
 * Implementation:
 *
 * The server "main" thread synchronises each of the clients in turn.
 * For each client:
 *     It does a half time exchange by calling exchange_times,
 *     awaiting the reply of the first half time exchange. The server "main"
 *     thread is signalled by the first upcall at the server site.
 *     then it awaits handling of the upcall of the second half time
 *     exchange.
 *     This second half time exchange (initiated by the client)consists
 *     of actions by the upcall. It sends a reply (containing t2 and t4)
 *     to the client, then it signals the server "main" thread so it may
 *     continue with the next client.
 *
 * The client "main" thread awaits a signal(*)from the upcall corresponding
 * to the first half exchange. This upcall consists of:
 *     - send a reply to the server (containing t1)
 *     - signal(*)the client "main" thread.
 * Then the client "main" thread idles for a short time (recommended: a few
 * times the "unicast" time), then performs the second half time exchange
 * by sending a message to the server containing t3. Then it awaits a signal
 * by the upcall for the reply message from the server (i.e. the reply part
 * of the second haf exchange).
 * This second upcall calculates the time offset and signals the client
 * "main" thread that the clock synchronisation is complete.
 *
 * Reliability:
 * proto_unicast is assumed to be unreliable, so there is a retrial protocol
 * so the first send of the time exchange is repeated until an answer has
 * arrived. Since timing is (extremely)important, we must know to which
 * retrial the response is; therefore, the retrials are numbered.
 * Since the network is probably reliable most of the time (loss of a message
 * is a rarity), answers to queries other than the most recent one are
 * discarded.
 * This has one significant pitfall: if the time-out is shorter than the
 * round trip time, this will never occur. Ad hoc solution: if this has
 * happened 3 times, the timeout is increased.
 *
 * For the purpose of time exchange, each platform registers a time server
 * upcall routine with the system layer communication daemon.
 */





static pan_pset_p  the_world;
static int         upb_platform;

static int         time_server;		/* time server platform */

static pan_time_p  my_clock_shift;	/* Result of clock sync */

static pan_time_p  unicast_delay;	/* delay before 2. half */
static pan_time_p  clock_req_timeout;	/* timeout for reliability */

static const int   FAIL_INCREASE = 3;	/* increase timeout after # rpc
					 * failures */

static pan_nsap_p  clock_nsap;		/* nsap for clock sync msgs */

 /* t_global = t_local - shift */
static pan_time_p  my_d_clock_shift;	/* Std dev of my clock sync */
static int         time_is_synced = 0;

static pan_mutex_p clock_reply_lock;	/* Reply sync between main */
static pan_cond_p  clock_reply_arrived;	/* thread and upcalls */
static pan_mutex_p clock_request_lock;	/* Request sync between main */
static pan_cond_p  clock_request_arrived;	/* thread and upcalls */
static int         server_msg_arrived;		/* Signal client main thread */
static int        *client_msg_arrived;		/* Signal server main thread */

#ifndef OS_FRAGMENTS
static pan_msg_p   clock_rcv_msg;		/* Assemble incoming msg */
#endif		/* OS_FRAGMENTS */
static clock_hdr_t clock_reply_hdr;		/* incoming reply header */
static int         clock_new_reply = 0;	/* flag to signal new reply */

static pan_time_p *t_arrive_server_req;	/* = t2, remembered by server */
static pan_time_p  t_arrive_client_req;	/* = t1, remembered by client */
static pan_time_p  t_sent_server_req;	/* = t0, remembered by client */
static pan_time_p  t_sent_client_req;	/* = t3, remembered by client */


/*------ Clock synch functions -----------------------------------------------*/


#ifdef OS_FRAGMENTS


static void
exchange_times(int i, int round, pan_msg_p clock_req,
	       void *time_buf, clock_hdr_p hdr)
{
    pan_time_p  now = pan_time_create();

    hdr->type = CLOCK_REQ;
    hdr->sender = pan_my_pid();
    hdr->id = round;

    pan_time_get(now);
    pan_time_copy(t_sent_client_req, now);
    pan_time_marshall(now, time_buf);

    pan_comm_unicast_msg(i, clock_req, clock_nsap);

    pan_mutex_lock(clock_reply_lock);
	
    while (clock_reply_hdr.sender != i && clock_reply_hdr.id != round) {
	pan_cond_wait(clock_reply_arrived);
    }
    pan_mutex_unlock(clock_reply_lock);

    pan_time_clear(now);
}


static void
clock_upcall(pan_msg_p msg)
{
    clock_hdr_t h;
    clock_hdr_p hdr;
    pan_time_p  now = pan_time_create();
    pan_time_p  s_dt;
    pan_time_p  t;
    pan_time_p  t2;
    pan_time_p  t4;

    pan_time_get(now);

    h = *clock_hdr_pop(msg);

    if (pan_my_pid() == time_server) {

	switch (h.type) {

	case CLOCK_REQ:					/* now = t4 */
	    t = pan_time_create();
	    tm_pop_time(msg, t);		/* ignore t3 */

	    tm_push_time(msg, t_arrive_server_req[h.sender]);	/* t2 */
	    tm_push_time(msg, now);				/* t4 */
	    hdr = clock_hdr_push(msg);
	    hdr->type = CLOCK_REPLY;
	    hdr->sender = pan_my_pid();
	    hdr->id = h.id;

	    pan_comm_unicast_msg(h.sender, msg, clock_nsap);

	    pan_mutex_lock(clock_request_lock);
	    if (h.id > client_msg_arrived[h.sender]) {
		client_msg_arrived[h.sender] = h.id;
	    }
	    pan_cond_signal(clock_request_arrived);
	    pan_mutex_unlock(clock_request_lock);

	    pan_time_clear(t);
	    break;

	case CLOCK_REPLY:				/* now = t2 */
	    pan_time_copy(t_arrive_server_req[h.sender], now);		/* t2 */
	    pan_mutex_lock(clock_reply_lock);
	    clock_new_reply = 1;
	    clock_reply_hdr = h;
	    pan_cond_signal(clock_reply_arrived);
	    pan_mutex_unlock(clock_reply_lock);
	    break;
	}

    } else {

	switch (h.type) {

	case CLOCK_REQ:					/* now = t1 */
	    tm_pop_time(msg, t_sent_server_req); /* remember t0 */

	    hdr = clock_hdr_push(msg);
	    hdr->type = CLOCK_REPLY;
	    hdr->sender = pan_my_pid();
	    hdr->id = h.id;

	    pan_comm_unicast_msg(time_server, msg, clock_nsap);

	    pan_time_copy(t_arrive_client_req, now);	/* remember t1 */

	    pan_mutex_lock(clock_request_lock);
	    if (h.id > server_msg_arrived) {
		server_msg_arrived = h.id;
	    }
	    pan_cond_signal(clock_request_arrived);
	    pan_mutex_unlock(clock_request_lock);
	    break;

	case CLOCK_REPLY:				/* now = t5 */
	    s_dt = pan_time_create();
	    t2 = pan_time_create();
	    t4 = pan_time_create();

	    tm_pop_time(msg, t4);
	    tm_pop_time(msg, t2);

	    pan_time_sub(t4, t_arrive_client_req);	/* t4 - t1 */
	    pan_time_copy(s_dt, t_sent_client_req);
	    pan_time_sub(s_dt, t_sent_server_req);	/* t3 - t0 */
	    pan_time_sub(s_dt, t4);		/* t3 - t0 - (t4 - t1) */
/* printf("\"His\"  = %f / 2; ", pan_time_t2d(s_dt)); */
	    pan_time_copy(my_clock_shift, now);
	    pan_time_sub(my_clock_shift, t2);		/* t5 - t2 */
	    pan_time_sub(my_clock_shift, t4);	/* t5 - t2 - (t4 - t1) */
/* printf("\"Mine\" = %f / 2\n", pan_time_t2d(my_clock_shift)); */
	    pan_time_add(my_clock_shift, s_dt);
	    pan_time_div(my_clock_shift, 4);

	    pan_mutex_lock(clock_reply_lock);
	    clock_reply_hdr = h;
	    time_is_synced = 1;
	    clock_new_reply = 1;
	    pan_cond_signal(clock_reply_arrived);
	    pan_mutex_unlock(clock_reply_lock);

	    pan_time_clear(s_dt);
	    pan_time_clear(t2);
	    pan_time_clear(t4);
	    break;
	}

    }

    pan_msg_clear(msg);

    pan_time_clear(now);
}



#else		/* OS_FRAGMENTS */


static void
exchange_times(int i, int round, pan_msg_p clock_req,
	       void *time_buf, clock_hdr_p hdr)
{
    int         reply_arrived = 0;
    int         attempts;
    int         failed_attempts;
    pan_time_p  now = pan_time_create();
    pan_time_p  d_timeout = pan_time_create();
    pan_fragment_p frgm;

    pan_mutex_lock(clock_reply_lock);
    attempts = 0;
    failed_attempts = 0;
    pan_time_copy(d_timeout, clock_req_timeout);

    hdr->type = CLOCK_REQ;
    hdr->sender = pan_my_pid();
    hdr->id = round;

    while (1) {
	pan_time_get(now);
	pan_time_copy(t_sent_client_req, now);
	pan_time_marshall(now, time_buf);
	hdr->attempts = attempts;

	frgm = pan_msg_fragment(clock_req, clock_nsap);
	pan_comm_unicast_fragment(i, frgm);
	if (pan_msg_next(clock_req)) {
	    pan_panic("Clock msg does not fit in one fragment\n");
	}

	pan_time_get(now);
	pan_time_add(now, d_timeout);
	reply_arrived = pan_cond_timedwait(clock_reply_arrived, now);

	if (reply_arrived) {
	    assert(clock_new_reply);
	    if (clock_reply_hdr.sender == i) {
		/* Reply from correct host */
		if (clock_reply_hdr.id == round &&
			clock_reply_hdr.attempts == attempts) {
		    				/* Correct retrial number */
		    clock_new_reply = 0;	/* trigger assert */
		    break;
		} else {
		    ++failed_attempts;
		    if (failed_attempts == FAIL_INCREASE) {
			failed_attempts = 0;
			pan_time_mulf(d_timeout, 1.2);
		    }
		}
	    }
	    clock_new_reply = 0;		/* trigger assert */
	}
	++attempts;
    }

    pan_mutex_unlock(clock_reply_lock);

    pan_time_clear(now);
}


static void
clock_upcall(pan_fragment_p frgm)
{
    clock_hdr_t h;
    clock_hdr_p hdr;
    pan_time_p  now = pan_time_create();
    pan_time_p  s_dt;
    pan_time_p  t;
    pan_time_p  t2;
    pan_time_p  t4;

    pan_time_get(now);

    pan_msg_assemble(clock_rcv_msg, frgm, 0);

    h = *clock_hdr_pop(clock_rcv_msg);

    if (pan_my_pid() == time_server) {

	switch (h.type) {

	case CLOCK_REQ:					/* now = t4 */
	    t = pan_time_create();
	    tm_pop_time(clock_rcv_msg, t);		/* ignore t3 */

	    tm_push_time(clock_rcv_msg, t_arrive_server_req[h.sender]);	/* t2 */
	    tm_push_time(clock_rcv_msg, now);				/* t4 */
	    hdr = clock_hdr_push(clock_rcv_msg);
	    hdr->type = CLOCK_REPLY;
	    hdr->sender = pan_my_pid();
	    hdr->id = h.id;
	    hdr->attempts = h.attempts;

	    frgm = pan_msg_fragment(clock_rcv_msg, clock_nsap);
	    pan_comm_unicast_fragment(h.sender, frgm);
	    if (pan_msg_next(clock_rcv_msg)) {
		pan_panic("Clock reply msg does not fit in one fragment\n");
	    }

	    pan_mutex_lock(clock_request_lock);
	    if (h.id > client_msg_arrived[h.sender]) {
		client_msg_arrived[h.sender] = h.id;
	    }
	    pan_cond_signal(clock_request_arrived);
	    pan_mutex_unlock(clock_request_lock);

	    pan_time_clear(t);
	    break;

	case CLOCK_REPLY:				/* now = t2 */
	    pan_time_copy(t_arrive_server_req[h.sender], now);		/* t2 */
	    pan_mutex_lock(clock_reply_lock);
	    clock_new_reply = 1;
	    clock_reply_hdr = h;
	    pan_cond_signal(clock_reply_arrived);
	    pan_mutex_unlock(clock_reply_lock);
	    break;
	}

    } else {

	switch (h.type) {

	case CLOCK_REQ:					/* now = t1 */
	    tm_pop_time(clock_rcv_msg, t_sent_server_req); /* remember t0 */

	    hdr = clock_hdr_push(clock_rcv_msg);
	    hdr->type = CLOCK_REPLY;
	    hdr->sender = pan_my_pid();
	    hdr->id = h.id;
	    hdr->attempts = h.attempts;

	    frgm = pan_msg_fragment(clock_rcv_msg, clock_nsap);
	    pan_comm_unicast_fragment(time_server, frgm);
	    if (pan_msg_next(clock_rcv_msg)) {
		pan_panic("Clock msg does not fit in one fragment\n");
	    }

	    pan_time_copy(t_arrive_client_req, now);	/* remember t1 */

	    pan_mutex_lock(clock_request_lock);
	    if (h.id > server_msg_arrived) {
		server_msg_arrived = h.id;
	    }
	    pan_cond_signal(clock_request_arrived);
	    pan_mutex_unlock(clock_request_lock);
	    break;

	case CLOCK_REPLY:				/* now = t5 */
	    s_dt = pan_time_create();
	    t2 = pan_time_create();
	    t4 = pan_time_create();

	    tm_pop_time(clock_rcv_msg, t4);
	    tm_pop_time(clock_rcv_msg, t2);

	    pan_time_sub(t4, t_arrive_client_req);	/* t4 - t1 */
	    pan_time_copy(s_dt, t_sent_client_req);
	    pan_time_sub(s_dt, t_sent_server_req);	/* t3 - t0 */
	    pan_time_sub(s_dt, t4);		/* t3 - t0 - (t4 - t1) */
/* printf("\"His\"  = %f / 2; ", pan_time_t2d(s_dt)); */
	    pan_time_copy(my_clock_shift, now);
	    pan_time_sub(my_clock_shift, t2);		/* t5 - t2 */
	    pan_time_sub(my_clock_shift, t4);	/* t5 - t2 - (t4 - t1) */
/* printf("\"Mine\" = %f / 2\n", pan_time_t2d(my_clock_shift)); */
	    pan_time_add(my_clock_shift, s_dt);
	    pan_time_div(my_clock_shift, 4);

	    pan_mutex_lock(clock_reply_lock);
	    clock_reply_hdr = h;
	    time_is_synced = 1;
	    clock_new_reply = 1;
	    pan_cond_signal(clock_reply_arrived);
	    pan_mutex_unlock(clock_reply_lock);

	    pan_time_clear(s_dt);
	    pan_time_clear(t2);
	    pan_time_clear(t4);
	    break;
	}

    }

    pan_msg_empty(clock_rcv_msg);

    pan_time_clear(now);
}


#endif		/* OS_FRAGMENTS */


static void
one_clock_sync(int round, pan_time_p shift)
{
    int         i;
    pan_msg_p   clock_msg;
    void       *t_buf;
    clock_hdr_p hdr;

    clock_msg = pan_msg_create();
    t_buf = pan_msg_push(clock_msg, pan_time_size(), alignof(char));
    hdr = clock_hdr_push(clock_msg);

    if (pan_my_pid() == time_server) {

	pan_time_copy(my_clock_shift, pan_time_zero);

	i = pan_pset_find(the_world, time_server + 1);
	while (i != -1) {
	    exchange_times(i, round, clock_msg, t_buf, hdr);
	    pan_mutex_lock(clock_request_lock);
	    while (client_msg_arrived[i] < round) {
		pan_cond_wait(clock_request_arrived);
	    }
	    pan_mutex_unlock(clock_request_lock);
	    i = pan_pset_find(the_world, i + 1);
	}

    } else {

	pan_mutex_lock(clock_request_lock);
	while (server_msg_arrived < round) {
	    pan_cond_wait(clock_request_arrived);
	}
	pan_mutex_unlock(clock_request_lock);
#ifndef OS_FRAGMENTS
	pan_sleep(unicast_delay);
#endif
	exchange_times(time_server, round, clock_msg, t_buf, hdr);
    }

    pan_msg_clear(clock_msg);

    pan_time_copy(shift, my_clock_shift);
}


int
pan_clock_sync(int n_syncs, pan_time_p shift, pan_time_p std)
{
    double      sum = 0.0;
    double      squares = 0.0;
    double      d_std = 0.0;
    int         i;
    int         n;
    pan_time_p *clock_shift;
    int        *discarded;
    double     *d_sh;
    int         converged = 0;
    double      offset;

    clock_shift = pan_malloc(n_syncs * sizeof(pan_time_p));
    for (i = 0; i < n_syncs; i++) {
	clock_shift[i] = pan_time_create();
	one_clock_sync(i, clock_shift[i]);
    }

    discarded = pan_malloc(n_syncs * sizeof(int));
    d_sh = pan_malloc(n_syncs * sizeof(double));
    for (i = 0; i < n_syncs; i++) {
	d_sh[i] = pan_time_t2d(clock_shift[i]);
	discarded[i] = 0;
    }

    offset = 0.0;
    for (i = 0; i < n_syncs; i++) {
	offset += d_sh[i];
    }
    offset = offset / n_syncs;
    for (i = 0; i < n_syncs; i++) {
	d_sh[i] -= offset;
    }

    n = n_syncs;
    while (!converged) {
	sum = 0.0;
	squares = 0.0;
	for (i = 0; i < n_syncs; i++) {
	    if (!discarded[i]) {
		sum += d_sh[i];
		squares += d_sh[i] * d_sh[i];
	    }
	}
	d_std = stdev(squares, sum, n);
	sum = sum / n;
	converged = 1;
	for (i = 0; i < n_syncs; i++) {
	    if ((!discarded[i]) && (fabs(d_sh[i] - sum) > 2 * d_std)) {
					    /* discard this data point */
		discarded[i] = 1;
		converged = 0;
		--n;
	    }
	}
	if (n < 3) {
	    converged = 1;
	}
    }
				/* Retain reliable value in my_clock_shift */
    pan_time_d2t(my_clock_shift, sum + offset);
    pan_time_d2t(my_d_clock_shift, d_std);
    pan_time_copy(shift, my_clock_shift);
    pan_time_copy(std, my_d_clock_shift);

    for (i = 0; i < n_syncs; i++) {
	pan_time_clear(clock_shift[i]);
    }
    pan_free(clock_shift);
    pan_free(discarded);
    pan_free(d_sh);

    return n;
}


void
pan_clock_sync_start(void)
{
    int         i;

    my_clock_shift      = pan_time_create();
    my_d_clock_shift    = pan_time_create();

    unicast_delay       = pan_time_create();
    pan_time_set(unicast_delay, 0, 20000000);
    clock_req_timeout   = pan_time_create();

    t_arrive_client_req = pan_time_create();
    t_sent_server_req   = pan_time_create();
    t_sent_client_req   = pan_time_create();

    clock_reply_lock      = pan_mutex_create();
    clock_reply_arrived   = pan_cond_create(clock_reply_lock);
    clock_request_lock    = pan_mutex_create();
    clock_request_arrived = pan_cond_create(clock_request_lock);

    the_world = pan_pset_create();
    pan_pset_fill(the_world);
				/* The time server is the platform with the
				 * lowest number */
    time_server = pan_pset_find(the_world, 0);
    if (pan_my_pid() == time_server) {
	upb_platform = time_server + 1;
	i = pan_pset_find(the_world, upb_platform);
	while (i != -1) {
	    ++i;
	    if (i > upb_platform) {
		upb_platform = i;
	    }
	    i = pan_pset_find(the_world, i);
	}
	t_arrive_server_req = pan_malloc(upb_platform * sizeof(pan_time_p));
	client_msg_arrived = pan_malloc(upb_platform * sizeof(int));
	for (i = 0; i < upb_platform; i++) {
	    client_msg_arrived[i] = -1;
	    t_arrive_server_req[i] = pan_time_create();
	}
	time_is_synced = 1;
    } else {
	server_msg_arrived = -1;
    }
    pan_time_copy(clock_req_timeout, unicast_delay);
    pan_time_mul(clock_req_timeout, 10);
    clock_nsap = pan_nsap_create();
#ifdef OS_FRAGMENTS
    pan_nsap_msg(clock_nsap, clock_upcall, PAN_NSAP_UNICAST);
#else		/* OS_FRAGMENTS */
    clock_rcv_msg = pan_msg_create();
    pan_nsap_fragment(clock_nsap, clock_upcall, 0, PAN_NSAP_UNICAST);
#endif		/* OS_FRAGMENTS */
}


void
pan_clock_sync_end(void)
{
    int i;

    /* proto_unbind(PROT_CLOCK_PROTOCOL); */
    pan_nsap_clear(clock_nsap);
#ifndef OS_FRAGMENTS
    pan_msg_clear(clock_rcv_msg);
#endif
    pan_mutex_clear(clock_reply_lock);
    pan_cond_clear(clock_reply_arrived);
    pan_mutex_clear(clock_request_lock);
    pan_cond_clear(clock_request_arrived);
    pan_pset_clear(the_world);
    if (pan_my_pid() == time_server) {
	for (i = 0; i < upb_platform; i++) {
	    pan_time_clear(t_arrive_server_req[i]);
	}
	pan_free(client_msg_arrived);
	pan_free(t_arrive_server_req);
	client_msg_arrived = NULL;
	t_arrive_server_req = NULL;
    }
}


void
pan_clock_set_timeout(pan_time_p timeout, pan_time_p uc_delay)
{
    if (pan_time_cmp(timeout, pan_time_infinity) != 0) {
	pan_time_copy(clock_req_timeout, timeout);
    }
    if (pan_time_cmp(uc_delay, pan_time_infinity) != 0) {
	pan_time_copy(unicast_delay, uc_delay);
    }
}


void
pan_clock_get_timeout(pan_time_p timeout, pan_time_p uc_delay)
{
    pan_time_copy(timeout, clock_req_timeout);
    pan_time_copy(uc_delay, unicast_delay);
}


int
pan_clock_get_shift(pan_time_p timeout, pan_time_p d_timeout)
{
    if (time_is_synced) {
	pan_time_copy(timeout, my_clock_shift);
	pan_time_copy(d_timeout, my_d_clock_shift);
    }
    return time_is_synced;
}
