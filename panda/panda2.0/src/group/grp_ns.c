/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Name server tailored towards group communication.
 * Provides one service:
 *     pan_ns_lookup_or_register("group name", &group_id, &sequencer_platform);
 * The server is conceptually single-threaded;
 * The first client who does a pan_ns_lookup_or_register of a group_name,
 * becomes the sequencer. The server binds group_name to
 * (1) a new, unique group_id and
 * (2) the platform number of the sequencer.
 * Later clients for the
 * same group receive the group_id and the sequencer address, they become
 * ordinary group members.
 * This server runs on one platform only, and its address is known to
 * all platforms in the world (i.e. all platforms returned in pan_pset_fill()).
 *
 * THIS IMPLEMENTATION IS A HACK!!!!
 * The algorithm to find out whether this is the server's platform:
 * all processes start enumerating all legal platform numbers, and the first
 * one that exists in pan_pset_fill is the server platform.
 *
 *     Anyway, since the name server only services group communication, these
 *     routines are called from the group demuxer daemon, and the nameserver
 *     message header format is merged with the group header format.
 */


#ifdef NAME_SERVER_THREAD

#include "grp_ns_daemon.c"

#else


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pan_sys.h"

#include "pan_mp.h"

#include "grp_ns.h"



typedef struct GIDNAME_BIND_T gidname_bind_t, *gidname_bind_p;




typedef enum NS_MSG_T {
    NS_LOCATE_GRP,
    NS_HEREIS_GRP,
    NS_DEL_GRP,
    NS_ACK_DEL_GRP
}   ns_msg_t, *ns_msg_p;


typedef enum  NS_HDR_FLAG_T {
    NS_IS_BLANK		= 0,
    NS_IS_BIGENDIAN	= (0x1 << 0)
}   ns_hdr_flag_t;


typedef struct NS_HDR_T ns_hdr_t, *ns_hdr_p;

struct NS_HDR_T {
    ns_msg_t         type;
    int              gid;
    int              host;
    int              sender_id;
};



/*----- Global data structures for name service ------------------------------*/


static int             ns_map;
static int             ns_me;			/* Cache pan_my_pid() */
static int             ns_pid;			/* Name server pid */

						/* Monitor for any rpc client */
static pan_mutex_p     ns_send_lock;		/* Only 1 ns rcp at a time */


						/* Monitor for name server
						 * data structures */
static pan_mutex_p     ns_lock;
static pan_cond_p      ns_changed;			/* Wait for change */
static gidname_bind_p  bindings;			/* name-id bindings */



#ifdef STATISTICS

static int	   ns_home_replies	= 0;
static int	   ns_outside_replies	= 0;
static int	   ns_home_requests	= 0;
static int	   ns_outside_requests	= 0;

#define STATINC(n)	do { ++(n); } while (0)

#else

#define STATINC(n)

#endif




/*--- Module for group name bindings -----------------------------------------*/


struct GIDNAME_BIND_T {
    int         max_bindings;
    int         used_bindings;
    char      **name;
    int        *seq;
};


static gidname_bind_p
gidname_bind_create(int initial_max)
{
    int            i;
    gidname_bind_p bind;

    bind = pan_malloc(sizeof(gidname_bind_t));
    bind->max_bindings  = initial_max;
    bind->used_bindings = 0;
    bind->name = pan_malloc(initial_max * sizeof(char *));
    bind->seq  = pan_malloc(initial_max * sizeof(int));
    for (i = 0; i < initial_max; i++) {
	bind->name[i] = NULL;
    }

    return bind;
}


static int
register_new_name(gidname_bind_p bind, char *name, int len, int seq)
{
    int           gid;
    int           i;

    for (gid = 0; gid < bind->max_bindings && bind->name[gid] != NULL; gid++);

    if (gid == bind->max_bindings) {
	bind->max_bindings *= 2;
	bind->name = pan_realloc(bind->name, bind->max_bindings * sizeof(char *));
	bind->seq  = pan_realloc(bind->seq, bind->max_bindings * sizeof(int));
	for (i = gid; i < bind->max_bindings; i++) {
	    bind->name[i] = NULL;
	}
    }

    ++bind->used_bindings;
    bind->seq[gid]  = seq;
    i = strlen(name);
    bind->name[gid] = pan_malloc(i + 1);
    strcpy(bind->name[gid], name);

    return gid;
}


static boolean
reg_or_lookup(gidname_bind_p bind, char *name, int len, int sender,
	      int *gid, int *sequencer)
{
    int           g;
    boolean       is_new;
    int           seq;

    for (g = 0; g < bind->max_bindings; g++) {
	if (bind->name[g] != NULL && strcmp(name, bind->name[g]) == 0) {
	    break;
	}
    }
    is_new = (g == bind->max_bindings);

    if (is_new) {
#ifdef SEQUENCER
	seq = SEQUENCER;
#else
	seq = sender;
#endif
        g = register_new_name(bind, name, len, seq);
    }
    *sequencer = bind->seq[g];
    *gid       = g;

    return is_new;
}


/* Implementation for the name server delete */
static boolean
name_delete(gidname_bind_p bind, int gid)
{
    assert(ns_pid == ns_me);

    if (bind->name[gid] == NULL) {
	return FALSE;
    }
    pan_free(bind->name[gid]);
    bind->name[gid] = NULL;
    --bind->used_bindings;

    return TRUE;
}


static void
gidname_bind_clear(gidname_bind_p bind)
{
    int i;

    for (i = 0; i < bind->max_bindings; i++) {
	if (bind->name[i] != NULL) {
	    pan_free(bind->name[i]);
	}
    }
    pan_free(bind->name);
    pan_free(bind->seq);
    pan_free(bind);
}



/*--- End of module for group name bindings ----------------------------------*/




/* Upcall routine for the name server. Two types of request are recognised:
 * 1. register or lookup (see above)
 * 2. unregister. If the group already is unregistered, no further action
 *    is taken. An ack is sent anyway (the first ack may have got lost).
 */
static void
ns_server_upcall(int map, pan_msg_p msg)
{
    char     *name;
    ns_hdr_t  h;
    ns_hdr_p  hdr;
    int       len;
    int       sender;
    int      *p_len;

    assert(ns_pid == ns_me);
    assert(map    == ns_map);

    h = *(ns_hdr_p)pan_msg_pop(msg, sizeof(ns_hdr_t), alignof(ns_hdr_t));
    sender = h.host;

    pan_mutex_lock(ns_lock);

    switch (h.type) {

    case NS_LOCATE_GRP:
	len = *(int *)pan_msg_pop(msg, sizeof(int), alignof(int));
	name = pan_msg_look(msg, len, alignof(char));
	if (reg_or_lookup(bindings, name, len, sender, &h.gid, &h.host)) {
	    pan_cond_broadcast(ns_changed);
	}
	p_len = pan_msg_push(msg, sizeof(int), alignof(int));
	*p_len = len;
	h.type = NS_HEREIS_GRP;
	break;

    case NS_DEL_GRP:
	if (name_delete(bindings, h.gid)) {
	    pan_cond_broadcast(ns_changed);
	}
	h.type = NS_ACK_DEL_GRP;
	break;

    default:
	pan_panic("Wrong message type arrived at name server\n");
    }

    pan_mutex_unlock(ns_lock);

    hdr = pan_msg_push(msg, sizeof(ns_hdr_t), alignof(ns_hdr_t));
    *hdr = h;
    
    if (sender == ns_me) {
					STATINC(ns_home_replies);
    } else {
	pan_mp_send_message(sender, h.sender_id, msg, MODE_ASYNC);
					STATINC(ns_outside_replies);
    }

}


static void
do_local_ns_rpc(pan_msg_p msg)
{
    int old_prio;

    old_prio = pan_thread_setprio(pan_thread_maxprio());
    ns_server_upcall(ns_map, msg);
    pan_thread_setprio(old_prio);
}


static pan_msg_p
do_ns_rpc(pan_msg_p msg, ns_hdr_p hdr)
{
    pan_msg_p rcv_msg;
    int       send_id;
    int       recv_id;

    if (ns_pid == ns_me) {
					STATINC(ns_home_requests);
	do_local_ns_rpc(msg);
	return msg;
    }

    rcv_msg = pan_msg_create();
    recv_id = pan_mp_receive_message(DIRECT_MAP, rcv_msg, MODE_SYNC2);
					STATINC(ns_outside_requests);
    hdr->sender_id = recv_id;
    send_id = pan_mp_send_message(ns_pid, ns_map, msg, MODE_SYNC2);
    pan_mp_finish_receive(recv_id);
    pan_mp_finish_send(send_id);
    pan_msg_clear(msg);

    return rcv_msg;
}


/* Provides distributed register-or-lookup for group startup.
 * Before this routine may be called, initialise with pan_ns_init().
 */
void
pan_ns_register_or_lookup(char *name, int *gid, int *sequencer)
{
    int       len;
    pan_msg_p msg;
    char     *p;
    ns_hdr_p  hdr;
    int      *p_len;

    msg = pan_msg_create();

    len = strlen(name) + 1;
    p = pan_msg_push(msg, len, alignof(char));
    strcpy(p, name);
    p_len = pan_msg_push(msg, sizeof(int), alignof(int));
    *p_len = len;

    hdr = pan_msg_push(msg, sizeof(ns_hdr_t), alignof(ns_hdr_t));
    hdr->type = NS_LOCATE_GRP;
    hdr->host = ns_me;

    msg = do_ns_rpc(msg, hdr);

    hdr        = pan_msg_pop(msg, sizeof(ns_hdr_t), alignof(ns_hdr_t));
    *gid       = hdr->gid;
    *sequencer = hdr->host;

#ifndef NDEBUG
    {
    int rcv_len;
    rcv_len = *(int *)pan_msg_pop(msg, sizeof(int), alignof(int));
    assert(len == rcv_len);
    p = pan_msg_pop(msg, len, alignof(char));
    assert(strcmp(p, name) == 0);
    }
#endif

    pan_msg_clear(msg);
}


/* Provides distributed unregister for group shut-down. This should be called
 * only once for each group (typically by the sequencer).
 */
void
pan_ns_unregister_group(int gid)
{
    pan_msg_p msg;
    ns_hdr_p  hdr;

    msg = pan_msg_create();
    hdr = pan_msg_push(msg, sizeof(ns_hdr_t), alignof(ns_hdr_t));
    hdr->type = NS_DEL_GRP;
    hdr->host = ns_me;
    hdr->gid = gid;

    msg = do_ns_rpc(msg, hdr);

    pan_msg_clear(msg);
}


/* Condition, for instance for shut-down */
void
pan_ns_await_size(int n)
{
    pan_mutex_lock(ns_lock);
    while (bindings->used_bindings != n) {
	pan_cond_wait(ns_changed);
    }
    pan_mutex_unlock(ns_lock);
}


/* Find out who will be the name server.
 * This routine must be called at start-up time, when there is only
 * one thread at this platform.
 * Init locks for the name server:
 * 1. a lock to ensure exclusive access to the name server data structures
 * 2. a lock to ensure there is only one name service client per platform.
 */
void
pan_ns_init(void)
{
    pan_pset_p  the_world = pan_pset_create();

    ns_me = pan_my_pid();
    pan_pset_fill(the_world);
    ns_pid = pan_pset_find(the_world, 0);
    /* ns_pid now is the lowest-numbered platform in the world */

    ns_map = pan_mp_register_map();

    ns_lock       = pan_mutex_create();
    ns_changed    = pan_cond_create(ns_lock);
    if (ns_pid == ns_me) {		/* I become the name server */
	bindings      = gidname_bind_create(16);
	pan_mp_register_async_receive(ns_map, ns_server_upcall);
    }

    ns_send_lock = pan_mutex_create();

    pan_pset_clear(the_world);
}


void
pan_ns_clear(boolean await_unregisters)
{
    if (ns_pid == ns_me) {
	if (await_unregisters) {
	    pan_ns_await_size(0);
	}
    }

    pan_mp_clear_map(ns_map, pan_msg_clear);
    pan_mp_free_map(ns_map);

    pan_mutex_clear(ns_send_lock);

    pan_cond_clear(ns_changed);
    pan_mutex_clear(ns_lock);
    if (ns_pid == ns_me) {
	gidname_bind_clear(bindings);
    }
}




void
pan_ns_statistics(void)
{
#ifdef STATISTICS
    if (ns_pid == ns_me) {
	printf("%2d: name sv server: %8s %8s %8s\n", ns_me,
		"repl/hm", "repl/out", "requests");
	printf("%2d: msgs            %8d %8d %8d\n", ns_me,
		ns_home_replies, ns_outside_replies, ns_home_requests);
    } else {
	printf("%2d: name sv client: %8s\n", ns_me, "requests");
	printf("%2d: msgs            %8d\n", ns_me, ns_outside_requests);
    }
#endif
}


#endif		/* NAME_SERVER_THREAD */
