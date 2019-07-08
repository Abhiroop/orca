#include <assert.h>
#include <stdio.h>
#include "pan_sys.h"
#include "forward.h"

typedef struct{
    int seqno;
}header_t, *header_p;

typedef struct{
    int ackno;
}ack_hdr_t, *ack_hdr_p;

				/* Global system level objects */
static pan_msg_p     message;       /* Message that is received         */
static pan_mutex_p   ack_mutex;     /* Acknowledge mutex                */
static pan_cond_p    ack_cond;      /* Acknowledge condition variable   */
static pan_nsap_p    msg_nsap;      /* Destination of messages          */
static pan_nsap_p    ack_nsap;      /* Destination of acknowledgements  */

				/* Global variables */
static int           ackno;         /* Last acknowledgement received    */
static int           snd_seqno;     /* First sequence number to send    */
static int           rcv_seqno;     /* First sequence number to receive */


static pan_time_p    now;
static pan_time_p    timeout;

/*#define DEBUG */
#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

static pan_send_t 
frag_handler(void *header, int data, int flags)
{
    header_p hdr = (header_p)header;
    int ret;

    dprintf(("frag handler called\n"));

    pan_time_get(now);
    pan_time_add(now, now, timeout);

    /* Note that an ack can be received before the upcall is made */
    pan_mutex_lock(ack_mutex);
    if (ackno < snd_seqno){
	ret = pan_cond_timedwait(ack_cond, ack_mutex, now);
    }
    pan_mutex_unlock(ack_mutex);

    if (ret){
	/* acknowledgement received */
	hdr->seqno = snd_seqno++;
	return PAN_NEXT_FRAGMENT;
    }else{
	/* retransmit fragment */
	return PAN_RETR_FRAGMENT;
    }
}

static void 
send_ack(int seqno)
{
    ack_hdr_t hdr;
    
    dprintf(("Send ack: %d\n", seqno));

    hdr.ackno = seqno + 1;
    pan_send(ack_nsap, NULL, (void *)&hdr, sizeof(ack_hdr_t), NULL, 0);
}    

static pan_msg_p 
receive_fragment(void *header, int flags)
{
    header_p hdr = (header_p)header;
    
    if (hdr->seqno <= rcv_seqno){
        send_ack(hdr->seqno);
    }
    
    if (hdr->seqno != rcv_seqno){
	/* Discard this fragment */
        return NULL;
    }
    
    rcv_seqno++;
    
    if (flags & PAN_FRAGMENT_FIRST){
        message = pan_msg_init();
    }
    
    return message;
}

static void 
receive_message(pan_msg_p message, void *hdr)
{
    /* Normally, some kind of upcall would be made here */
    pan_msg_clear(message);

    dprintf(("Message received\n"));
}

static pan_msg_p 
receive_ack_frag(void *header, int flags)
{
    ack_hdr_p hdr = (ack_hdr_p)header;
    
    /* Don't handle fragmented acknowledgements */
    assert(flags & PAN_FRAGMENT_LAST);

    pan_mutex_lock(ack_mutex);
    if (hdr->ackno > ackno){
        ackno = hdr->ackno;
	pan_cond_signal(ack_cond);
    }
    pan_mutex_unlock(ack_mutex);

    /* Discard the fragment */
    return NULL;
}


void 
pan_saw_send(pan_msg_p message)
{
    header_t header;

    dprintf(("pan_saw_send called\n"));

    header.seqno = snd_seqno++;
    pan_send(msg_nsap, message, (void *)&header, sizeof(header_t),
	     frag_handler, 0);
    /* Blocked until the message is sent */
}

void
pan_saw_start(void)
{
    ack_mutex = pan_mutex_init();
    ack_cond  = pan_cond_init();

    now     = pan_time_init();
    timeout = pan_time_init();
    pan_time_set(timeout, 1L, 0L);

    msg_nsap = pan_nsap_init();
    pan_nsap_receive(msg_nsap, receive_fragment, 
		     receive_message, sizeof(header_t));
    pan_nsap_send(msg_nsap, PAN_UNICAST);
    pan_nsap_change(msg_nsap, pan_my_pid() == 1 ? 0 : 1);

    ack_nsap = pan_nsap_init();
    pan_nsap_receive(ack_nsap, receive_ack_frag, NULL, sizeof(ack_hdr_t));
    pan_nsap_send(ack_nsap, PAN_UNICAST);
    pan_nsap_change(ack_nsap, pan_my_pid() == 1 ? 0 : 1);
}

void
pan_saw_end(void)
{
    pan_nsap_clear(msg_nsap);
    pan_nsap_clear(ack_nsap);

    pan_time_clear(timeout);
    pan_mutex_clear(ack_mutex);
    pan_cond_clear(ack_cond);
}

/*
Local variables:
compile-command: "make"
End:
*/
