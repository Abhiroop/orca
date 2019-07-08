#include "pan_sys_msg.h"

#include "pan_mp.h"

#include "pan_mp_queue.ci"
#include "pan_mp_tickets.ci"
#include "pan_mp_ports.ci"
#include "pan_mp_hdr.ci"



#define pan_mp_mode_t	int
#define MODE_2PHASE	MODE_SYNC2


static int		inits = 0;

static pan_mutex_p	mp_lock;

static pan_nsap_p	mp_nsap;

static mp_port_p	mp_direct_port;


#ifdef POLL_ON_WAIT
void pan_mp_poll_reply(void);
static int mp_poll = 0;
#endif


#ifdef STATISTICS
#include <stdio.h>

static int	n_rcve_served;
static int	n_rcve_queued;
static int	n_server_arrived;
static int	n_server_queued;

#define STATINC(n)	(++(n))
#else
#define STATINC(n)
#endif

/* Obtain a port to register a receiver on */

int
pan_mp_register_map(void)
{
    mp_port_p p;

    p = mp_port_get();

    p->port_nsap = mp_nsap;

    mp_q_init(&p->server_q);
    mp_q_init(&p->arrived_q);

    return mp_port_id(p);
}



void
pan_mp_free_map(int port_id)
{
    if (port_id >= MP_MAX_PORT_VALUE) {
	mp_ticket_clear(mp_ticket(port_id));
    }				/* Otherwise, await cleanup at shutdown */
}


void
pan_mp_clear_map(int port_id, void (*clear)(pan_msg_p msg))
{
    mp_port_p p = mp_port(port_id);

    mp_q_clear(&p->server_q, NULL);
    mp_q_clear(&p->arrived_q, clear);
}


int
pan_mp_send_message(int dest, int port, pan_msg_p msg, pan_mp_mode_t mode)
{
    mp_port_p p;
    mp_hdr_p  hdr;

    hdr = mp_hdr_push(msg);
    hdr->ticket = port;

    if (port >= MP_MAX_PORT_VALUE) {
					/* This is a ticket: a pan_mp_receive
					 * call has preceded this send call */
	pan_comm_unicast_msg(dest, msg, mp_nsap);
    } else {
	p = mp_port(port);
	pan_comm_unicast_msg(dest, msg, p->port_nsap);
    }

    if (mode == MODE_ASYNC) {
	pan_msg_clear(msg);
    } else {
	(void)mp_hdr_pop(msg);		/* Restore for reuse */
    }

    return -1;
}


void
pan_mp_finish_send(int ticket)
{
}


int
pan_mp_poll_send(int ticket)
{
    return 1;
}


int
pan_mp_receive_message(int port, pan_msg_p msg, pan_mp_mode_t mode)
{
    mp_ticket_p t;
    mp_port_p p;

    if (mode == MODE_SYNC) {
	pan_mp_finish_receive(pan_mp_receive_message(port, msg, MODE_2PHASE));

	return -1;

    } else if (mode == MODE_2PHASE) {

	pan_mutex_lock(mp_lock);

	if (port == DIRECT_MAP) {	/* Blocking receive on ticket */
	    t = mp_ticket_get();
	    t->msg = msg;

	} else {
	    p = mp_port(port);		/* Blocking receive on synch channel */

	    t = mp_q_deq(&p->arrived_q);
	    if (t == NULL) {		/* No waiting msgs. Just register */
		t = mp_ticket_get();
		t->port  = p;
		t->msg   = msg;
		mp_q_enq(&p->server_q, t);
		STATINC(n_server_queued);
	    } else {			/* Handle the rest in finish_rcve */
		pan_msg_copy(t->msg, msg);
		pan_msg_clear(t->msg);
		STATINC(n_server_arrived);
	    }
	}

	pan_mutex_unlock(mp_lock);

	return mp_ticket_id(t);

    } else {
	pan_panic("pan_mp_receive_message called in mode MODE_ASYNC");
	return -1;		/*NOTREACHED*/
    }
}


void
pan_mp_finish_receive(int ticket)
{
    mp_ticket_p t;

    t = mp_ticket(ticket);

    pan_mutex_lock(mp_lock);

    while (! (t->flags & MP_ARRIVED)) {
#ifdef POLL_ON_WAIT
	if (mp_poll) {
	    pan_mutex_unlock(mp_lock);
	    pan_poll();
	    pan_thread_yield();
	    pan_mutex_lock(mp_lock);
	} else {
	    pan_cond_wait(t->arrived);
	}
#else
	pan_cond_wait(t->arrived);
#endif
    }

    mp_ticket_clear(t);

    pan_mutex_unlock(mp_lock);
}



int
pan_mp_poll_receive(int ticket)
{
    int       result;
    mp_ticket_p t;

    t = mp_ticket(ticket);

    pan_mutex_lock(mp_lock);
    result = (t->flags & MP_ARRIVED) == MP_ARRIVED;
    pan_mutex_unlock(mp_lock);

    return result;
}



static void
mp_async_rcve(pan_msg_p msg)
{
    mp_hdr_p  hdr;
    mp_port_p p;

    hdr = mp_hdr_pop(msg);
    p = mp_port(hdr->ticket);
    p->handler(hdr->ticket, msg);
}



void
pan_mp_register_async_receive(int port, void (*handler)(int map, pan_msg_p msg))
{
    mp_port_p p = mp_port(port);

    p->handler   = handler;
    p->port_nsap = pan_nsap_create();
    pan_nsap_msg(p->port_nsap, mp_async_rcve, PAN_NSAP_UNICAST);
}


static void
mp_rcve_ticket(mp_ticket_p t, pan_msg_p msg)
{
    pan_msg_copy(msg, t->msg);
    pan_msg_clear(msg);
    t->flags |= MP_ARRIVED;
    pan_cond_signal(t->arrived);
}


static void
mp_rcve_port(mp_port_p p, pan_msg_p msg)
{
    mp_ticket_p t;

    t = mp_q_deq(&p->server_q);

    if (t == NULL) {		/* No pending server. Enqueue the message */
	t = mp_ticket_get();
	t->msg   = msg;
	t->flags = MP_ARRIVED;
	t->port  = p;
	mp_q_enq(&p->arrived_q, t);
	STATINC(n_rcve_queued);
    } else {			/* Pending server. Signal it. */
	pan_msg_copy(msg, t->msg);
	pan_msg_clear(msg);
	t->flags |= MP_ARRIVED;
	pan_cond_signal(t->arrived);
	STATINC(n_rcve_served);
    }
}


static void
mp_rcve(pan_msg_p msg)
{
    mp_hdr_p  hdr;
    int       ticket;

    hdr = mp_hdr_pop(msg);
    ticket = hdr->ticket;

    pan_mutex_lock(mp_lock);

    if (ticket >= MP_MAX_PORT_VALUE) {	/* For blocking receive on ticket */
	mp_rcve_ticket(mp_ticket(ticket), msg);
    } else {				/* For blocking receive on port */
	mp_rcve_port(mp_port(ticket), msg);
    }

    pan_mutex_unlock(mp_lock);
}


void
pan_mp_init(void)
{
    if (inits++) {
	return;
    }

    mp_lock = pan_mutex_create();

    mp_nsap = pan_nsap_create();
    pan_nsap_msg(mp_nsap, mp_rcve, PAN_NSAP_UNICAST);

    mp_port_start(mp_lock);
    mp_ticket_start(mp_lock);

    mp_direct_port = mp_port_get();
    assert(mp_port_id(mp_direct_port) == DIRECT_MAP);
}


void
pan_mp_end(void)
{
    if (--inits) {
	return;
    }

    mp_ticket_end();
    mp_port_end();

    pan_nsap_clear(mp_nsap);

    pan_mutex_clear(mp_lock);
#ifdef STATISTICS
    printf("%2d: MP %5s %5s %5s %5s\n",
	   pan_my_pid(), "srv/a", "srv/q", "rcv/a", "rcv/q");
    printf("%2d: MP %5d %5d %5d %5d\n",
	   pan_my_pid(), n_server_arrived, n_server_queued,
	   n_rcve_served, n_rcve_queued);
#endif
}

#ifdef POLL_ON_WAIT
void
pan_mp_poll_reply(void)
{
    mp_poll = 1;
}
#endif
