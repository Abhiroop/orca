/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_rpc.h"		/* Provides an RPC interface */
#include "pan_trace.h"
#include <assert.h>
#include <stdio.h>

/*
 * Public RPC interface module.
 */

				/* Here comes the dirty trick: encode the
				 * client id in the pointer of the pan_upcall_p,
				 * not in the struct it points to. This saves
				 * a malloc/free (on the critical path).
				 */
typedef union CLIENT_ID_T {
    struct {
	short int    dest;
	short int    entry;
    }			uus;
    pan_upcall_p	ptr;
} rpc_hdr_t, *rpc_hdr_p;


typedef struct pan_upcall{
    int			dummy;
}pan_upcall_t;



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
static int               rpc_map;
static pan_rpc_handler_f rpc_handler;

static int               rpc_me;	/* cache pan_my_pid() */


				/* Forward declarations */
static void pan_rpc_upcall(int map, pan_msg_p message);


static rpc_hdr_p
rpc_hdr_push(pan_msg_p msg)
{
    return pan_msg_push(msg, sizeof(rpc_hdr_t), alignof(rpc_hdr_t));
}

static rpc_hdr_p
rpc_hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(rpc_hdr_t), alignof(rpc_hdr_t));
}


/*
 * pan_rpc_init:
 *                 Initialize the RPC code. Register the RPC destination map.
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

    rpc_me  = pan_my_pid();
    rpc_map = pan_mp_register_map();
    
    pan_mp_register_async_receive(rpc_map, pan_rpc_upcall);
}


/*
 * pan_rpc_end:
 *                 End the RPC code. First, all pending messages on the
 *                 RPC map are cleared.
 */

void
pan_rpc_end(void)
{
    pan_mp_clear_map(rpc_map, pan_msg_clear);
    pan_mp_free_map(rpc_map);
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
pan_rpc_trans(int dest, pan_msg_p request, pan_msg_p *reply)
{
    rpc_hdr_p req_hdr;
    short int send_entry, rec_entry;
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
    if (dest == rpc_me) {
	fprintf(stderr, "%2d: Warning: rpc to self!\n", rpc_me);
    }
#endif

    *reply = pan_msg_create();
    
    /* the upcall struct is used to address the reply */
    req_hdr = rpc_hdr_push(request);

    /*
     * receive the reply before we send the request, so it will always be
     * buffered.
     */
    req_hdr->uus.entry = rec_entry = 
	pan_mp_receive_message(DIRECT_MAP, *reply, MODE_SYNC2);
    req_hdr->uus.dest  = rpc_me;

    /* Do not access upcall until after finish_send!!! */
#ifdef TRACING
    trc_rpc.partner = dest;
    trc_rpc.client_id = rec_entry;
    trc_event(rpc_request, &trc_rpc);
#endif
    send_entry = pan_mp_send_message(dest, rpc_map, request, MODE_SYNC2);

    /* wait for reply message */
    pan_mp_finish_receive(rec_entry);
    trc_event(rpc_done, &trc_rpc);

    /* wait until request message has been acknowledged */
    pan_mp_finish_send(send_entry);

    (void)rpc_hdr_pop(request);

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
 *                 The reply message is cleared by this call.
 */

void
pan_rpc_reply(pan_upcall_p upcall, pan_msg_p reply)
{
    rpc_hdr_t reply_hdr;
#ifdef TRACING
    trc_rpc_t trc_rpc;
#endif

    reply_hdr.ptr = upcall;

#ifdef TRACING
    trc_rpc.partner = reply_hdr.uus.dest;
    trc_rpc.client_id = reply_hdr.uus.entry;
    trc_event(rpc_reply, &trc_rpc);
#endif

    (void)pan_mp_send_message(reply_hdr.uus.dest, reply_hdr.uus.entry, reply,
			      MODE_ASYNC);
}

/*
 * pan_rpc_upcall:
 *                 Handles a request message. It registers a new message
 *                 on the rpc receive map, pops the upcall structure from
 *                 the request, and performs the upcall.
 */

static void
pan_rpc_upcall(int map, pan_msg_p request)
{
    rpc_hdr_p rpc_hdr;
#ifdef TRACING
    trc_rpc_t trc_rpc;
#endif

    rpc_hdr = rpc_hdr_pop(request);

    if (rpc_handler){

#ifdef TRACING
	trc_rpc.partner = rpc_hdr->uus.dest;
	trc_rpc.client_id = rpc_hdr->uus.entry;
	trc_event(rpc_service, &trc_rpc);
#endif

				/* The upcall structure is encoded in the
				 * pointer to save a malloc/free.
				 */
	/*
	 * For orca, this handler is r_rpc_invocation().
	 */
	rpc_handler(rpc_hdr->ptr, request);
    }else{
	fprintf(stderr, "Discarded RPC request because no handler installed;"
		" upcall: %d %d\n", rpc_hdr->uus.dest, rpc_hdr->uus.entry);
    }
}
    

#ifdef POLL_ON_WAIT

void
pan_rpc_poll_reply(void)
{
    pan_mp_poll_reply();
}

#endif
