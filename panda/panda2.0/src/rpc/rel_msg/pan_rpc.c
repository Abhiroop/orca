#include <assert.h>
#include <stdio.h>

#include "pan_sys_msg.h"	/* System interface */

#include "pan_trace.h"

#include "pan_rpc.h"		/* Provides an RPC interface */
#include "pan_upcall.h"
#include "pan_upcall.ci"
#include "pan_rpc_ticket.ci"

/*
 * Public RPC interface module.
 */


#ifdef TRACING
typedef struct TRC_RPC_T {
    short int partner;
    short int client_id;
} trc_rpc_t, *trc_rpc_p;

trc_event_t rpc_request;		/* Start RPC request */
trc_event_t rpc_done;			/* RPC request done */
trc_event_t rpc_service;		/* Start RPC service */
trc_event_t rpc_reply;			/* RPC service done */
#endif


				/* Static Variables */
static pan_nsap_p	 rpc_nsap;
static pan_rpc_handler_f rpc_handler;
static int               rpc_me;


				/* Forward declarations */
static void pan_rpc_upcall(pan_msg_p message);


/*
 * pan_rpc_init:
 *                 Initialize the RPC code. Register the RPC nsap.
 */

void
pan_rpc_init(void)
{
#ifdef TRACING
    rpc_request = trc_new_event(3300, sizeof(trc_rpc_t), "RPC request",
				"server = %h, id = %h");
    rpc_done    = trc_new_event(3300, sizeof(trc_rpc_t), "RPC done",
				"server = %h, id = %h");
    rpc_service = trc_new_event(3300, sizeof(trc_rpc_t), "RPC service",
				"client = %h, id = %h");
    rpc_reply   = trc_new_event(3300, sizeof(trc_rpc_t), "RPC reply",
				"client = %h, id = %h");
#endif

    pan_rpc_upcall_start();
    pan_rpc_ticket_start();

    rpc_me   = pan_my_pid();

    rpc_nsap = pan_nsap_create();
    pan_nsap_msg(rpc_nsap, pan_rpc_upcall, PAN_NSAP_UNICAST);
}


/*
 * pan_rpc_end:
 *                 End the RPC code.
 */

void
pan_rpc_end(void)
{
    pan_rpc_ticket_end();
    pan_rpc_upcall_end();
    pan_nsap_clear(rpc_nsap);
}


/*
 * pan_rpc_register:
 *                 Registers an RPC upcall handler. Only one upcall
 *                 handler can be registered.
 */

void
pan_rpc_register(pan_rpc_handler_f handler)
{
    assert(!rpc_handler);
    rpc_handler = handler;
}

/*
 * pan_rpc_trans:
 *                 Performs an RPC operation.
 */

void
pan_rpc_trans(int server, pan_msg_p request, pan_msg_p *reply)
{
    rpc_hdr_p req_hdr;
#ifdef TRACING
    trc_rpc_t trc_rpc;
#endif

#ifdef KOEN
    extern pan_time_p up_start, up_stop, up_total;
    extern int nr_up;
    extern pan_time_p raoul_down_start, raoul_down_stop, raoul_down_total;
    extern int raoul_nr_down;

    pan_time_get(raoul_down_start);
#endif

#ifndef NDEBUG				/* Add this test RFHH */
    if (server == rpc_me) {
	fprintf(stderr, "%2d: Warning: rpc to self!\n", rpc_me);
    }
#endif

    /* the upcall struct is used to address the reply */
    req_hdr = pan_rpc_hdr_push(request);

    /* acquire an entry in the receive data structure */
    req_hdr->client.uus.ticket = pan_rpc_ticket_get();
    req_hdr->client.uus.pid    = rpc_me;
    req_hdr->type = RPC_REQUEST;

#ifdef TRACING
    trc_rpc.partner = server;
    trc_rpc.client_id = req_hdr->client.uus.ticket;
    trc_event(rpc_request, &trc_rpc);
#endif
    pan_comm_unicast_msg(server, request, rpc_nsap);

    /* wait for reply message; this also clears the entry */
    *reply = pan_rpc_ticket_wait(req_hdr->client.uus.ticket);
    trc_event(rpc_done, &trc_rpc);

    (void)pan_rpc_hdr_pop(request);	/* Restore for reuse; awghhh */

#ifdef KOEN
    pan_time_get(up_stop);
    pan_time_sub(up_stop, up_start);
    pan_time_add(up_total, up_stop);
    nr_up++;
#endif
}


/*
 * pan_rpc_reply:
 *                 Sends a reply to the address in the upcall structure.
 *                 The reply message is cleared by this routine.
 */

void
pan_rpc_reply(pan_upcall_p req_hdr, pan_msg_p reply)
{
    rpc_hdr_p    reply_hdr;
#ifdef TRACING
    trc_rpc_t trc_rpc;
#endif

    reply_hdr = pan_rpc_hdr_push(reply);
    reply_hdr->type       = RPC_REPLY;
    reply_hdr->client.ptr = req_hdr;

#ifdef TRACING
    trc_rpc.partner = reply_hdr->client.uus.pid;
    trc_rpc.client_id = reply_hdr->client.uus.ticket;
    trc_event(rpc_reply, &trc_rpc);
#endif

    pan_comm_unicast_msg(reply_hdr->client.uus.pid, reply, rpc_nsap);

    pan_msg_clear(reply);
}

/*
 * pan_rpc_upcall:
 *              Handles an rpc message.
 *		If a request, it pops the upcall structure from the request,
 *		and performs the upcall.
 *		If a reply, it signals the waiting client.
 */

static void
pan_rpc_upcall(pan_msg_p msg)
{
    rpc_hdr_p    rpc_hdr;
#ifdef TRACING
    trc_rpc_t trc_rpc;
#endif

    rpc_hdr = pan_rpc_hdr_pop(msg);

    if (rpc_hdr->type == RPC_REQUEST) {

	if (rpc_handler == NULL) {
	    fprintf(stderr, "%2d: Discarded RPC msg because no handler"
		    " installed; rpc_hdr: %d %d\n",
		    rpc_me, rpc_hdr->client.uus.pid,
		    rpc_hdr->client.uus.ticket);
	    return;
	}

#ifdef TRACING
	trc_rpc.partner = rpc_hdr->client.uus.pid;
	trc_rpc.client_id = rpc_hdr->client.uus.ticket;
	trc_event(rpc_service, &trc_rpc);
#endif

	/*
	 * For orca, this handler is r_rpc_invocation().
	 */
					/* The upcall structure is encoded
					 * in the pointer to save a malloc/free
					 */
	rpc_handler(rpc_hdr->client.ptr, msg);
    } else {
	pan_rpc_ticket_signal(rpc_hdr->client.uus.ticket, msg);
    }
}
