/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 30, 1995
 *
 * Unreliable message passing interface.
 *            Implements unreliable message passing with Panda
 *            primitives. For multifragment messages, we use the MP and
 *            group modules of Panda.
 */

#include "communication.h"	/* Part of the communication module */
#include "pan_sys.h"
#include "pan_module.h"
#ifndef USE_BG
#include "pan_group.h"
#else
#include "pan_bg.h"
#endif
#include "pan_mp.h"
#include "util.h"
#include "panda_message.h"
#include "tpool.h"
#include "sync.h"

#include <assert.h>

struct mp_channel_s {
    pan_pset_p members; 

    /* Map management */
    int        id;
    int        used;
};

static int me;
static int grp_size;

static int initialized;

static mp_channel_p *map;
static int map_nr, map_size, map_active;
static pan_mutex_p map_lock;
static pan_cond_p map_cond;

static pan_sap_p nsap;

static rts_tpool_p pool;

static int grp_port;
static int mp_port;

/*
 * receive:
 *                 Receive a fragment and put a complete message in the
 *                 thread pool queue.
 */
static int
receive(void *data, int size, int len)
{
    mp_channel_p ch;
    msg_hdr_p hdr;
    message_p m;

    hdr = (msg_hdr_p)data;

    /* Do not accept multicast messages from yourself. Unicasts cannot be
     * sent to self.
     */
    if (hdr->sender == me){
	rts_sync_signal(hdr->sync_id);
	return 1;
    }

    if (hdr->ch_id >= map_nr) {
	rts_warning(WARN_SEM, "%d: message for unregistered mp channel "
		    "discarded: %d\n", me, hdr->ch_id);
	return 0;
    }

    ch = map[hdr->ch_id];

    /* Channel already freed */
    if (!ch->used) {
	rts_warning(WARN_SEM, "%d: message on unused mp channel "
		    "discarded: %d\n", me, ch->id);
	return 0;
    }
    
    /* I don't want it */
    if (!pan_pset_ismember(ch->members, me)){
	return 0;
    }

    m = rts_message_receive(data, size, len);

    rts_tpool_add(pool, m);

    return 1;
}

/*
 * msg_receive:
 *                 Receive routine for multi-fragment messages. Those
 *                 messages are sent with either the MP module or the
 *                 group module.
 */
static int
msg_receive(void *data, int size, int len)
{
    mp_channel_p ch;
    msg_hdr_p hdr;
    message_p m;

    hdr = (msg_hdr_p)data;
    
    /* Do not accept group messages from yourself. Unicasts cannot be
     * sent to self.
     */
    if (hdr->sender == me){
#ifndef USE_BG
	rts_sync_signal(hdr->sync_id);
#endif
	return 1;
    }

    if (hdr->ch_id >= map_nr) {
	rts_warning(WARN_SEM, "%d: message for unregistered mp channel "
		    "discarded: %d\n", me, hdr->ch_id);
	return 0;
    }

    ch = map[hdr->ch_id];

    /* Channel already freed */
    if (!ch->used) {
	rts_warning(WARN_SEM, "%d: message on unused mp channel "
		    "discarded: %d\n", me, ch->id);
	return 0;
    }
    
    /* I don't want it */
    if (!pan_pset_ismember(ch->members, me)){
	return 0; 
    }

    m = rts_message_receive(data, size, len);

    rts_tpool_add(pool, m);

    return 1;
}
 
int 
init_mp_channel(int moi, int gsize, int pdbug)
{
    if (initialized++) return 0;

    me = moi;
    grp_size = gsize;

    map_lock = pan_mutex_create();
    map_cond = pan_cond_create(map_lock);

    pool = rts_tpool_create(DYNAMIC_POOL);

    nsap = pan_sap_create(receive, PAN_SAP_UNICAST | PAN_SAP_MULTICAST);
    rts_message_trailer(pan_sap_trailer(nsap));

#ifndef USE_BG
    grp_port = pan_group_register_port(msg_receive);
    rts_message_trailer(pan_group_trailer());
#else
    grp_port = pan_bg_register_port(msg_receive);
    rts_message_trailer(pan_bg_trailer());
#endif

    mp_port = pan_mp_register_port(msg_receive);
    rts_message_trailer(pan_mp_trailer());

    return 0;
}

int 
finish_mp_channel(void)
{
    int i;

    if (--initialized) return 0;
 
    pan_mutex_lock(map_lock);
    while(map_active) {
	assert(map_active >= 0);
	pan_cond_wait(map_cond);
    }
    pan_mutex_unlock(map_lock);

#ifndef USE_BG
#else
    pan_bg_free_port(grp_port);
#endif

    pan_mp_free_port(mp_port);

    pan_sap_clear(nsap);

    for(i = 0; i < map_nr; i++) {
	assert (map[i]->used == 0);
	pan_free(map[i]);
    }
    if (map) pan_free(map);

    rts_tpool_destroy(pool);

    pan_cond_clear(map_cond);
    pan_mutex_clear(map_lock);

    return 0;
}

static void
increase_map(void)
{
    if (map_nr == map_size) {
	map_size += 10;
	map = pan_realloc(map, map_size * sizeof(mp_channel_p));
    }
}
 
mp_channel_p 
new_mp_channel(set_p members)
{
    mp_channel_p ch;
    int i;

    ch = pan_malloc(sizeof(mp_channel_t));

    ch->members = pan_pset_create();
    /* Convert group set to pset members */
    for(i = 0; i < pan_nr_platforms(); i++) {
	if (is_member(members, i)) pan_pset_add(ch->members, i);
    }

    pan_mutex_lock(map_lock);
    increase_map();
    map[map_nr] = ch;
    ch->id = map_nr++;
    ch->used = 1;
    map_active++;
    pan_mutex_unlock(map_lock);    

    return ch;    
}

int 
free_mp_channel(mp_channel_p ch)
{
    pan_mutex_lock(map_lock);
    pan_pset_clear(ch->members);
    ch->used = 0;
    map_active--;

    pan_cond_signal(map_cond);
    pan_mutex_unlock(map_lock);

    return 0;
}

int 
mp_channel_unicast(mp_channel_p ch, int receiver, message_p m)
{
    int len; 

    assert(ch->used);
    assert(receiver != me);

    len = rts_message_length(m);
    m->hdr->ch_id = ch->id;
    m->hdr->sender = me;

#ifdef PAN_UNICAST_PACKET
    if (len < PAN_UNICAST_PACKET - pan_sap_trailer(nsap)){
	pan_unicast(receiver, nsap, m->data, len);
    }else{
	/* Sent multifragment messages with the MP module */
	int ticket;

	ticket = pan_mp_send(receiver, mp_port, m->data, len, PAN_MP_NORMAL);
	pan_mp_finish_send(ticket);
    }
#else
    pan_unicast(receiver, nsap, m->data, len);
#endif

    return 0;
}

int 
mp_channel_multicast(mp_channel_p ch, message_p m)
{
    unsigned short sid;
    int len;
    int nmem;

    assert(ch->used);

    nmem = pan_pset_nr_members(ch->members);
    if (nmem == 1 && pan_pset_ismember(ch->members, me)) {
	return 0;
    }

    len = rts_message_length(m);
    m->hdr->ch_id = ch->id;
    m->hdr->sender = me;
    m->hdr->type = TYPE_MSG;    

#ifdef PAN_MULTICAST_PACKET
    if (len < PAN_MULTICAST_PACKET - pan_sap_trailer(nsap)){
	sid = rts_sync_get();
	m->hdr->sync_id = sid;
	pan_multicast(ch->members, nsap, m->data, len);
	rts_sync_wait(sid);
    }else{
#ifndef USE_BG	
	sid = rts_sync_get();
	m->hdr->sync_id = sid;
	pan_group_send(grp_port, m->data, len);
	rts_sync_wait(sid);
#else
	int ticket;

	ticket = pan_bg_send(grp_port, m->data, len);
	pan_bg_finish_send(ticket);
#endif
    }
#else
    sid = rts_sync_get();
    m->hdr->sync_id = sid;
    pan_multicast(ch->members, nsap, m->data, len);
    rts_sync_wait(sid);
#endif

    return 0;
}

