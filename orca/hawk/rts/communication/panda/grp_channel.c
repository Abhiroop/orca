/*
 * Author:         Tim Ruhl
 *
 * Date:           Dec 1, 1995
 *
 * Group channel module.
 *       Implements reliable group communication on top of Panda.
 */

#include "communication.h"	/* Part of the communication module */
#include "pan_sys.h"
#include "pan_module.h"

#ifndef USE_BG
#include "pan_group.h"
#else
#include "pan_bg.h"
#endif
#include "util.h"
#include "panda_message.h"
#include "tpool.h"
#include "sync.h"

#include <assert.h>
#include <string.h>

struct grp_channel_s {
    int         arrived;

    /* Map management */
    int         id;
    int         used;
    pan_cond_p  cond;
};

static int me;
static int grp_size;
static int initialized;

static int grp_port;

static rts_tpool_p pool;

static pan_mutex_p map_lock;
static pan_cond_p map_cond;
static grp_channel_p *map;
static int map_size;

static int index, active;

/*
 * find_entry:
 *                 Find the group channel entry that corresponds to a
 *                 given entry number. If it doesn't exist yet, create
 *                 it. 
 */
static grp_channel_p
find_entry(int index)
{
    int old_size = map_size;
    grp_channel_p ch;
    int i;

    pan_mutex_lock(map_lock);

    if (index >= map_size) {
        map_size = index + 5;
        map = pan_realloc(map, map_size * sizeof(mp_channel_p));
	for(i = old_size; i < map_size; i++) map[i] = NULL;
    }
    
    if (!map[index]){
	map[index] = ch = pan_malloc(sizeof(grp_channel_t));
	ch->arrived = 0;
	ch->id = index;
	ch->used = 1;
	ch->cond = pan_cond_create(map_lock);
    }else{
	ch = map[index];
    }

    pan_mutex_unlock(map_lock);    

    return ch;
}
 
/*
 * receive:
 *                 Receive a message and put it in the thread pool queue
 *                 associated with the channel it is destinated for. Join
 *                 messages are handled special, they are used in
 *                 new_grp_channel.
 */
static int
receive(void *data, int size, int len)
{
    grp_channel_p ch;
    msg_hdr_p hdr;
    message_p m;
    void *copy;

    hdr = (msg_hdr_p)data;

    if (hdr->type == TYPE_MSG){
	assert(hdr->ch_id < map_size);
	assert(map[hdr->ch_id]);

	ch = map[hdr->ch_id];
	
	/* Channel already freed */
	if (!ch->used) {
	    rts_warning(WARN_SEM, "%d: message on unused grp channel "
			"discarded: %d\n", me, ch->id);

	    assert(hdr->sender != me);
	    return 0;
	}
    
	if (hdr->sender == me){
	    copy = pan_malloc(size);
	    memcpy(copy, data, len);
	    data = copy;

#ifndef USE_BG
	    rts_sync_signal(hdr->sync_id);
#endif
	}

	m = rts_message_receive(data, size, len);
	
	rts_tpool_add(pool, m);

	return 1;
    }else{
	/* Join the grp_channel */
	int cp;
	assert(hdr->type == TYPE_JOIN);
	
	ch = find_entry(hdr->ch_id);

	pan_mutex_lock(map_lock);
	ch->arrived++;
	cp = hdr->sender;
	pan_cond_signal(ch->cond);
	pan_mutex_unlock(map_lock);

	return cp == me;
    }

    /* NOTREACHED */
    return -1;
}



int 
init_grp_channel(int moi, int gsize, int pdebug)
{
    if (initialized++) return 0;

    me = moi;
    grp_size = gsize;

    map_lock = pan_mutex_create();
    map_cond = pan_cond_create(map_lock);

    pool = rts_tpool_create(1); /* One thread, so order is preserved */

#ifndef USE_BG
    grp_port = pan_group_register_port(receive);
    rts_message_trailer(pan_group_trailer());
#else
    grp_port = pan_bg_register_port(receive);
    rts_message_trailer(pan_bg_trailer());
#endif
 
    return 0;
}

int 
finish_grp_channel(void)
{
    int i;
    
    if (--initialized) return 0;
 
    pan_mutex_lock(map_lock);
    while(active) {
        assert(active >= 0);
        pan_cond_wait(map_cond);
    }

    for(i = 0; i < map_size; i++) {
	if (map[i]){
	    assert (map[i]->used == 0);
	    pan_cond_clear(map[i]->cond);
	    pan_free(map[i]);
	    map[i] = NULL;
	}
    }
    if (map) pan_free(map);
    map = NULL;

    pan_mutex_unlock(map_lock);
 
#ifndef USE_BG
#else
    pan_bg_free_port(grp_port);
#endif

    rts_tpool_destroy(pool);

    pan_cond_clear(map_cond);
    pan_mutex_clear(map_lock);
 
    return 0;
}

grp_channel_p 
new_grp_channel(set_p s)
{
    grp_channel_p ch;
    char msg[256];
    msg_hdr_p hdr;
    int i, cnt;

    ch = find_entry(index++);
    active++;

    /* Wait for all platforms to arrive */
    cnt = 0;
    for(i = 0; i < pan_nr_platforms(); i++) {
	if (is_member(s, i)) cnt++;
    }

    /* Only send join message if we are actually a member of this
       group channel.
    */
    if (! is_member(s, me)) return ch;

    hdr = (msg_hdr_p)msg;
    hdr->hdr_size = -1;
    hdr->data_size = -1;
    hdr->ch_id = ch->id;
    hdr->sender = me;
    hdr->type = TYPE_JOIN;

#ifndef USE_BG    
    assert(256 > sizeof(msg_hdr_t) + pan_group_trailer());
    pan_group_send(grp_port, msg, sizeof(msg_hdr_t));
#else
    {
	int ticket;

	assert(256 > sizeof(msg_hdr_t) + pan_bg_trailer());
	ticket = pan_bg_send(grp_port, msg, sizeof(msg_hdr_t));
	pan_bg_finish_send(ticket);
    }
#endif

    pan_mutex_lock(map_lock);
    while(ch->arrived < cnt){
	pan_cond_wait(ch->cond);
    }
    pan_mutex_unlock(map_lock);

    return ch;    
}

int 
free_grp_channel(grp_channel_p ch)
{
    assert(ch->used);

    pan_mutex_lock(map_lock);

    ch->used = 0;
    active--;
 
    pan_cond_signal(map_cond);
    pan_mutex_unlock(map_lock);

    return 0;
}

int 
grp_channel_send(grp_channel_p ch, message_p m)
{
    int len;

    assert(ch->used);
    
    len = rts_message_length(m);
    m->hdr->ch_id = ch->id;
    m->hdr->sender = me;
    m->hdr->type = TYPE_MSG;
    
#ifndef USE_BG
    {
	unsigned short sid;

	sid = rts_sync_get();
	m->hdr->sync_id = sid;
	pan_group_send(grp_port, m->data, len);
	rts_sync_wait(sid);
    }
#else
    {
	int ticket;

	ticket = pan_bg_send(grp_port, m->data, len);
	pan_bg_finish_send(ticket);
    }
#endif
    
    return 0;
}

