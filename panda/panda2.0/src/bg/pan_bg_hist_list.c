/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_hist_list.h"
#include "pan_bg_alloc.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_ack.h"

#include <stdio.h>

/*
   hist_list manages a circular buffer of fragments that may be asked for
   by other platforms for retransmission. hist_end stops after all
   history fragments are removed.
*/

#define STOP_FLUSHES      1	/* nr flushes without confirms at end XXX: was 10 */
#define SEND_FLUSH        25	/* nr fragments in history before stop */

#define HISTORY_SIZE      30 

typedef struct{
    seqno_t           seqno;
    pan_fragment_p    frag;
}hist_t, *hist_p;

static hist_t         hist[HISTORY_SIZE];
static int            nr;
static seqno_t        head, tail;

#define INDEX(x)   ((x) % HISTORY_SIZE)

typedef struct{
    seqno_t tail;
}flush_hdr_t, *flush_hdr_p;


/* flush request message */
static flush_hdr_t flush_header_data;
static flush_hdr_p flush_header = &flush_header_data;

/* network service access points */
static pan_nsap_p  flush_nsap;	/* flush nsap */

/*
 * send_flush:
 *                 Broadcasts a flush request message.
 */

static void
send_flush(void)
{
    pan_bg_warning("Send flush request; size: %d (%d, %d)\n", nr, head, tail);

    flush_header->tail = tail;
    pan_comm_multicast_small(pan_bg_all, flush_nsap, (char *)flush_header);
}
    

static void
flush_handler(void *data)
{
    flush_hdr_p hdr = (flush_hdr_p)data;

    /* global synchronization */
    pan_mutex_lock(pan_bg_lock);

    if (pan_bg_ackno <= hdr->tail + 5) ack_flush();

    pan_mutex_unlock(pan_bg_lock);
}    



void
hist_start(void)
{
    if (pan_my_pid() == 0){
	head = tail = SEQNO_START;
	nr = 0;
    }

    flush_nsap = pan_nsap_create();
    pan_nsap_small(flush_nsap, flush_handler, sizeof(flush_hdr_t),
		   PAN_NSAP_MULTICAST);
}


void
hist_end(void)
{
    int count = STOP_FLUSHES;
    pan_time_p now, inc;
    pan_cond_p cond;

    now = pan_time_create();
    inc = pan_time_create();
    cond = pan_cond_create(pan_bg_lock);
    
    if (pan_my_pid() == 0){
	pan_time_set(inc, 1, 0);

	while(nr > 0 && count-- > 0){ /* XXX: should be nr > 0? */
	    /* send poll message about every second */
	    pan_time_get(now);
	    pan_time_add(now, inc);

	    if (pan_cond_timedwait(cond, now) == 0){
		printf("Send final flush message\n");
		send_flush();
	    }
	}
	
	if (nr != 0){
	    pan_bg_warning("History list not empty: %d\n", nr);
	}
	hist_confirm(head);
    }else{
	/* Termination problems, as always */
	pan_time_set(inc, 3L, 0L);
	pan_time_get(now);
	pan_time_add(now, inc);
	(void)pan_cond_timedwait(cond, now);
    }

    pan_time_clear(now);
    pan_time_clear(inc);
    pan_cond_clear(cond);

    pan_nsap_clear(flush_nsap);
}

/*
 * hist_add:
 *                 Add a copy of the fragment to the hist list. Return 0
 *                 if the history is full, 1 otherwise.
 */

int
hist_add(seqno_t seqno, pan_fragment_p fragment)
{
    pan_fragment_p copy;

    assert (pan_bg_seqno == head);

    if (nr >= SEND_FLUSH){
	/* Acknowledge sequencer, returns min_ackno */
	hist_confirm(ack_check(0, pan_bg_seqno - 1));
    }

    if (nr >= SEND_FLUSH){
	send_flush();
    }

    if (nr >= HISTORY_SIZE) return 0;

    /* Preallocate fragments? */
    copy = pan_fragment_create();
    assert(copy);
    pan_fragment_copy(fragment, copy, 1);

    hist[INDEX(head)].seqno = pan_bg_seqno;
    hist[INDEX(head)].frag  = copy;
    head++;
    nr++;

    return 1;
}

/*
 * hist_find:
 *                 Tries to find the fragment with sequence number seqno
 *                 in the list. If it is not found, it returns 0; else
 *                 the parameters are filled in and it returns 1.
 */

int
hist_find(seqno_t seqno, pan_fragment_p *fragment)
{
    if (seqno < tail)  return 0;
    if (seqno >= head) return 0;

    assert(hist[INDEX(seqno)].frag);
    *fragment = hist[INDEX(seqno)].frag;

    return 1;
}

/*
 * hist_seqno:
 *                 Finds the sequence number of a message with key
 *                 (pid,index).
 */

int
hist_seqno(int pid, seqno_t index, pan_fragment_p *fragment)
{
    pan_bg_hdr_p hdr;
    unsigned i;

    for(i = tail; i < head; i++){
	hdr = pan_fragment_header(hist[INDEX(i)].frag);
	if (hdr->index == index && hdr->pid == pid){
	    /* found it */
	    *fragment = hist[INDEX(i)].frag;
	    return 1;
	}
    }

    return 0;
}
    
/*
   hist_confirm:
                   Confirms the global receipt of all fragments with
                   sequence number less than seqno. All those
                   fragments are removed from the history list.
*/

void
hist_confirm(seqno_t seqno)
{
    assert(seqno <= head);

    for(; tail < seqno; tail++){
	hist[INDEX(tail)].seqno = -1;
	pan_fragment_clear(hist[INDEX(tail)].frag);
	hist[INDEX(tail)].frag = (void *)-1; /*NULL;*/

	nr--;
    }
    assert(head - tail == nr);
}

    
