
#include <string.h>
#include <assert.h>

#include "pan_sys.h"

#include "pan_util.h"

#include "global.h"
#include "header.h"
#include "name.h"
#include "name_server.h"

/* Author: Raoul Bhoedjang, October 1993
 *
 * Name Server:
 *   Handles communication with the name server.
 *
 * Data structures:
 * o group_id_buf: a buffer between pan_ns_register()and
 *   ns_handle_group_id which holds the group id returned
 *   by the name server when a client registers a name.
 *
 * Notes:
 * o The name server resides on a fixed and globally
 *   known location: NAME_SERVER_PID.
 * o When a client registers itself with the name server
 *   it draws a ticket and waits until the group id
 *   is returned in buffer group_id_buf.
 */



#define NAME_SERVER_PID   0
#define INIT_SIZE         8
#define INCREMENT         8


static pan_nsap_p ns_frag_nsap;


/* Termination */
static int         nr_clients;
static pan_mutex_p termination_lock;
static pan_cond_p  all_clients_done;


/********************************************************/
/*****     Group id buffer management                   */
/********************************************************/

/* Group id handoff monitor */
typedef struct GROUP_TICKET {
    group_id_t  group_id_buf;
    int         ticket;
    int         events;
    pan_mutex_p lock;
    pan_cond_p  advanced;
    pan_cond_p  id_empty;
} group_ticket_t, *group_ticket_p;

static group_ticket_t gidbuf;


static void
gidbuf_start(void)
{
    gidbuf.ticket = 1;
    gidbuf.events = 0;
    gidbuf.group_id_buf = NO_GROUP_ID;
    gidbuf.lock = pan_mutex_create();
    gidbuf.id_empty = pan_cond_create(gidbuf.lock);
    gidbuf.advanced = pan_cond_create(gidbuf.lock);
}


static void
gidbuf_end(void)
{
    pan_cond_clear(gidbuf.advanced);
    pan_cond_clear(gidbuf.id_empty);
    pan_mutex_clear(gidbuf.lock);
}


static void
gidbuf_put(group_id_t gid)
{
    pan_mutex_lock(gidbuf.lock);
    while (gidbuf.group_id_buf != NO_GROUP_ID) {
	pan_cond_wait(gidbuf.id_empty);
    }

    gidbuf.group_id_buf = gid;		/* copy data into buffer */

    gidbuf.events++;
    pan_cond_broadcast(gidbuf.advanced);
    pan_mutex_unlock(gidbuf.lock);
}



static group_id_t
gidbuf_get(int my_ticket)
{
    group_id_t  gid;

    pan_mutex_lock(gidbuf.lock);
    while (gidbuf.events != my_ticket) {
	pan_cond_wait(gidbuf.advanced);
    }
    gid = gidbuf.group_id_buf;
    gidbuf.group_id_buf = NO_GROUP_ID;
    pan_cond_broadcast(gidbuf.id_empty);
    pan_mutex_unlock(gidbuf.lock);

    return gid;
}


static int
gidbuf_ticket(void)
{
    int         ret;

    pan_mutex_lock(gidbuf.lock);
    ret = gidbuf.ticket++;
    pan_mutex_unlock(gidbuf.lock);

    return ret;
}



/********************************************************/
/*****           Name server interface              *****/
/********************************************************/


static void
ns_handle_register(grp_hdr_p hdr, pan_fragment_p frag)
{
    int        *name_len;
    char       *group_name;
    pan_msg_p   msg;
    int         gid;
    int         sender;

    assert(pan_grp_me == NAME_SERVER_PID);

    sender = hdr->sender;

    /* unpack and register group name */
    msg = pan_msg_create();
    pan_msg_assemble(msg, frag, 0);
    name_len = pan_msg_pop(msg, sizeof(int), alignof(int));
    group_name = pan_msg_pop(msg, *name_len, alignof(unsigned char));
    gid = pan_name_enter(group_name);

    /* build and send reply message */
    pan_msg_empty(msg);

    frag = pan_msg_fragment(msg, ns_frag_nsap);


    if (! (pan_fragment_flags(frag) & PAN_FRAGMENT_LAST)) {
	pan_panic("Grp group_id does not fit in one fragment\n");
    }

    hdr = pan_fragment_header(frag);
    hdr->type = G_GROUPID;
    hdr->gid  = gid;

    pan_comm_unicast_fragment(sender, frag);

    pan_msg_clear(msg);
}


static void
ns_handle_group_id(grp_hdr_p hdr)
{
    gidbuf_put(hdr->gid);
}


static void
ns_handle_end(grp_hdr_p hdr)
{
    assert(pan_grp_me == NAME_SERVER_PID);

    pan_mutex_lock(termination_lock);
    if (--nr_clients == 0) {
	pan_cond_signal(all_clients_done);
    }
    pan_mutex_unlock(termination_lock);
}


static void
discard_frag(pan_fragment_p frgm)
{
    pan_msg_p msg = pan_msg_create();

    pan_msg_assemble(msg, frgm, 0);
    pan_msg_clear(msg);
}


static void
ns_handle_frag(pan_fragment_p frgm)
{
    grp_hdr_p hdr = pan_fragment_header(frgm);

    switch (hdr->type) {
    case G_REGISTER:
	ns_handle_register(hdr, frgm);
	break;

    case G_GROUPID:
	ns_handle_group_id(hdr);
	discard_frag(frgm);
	break;

    case G_DONE:
	ns_handle_end(hdr);
	discard_frag(frgm);
	break;

    default:
	pan_panic("ns_handle_frag: illegal msg type\n");
    }
}


void
pan_ns_start(void)
{

#ifdef NO_NAME_SERVICE
return;
#endif

    termination_lock = pan_mutex_create();
    all_clients_done = pan_cond_create(termination_lock);

    if (pan_grp_me == NAME_SERVER_PID) {
	pan_name_start(INIT_SIZE, INCREMENT);
	nr_clients = pan_nr_platforms() - 1;
    }

    ns_frag_nsap = pan_nsap_create();
    pan_nsap_fragment(ns_frag_nsap, ns_handle_frag, sizeof(grp_hdr_t),
			PAN_NSAP_UNICAST);

    gidbuf_start();
}


void
pan_ns_end(void)
{
    pan_msg_p   msg;
    grp_hdr_p   hdr;
    pan_fragment_p frgm;


#ifdef NO_NAME_SERVICE
return;
#endif

    if (pan_grp_me == NAME_SERVER_PID) {
	/* Wait until there are no clients left */
	pan_mutex_lock(termination_lock);
	while (nr_clients > 0) {
	    pan_cond_wait(all_clients_done);
	}
	pan_mutex_unlock(termination_lock);
	pan_name_end();		/* added by HPH */
    } else {
	/* Inform name server */
	msg = pan_msg_create();
	frgm = pan_msg_fragment(msg, ns_frag_nsap);
	if (! (pan_fragment_flags(frgm) & PAN_FRAGMENT_LAST)) {
	    pan_panic("Grp done msg does not fit in one fragment\n");
	}
	hdr = pan_fragment_header(frgm);
	hdr->type = G_DONE;
	pan_comm_unicast_fragment(NAME_SERVER_PID, frgm);
	pan_msg_clear(msg);
    }

    gidbuf_end();

    pan_nsap_clear(ns_frag_nsap);

    pan_cond_clear(all_clients_done);
    pan_mutex_clear(termination_lock);
}


group_id_t
pan_ns_register(char *name)
{
    group_id_t  gid;
    int         my_ticket;
    pan_msg_p   msg;
    char       *p;
    int         name_len;
    int        *p_name_len;
    pan_fragment_p frag;
    grp_hdr_p   hdr;

#ifdef NO_NAME_SERVICE
return 0;
#endif

    if (pan_grp_me == NAME_SERVER_PID) {
	return pan_name_enter(name);
    }
    /* Draw a ticket, send request, and wait */
    my_ticket = gidbuf_ticket();

    msg = pan_msg_create();

    name_len = strlen(name) + 1;
    p = pan_msg_push(msg, name_len, alignof(unsigned char));
    strcpy(p, name);
    p_name_len = pan_msg_push(msg, sizeof(int), alignof(int));
    *p_name_len = name_len;

    frag = pan_msg_fragment(msg, ns_frag_nsap);

    if (! (pan_fragment_flags(frag) & PAN_FRAGMENT_LAST)) {
	pan_panic("Grp register does not fit in 1 fragment\n");
    }
    hdr = pan_fragment_header(frag);
    hdr->type   = G_REGISTER;
    hdr->sender = pan_grp_me;

    pan_comm_unicast_fragment(NAME_SERVER_PID, frag);

    pan_msg_clear(msg);

    gid = gidbuf_get(my_ticket);

    return gid;
}
