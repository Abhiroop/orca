#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "pan_sys.h"
#include "saw.h"

/*
   Stop and wait protocol between sender (pid 0) and receiver (pid 1).
*/

typedef struct{
    int seqno;
}header_t, *header_p;

typedef struct{
    int ackno;
}ack_hdr_t, *ack_hdr_p;

				/* Global system level objects */
static pan_msg_p     message;	/* Message that is received         */
static pan_mutex_p   ack_mutex;	/* Acknowledge mutex                */
static pan_cond_p    ack_cond;	/* Acknowledge condition variable   */
static pan_nsap_p    msg_nsap;	/* Destination of messages          */
static pan_nsap_p    ack_nsap;	/* Destination of acknowledgements  */

				/* Global variables */
static int           ackno = -1;/* Last acknowledgement received    */
static int           snd_seqno;	/* First sequence number to send    */
static int           rcv_seqno;	/* First sequence number to receive */

static void        (*upcall)(pan_msg_p msg);	/* Upcall handler */

static pan_time_p    now;
static pan_time_p    timeout;

#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

static void 
send_ack(int seqno)
{
    ack_hdr_t hdr;
    
    dprintf(("Send ack: %d\n", seqno));

    hdr.ackno = seqno + 1;
    pan_comm_unicast_small(1 - pan_my_pid(), ack_nsap, (void *)&hdr);
}    

static void
receive_fragment(pan_fragment_p fragment)
{
    header_p hdr = pan_fragment_header(fragment);
    int flags = pan_fragment_flags(fragment);

#ifdef CHECK
    if ((rand() % 128) < 32){
	return;
    }
#endif

    if (hdr->seqno <= rcv_seqno){
        send_ack(hdr->seqno);
    }
    
    if (hdr->seqno != rcv_seqno){
	printf("Discard: %d %d\n",hdr->seqno, rcv_seqno);
        return;
    }
    
    rcv_seqno++;
    
    if (flags & PAN_FRAGMENT_FIRST){
        message = pan_msg_create();
    }
    
    pan_msg_assemble(message, fragment, 0);

    if (flags & PAN_FRAGMENT_LAST){
	if (upcall){
	    upcall(message);
	}else{
	    fprintf(stderr, "Message discarded\n");
	    pan_msg_clear(message);
	}
    }	
}

static void 
receive_ack(void *header)
{
    ack_hdr_p hdr = (ack_hdr_p)header;
    
    pan_mutex_lock(ack_mutex);
    if (hdr->ackno > ackno){
        ackno = hdr->ackno;
	pan_cond_signal(ack_cond);
    }
    pan_mutex_unlock(ack_mutex);
}


void 
pan_saw_send(pan_msg_p message)
{
    pan_fragment_p fragment;
    header_p header;
    int ret;

    dprintf(("pan_saw_send called\n"));

    fragment = pan_msg_fragment(message, msg_nsap);
    
    pan_mutex_lock(ack_mutex);

    do{
	header = (header_p)pan_fragment_header(fragment);

	header->seqno = snd_seqno++;
	do{
	    pan_comm_unicast_fragment(1 - pan_my_pid(), fragment);
	    
	    pan_time_get(now);
	    pan_time_add(now, timeout);
	    
	    ret = pan_cond_timedwait(ack_cond, now);
	    
	    if (!ret){
		printf("Retransmitting fragment: %d %d\n", ackno, snd_seqno);
	    }else{
		assert(ackno == snd_seqno);
	    }
	} while (ackno < snd_seqno);

    }while(pan_msg_next(message));

    pan_mutex_unlock(ack_mutex);
}

void
pan_saw_start(void)
{
    ack_mutex = pan_mutex_create();
    ack_cond  = pan_cond_create(ack_mutex);

    now     = pan_time_create();
    timeout = pan_time_create();
    pan_time_set(timeout, 1L, 0L);

    msg_nsap = pan_nsap_create();
    pan_nsap_fragment(msg_nsap, receive_fragment, sizeof(header_t),
		      PAN_NSAP_UNICAST);

    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, receive_ack, sizeof(ack_hdr_t),
		   PAN_NSAP_UNICAST);
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
    pan_mutex_clear(ack_mutex);
    pan_cond_clear(ack_cond);
}

/*
Local variables:
compile-command: "make"
End:
*/
