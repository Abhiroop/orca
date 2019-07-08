#include <assert.h>
#include <stdio.h>
#include "pan_sys.h"
#include "slid.h"


/*
 * Sliding window protocol between a sender (pid 0) and a receiver (pid
 * 1). Tests the retransmission of fragments.
 */

#define WINDOW_SIZE 3
#define INDEX(x)    ((x) % WINDOW_SIZE)

#define MIN(x, y)   ((x) < (y) ? (x) : (y))


typedef struct{
    int seqno;
}header_t, *header_p;

typedef enum {
    ACK, NACK
}ack_type_t;

typedef struct{
    ack_type_t type;
    int        ackno;
}ack_hdr_t, *ack_hdr_p;


typedef struct{
    pan_fragment_p fragment;
    int            used;
}frag_info_t, *frag_info_p;

				/* Global system level objects */
static pan_msg_p     message;	/* Message that is received         */
static pan_mutex_p   mutex;     /* Acknowledge mutex                */
static pan_cond_p    cond;      /* Acknowledge condition variable   */
static pan_nsap_p    msg_nsap;	/* Destination of messages          */
static pan_nsap_p    ack_nsap;      /* Destination of acknowledgements  */

				/* Global variables */
static int     ackno;		/* First seqno not acknowledged    */
static int     snd_seqno;	/* First sequence number to send    */
static int     rcv_seqno;	/* First sequence number to receive */
static int     last_ackno = -1;	/* Last ackno send                  */
static int     last_seen;	/* Last fragment has been received  */


static frag_info_t window[WINDOW_SIZE];


static void        (*upcall)(pan_msg_p msg);	/* Upcall handler */

static pan_time_p    now;
static pan_time_p    timeout;

/*#define DEBUG 2*/
#ifdef DEBUG
#define dprintf(l, x) if (DEBUG >= (l)) printf x
#else
#define dprintf(l, x)
#endif

static void 
receive_ack(void *header)
{
    ack_hdr_p hdr = (ack_hdr_p)header;
    int i; 

    pan_mutex_lock(mutex);

    switch(hdr->type){
    case ACK:
	dprintf(2, ("ack: %d\n", hdr->ackno));
    
	/* Shift sending window */
	if (hdr->ackno >= ackno){
	    dprintf(2, ("clear sending window: %d %d\n", ackno, 
			hdr->ackno));

	    for(i = ackno; i <= hdr->ackno; i++){
		pan_fragment_clear(window[INDEX(i)].fragment);
		window[INDEX(i)].used = 0;
	    }

	    ackno = hdr->ackno + 1;
	}

	if (ackno < snd_seqno){
	    dprintf(1, ("Retransmit after too low ackno: %d %d\n", ackno, 
		     snd_seqno));
	    assert(window[INDEX(ackno)].used == 1);
	    pan_comm_unicast_fragment(1 - pan_my_pid(), 
				      window[INDEX(ackno)].fragment);
	}else{
	    pan_cond_signal(cond);
	}

	break;
    case NACK:
	dprintf(1, ("nack: %d\n", hdr->ackno));

	if (hdr->ackno < ackno){
	    printf("Request for acknowledged message discarded: %d %d\n",
		   hdr->ackno, ackno);
	    break;
	}

	/* Shift sending window */
	if (hdr->ackno > ackno){
	    dprintf(1, ("clear sending window on nack: %d %d\n",
			ackno, hdr->ackno));

	    for(i = ackno; i <= hdr->ackno - 1; i++){
		pan_fragment_clear(window[INDEX(i)].fragment);
		window[INDEX(i)].used = 0;
	    }

	    ackno = hdr->ackno;
	}

	dprintf(1, ("Retransmit %d %d %d\n", hdr->ackno, snd_seqno, ackno));
	assert(window[INDEX(hdr->ackno)].used == 1);
	pan_comm_unicast_fragment(1 - pan_my_pid(),
				  window[INDEX(hdr->ackno)].fragment);

	if (ackno == snd_seqno){
	    pan_cond_signal(cond);
	}

	break;
    }

    pan_mutex_unlock(mutex);
}


static void 
send_ack(int seqno)
{
    ack_hdr_t hdr;
    
    dprintf(2, ("Send ack: %d\n", seqno));

    hdr.type = ACK;
    if (seqno > last_ackno){
	last_ackno = seqno;
    }
    hdr.ackno = seqno;
    pan_comm_unicast_small(1 - pan_my_pid(), ack_nsap, (void *)&hdr);
}    


static void 
send_nack(int seqno)
{
    ack_hdr_t hdr;
    
    dprintf(2, ("Send nack: %d\n", seqno));

    hdr.type = NACK;
    hdr.ackno = seqno;
    pan_comm_unicast_small(1 - pan_my_pid(), ack_nsap, (void *)&hdr);
}    

static void
receive_fragment(pan_fragment_p fragment)
{
    header_p hdr = pan_fragment_header(fragment);
    int flags = pan_fragment_flags(fragment);

    pan_mutex_lock(mutex);

    dprintf(2, ("receive fragment: %d %d\n", hdr->seqno, rcv_seqno));

    if (hdr->seqno < rcv_seqno){
	printf("Received old fragment: %d %d\n", hdr->seqno, rcv_seqno);
	send_ack(rcv_seqno - 1);
	pan_mutex_unlock(mutex);
	return;
    }else if (hdr->seqno > (rcv_seqno + WINDOW_SIZE)){
	printf("Discarded overflow fragment: %d %d\n", hdr->seqno, 
	       rcv_seqno);
	send_nack(rcv_seqno);
	pan_mutex_unlock(mutex);
	return;
    }else if (window[INDEX(hdr->seqno)].used == 1){
	printf("Discard already received fragment: %d\n", hdr->seqno);
	if (rcv_seqno > hdr->seqno){
	    send_ack(rcv_seqno - 1);
	}else{
	    send_nack(rcv_seqno);
	}
	pan_mutex_unlock(mutex);
	return;
    }

    /* Put fragment in receive window */
    window[INDEX(hdr->seqno)].fragment = pan_fragment_create();
    pan_fragment_copy(fragment, window[INDEX(hdr->seqno)].fragment, 0);
    window[INDEX(hdr->seqno)].used     = 1;

    /* TODO: This doesn't look correct */
    if (flags & PAN_FRAGMENT_FIRST){
	last_seen = 0;
	if (hdr->seqno == rcv_seqno){
	    message = pan_msg_create();
	}
    }
    if (flags & PAN_FRAGMENT_LAST){
	last_seen = 1;
    }
    
    if (hdr->seqno != rcv_seqno){
	send_nack(rcv_seqno);
	pan_mutex_unlock(mutex);
	return;
    }

    /* Acknowledge receiving window */
    if ((rcv_seqno == last_ackno + WINDOW_SIZE) || last_seen){
	send_ack(rcv_seqno);
    }
    
    do{
	dprintf(2, ("Assembling fragment: %d\n", rcv_seqno));
	pan_msg_assemble(message, window[INDEX(rcv_seqno)].fragment, 0);
	pan_fragment_clear(window[INDEX(rcv_seqno)].fragment);
	window[INDEX(rcv_seqno)].used = 0;
	
	if (flags & PAN_FRAGMENT_LAST){
	    if (upcall){
		upcall(message);
		rcv_seqno++;
		break;
	    }else{
		fprintf(stderr, "Message discarded\n");
		pan_msg_clear(message);
	    }
	}
	rcv_seqno++;
    }while(window[INDEX(rcv_seqno)].used);

    dprintf(2, ("rcv_seqno: %d\n", rcv_seqno));

    pan_mutex_unlock(mutex);
}

void 
pan_saw_send(pan_msg_p message)
{
    pan_fragment_p fragment;
    header_p header;
    int ret;

    dprintf(2, ("pan_saw_send called\n"));

    fragment = pan_msg_fragment(message, msg_nsap);

    pan_mutex_lock(mutex);

    do{
	/* Store window information */
	assert(window[INDEX(snd_seqno)].used == 0);
	window[INDEX(snd_seqno)].fragment = pan_fragment_create();
	pan_fragment_copy(fragment, window[INDEX(snd_seqno)].fragment, 0);
	pan_fragment_nsap(window[INDEX(snd_seqno)].fragment, msg_nsap);
	window[INDEX(snd_seqno)].used = 1;

	header = pan_fragment_header(window[INDEX(snd_seqno)].fragment);
	header->seqno = snd_seqno;

	dprintf(2, ( "Sending fragment: %d\n", snd_seqno));
	pan_comm_unicast_fragment(1 - pan_my_pid(), 
				  window[INDEX(snd_seqno)].fragment);

	snd_seqno++;

	while (ackno <= (snd_seqno - WINDOW_SIZE)){
	    pan_time_get(now);
	    pan_time_add(now, timeout);
	    
	    ret = pan_cond_timedwait(cond, now);
	    
	    if (!ret){
		printf("Retransmit fragment: %d %d\n", snd_seqno, ackno);
		pan_comm_unicast_fragment(1 - pan_my_pid(),
					  window[INDEX(ackno)].fragment);
	    }
	}

    }while(pan_msg_next(message));
    
    /* Wait until the complete message is received */
    while (ackno < snd_seqno){
	pan_time_get(now);
	pan_time_add(now, timeout);
	
	dprintf(1, ("Waiting for final ackno: %d %d\n", ackno, snd_seqno));
	ret = pan_cond_timedwait(cond, now);
	
	if (!ret){
	    dprintf(1, ("Send tail fragment: %d %d\n", snd_seqno, ackno));
	    pan_comm_unicast_fragment(1 - pan_my_pid(),
				      window[INDEX(ackno)].fragment);
	}
    }

    pan_mutex_unlock(mutex);
}

void
pan_saw_start(void)
{
    mutex = pan_mutex_create();
    cond  = pan_cond_create(mutex);

    now     = pan_time_create();
    timeout = pan_time_create();
    pan_time_set(timeout, 1L, 0L);

    msg_nsap = pan_nsap_create();
    pan_nsap_fragment(msg_nsap, receive_fragment, sizeof(header_t),
		      PAN_NSAP_UNICAST);

    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, receive_ack, sizeof(ack_hdr_t), PAN_NSAP_UNICAST);
}

void 
pan_saw_register(void (*func)(pan_msg_p msg))
{
    upcall = func;
}

void
pan_saw_end(void)
{
    pan_nsap_clear(msg_nsap);
    pan_nsap_clear(ack_nsap);

    pan_time_clear(timeout);
    pan_mutex_clear(mutex);
    pan_cond_clear(cond);
}

/*
Local variables:
compile-command: "make"
End:
*/
