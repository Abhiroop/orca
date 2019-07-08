#include <assert.h>
#include <stdio.h>
#include "pan_sys.h"
#include "lat.h"

/*
 * Measures the latency of unicast messages. Timings are done after all
 * messages are sent. Since this layer has to handle the sequence
 * numbers, it is not possible to send any user definied header, because
 * the definition of the header must be given here.
 */

typedef struct{
    int ackno;
}ack_hdr_t, *ack_hdr_p;

				/* Global system level objects */
static pan_mutex_p   ack_mutex;     /* Acknowledge mutex                */
static pan_cond_p    ack_cond;      /* Acknowledge condition variable   */
static pan_nsap_p    msg_nsap;      /* Destination of messages          */
static pan_nsap_p    ack_nsap;      /* Destination of acknowledgements  */

				/* Global variables */
static int           ackno;         /* Last acknowledgement received    */
static int           snd_seqno;     /* First sequence number to send    */
static int           rcv_seqno;     /* First sequence number to receive */

static void        (*upcall)(header_p header);	/* Upcall handler */

static pan_time_p    now;
static pan_time_p    timeout;

/*#define DEBUG*/
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
receive_msg(void *header)
{
    header_p hdr = (header_p)header;

    dprintf(("receive fragment: %d %d\n", hdr->seqno, rcv_seqno));

    if (hdr->seqno <= rcv_seqno){
        send_ack(hdr->seqno);
    }
    
    if (hdr->seqno != rcv_seqno){
	printf("Discard: %d %d\n",hdr->seqno, rcv_seqno);
	/* Discard this fragment */
        return;
    }
    
    rcv_seqno++;
    
    if (upcall){
	upcall((void *)header);
    }else{
	fprintf(stderr, "Small message discarded\n");
    }
}

static void
receive_ack(void *header)
{
    ack_hdr_p hdr = (ack_hdr_p)header;
    
    dprintf(("receive ack fragment: %d %d\n", hdr->ackno, ackno));

    pan_mutex_lock(ack_mutex);
    if (hdr->ackno > ackno){
        ackno = hdr->ackno;
	pan_cond_signal(ack_cond);
    }
    pan_mutex_unlock(ack_mutex);
}


void 
pan_small_send(header_p header)
{
    dprintf(("pan_small_send called\n"));

    header->seqno = snd_seqno++;

    /* Note that an ack can be received before the upcall is made */
    pan_mutex_lock(ack_mutex);
    do{
	pan_comm_unicast_small(1 - pan_my_pid(), msg_nsap, (void *)header);

	pan_time_get(now);
	pan_time_add(now, timeout);

	(void)pan_cond_timedwait(ack_cond, now);
    }while(ackno < snd_seqno);
    pan_mutex_unlock(ack_mutex);
}

void
pan_small_start(void)
{
    ack_mutex = pan_mutex_create();
    ack_cond  = pan_cond_create(ack_mutex);

    now     = pan_time_create();
    timeout = pan_time_create();
    pan_time_set(timeout, 1L, 0L);

    msg_nsap = pan_nsap_create();
    pan_nsap_small(msg_nsap, receive_msg, sizeof(header_t), PAN_NSAP_UNICAST);

    ack_nsap = pan_nsap_create();
    pan_nsap_small(ack_nsap, receive_ack, sizeof(ack_hdr_t), PAN_NSAP_UNICAST);
}

void 
pan_small_register(void (*func)(header_p header))
{
    upcall = func;
}

void
pan_small_end(void)
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
