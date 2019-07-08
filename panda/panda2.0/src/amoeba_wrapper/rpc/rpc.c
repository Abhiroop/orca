#include <unistd.h>
#include <amoeba.h>
#include <module/ar.h>
#include <module/rpc.h>
#include <module/rnd.h>
#include "module/mutex.h"		/* use optimized version */
#include <stderr.h>
#include <assert.h>

#include "pan_sys.h"
#include "pan_sys_amoeba_wrapper.h"
#include "pan_rpc.h"

typedef struct pan_upcall {
#ifndef NDEBUG
	pan_mutex_p lock;
	pan_cond_p  cond;
#else
	mutex       lock;
#endif
	pan_msg_p   reply;
} pan_upcall_t;

/* often used amoeba header fields */
#define  COM               h_command
#define  LEN               h_size
#define  ADR               h_port

/* commands to a rpc daemon */
#define  STOP              1
#define  UPCALL            2
#define  LARGE_RPC         3
#define  UNIQ_PORT         4
#define  LARGE_REPLY       5

#define  START_NR_DAEMONS  5
#define  INC_NR_DAEMONS    3

static pan_rpc_handler_f request_handler;
static port *rpc_ports;
static pan_thread_p *rpc_daemons;
static pan_mutex_p lock;
static int nr_daemons, busy_daemons;

#ifdef KOEN
static pan_time_p start, stop, total;
static int nr_timings = 0;
static int valid = 0;
#endif


static void add_daemons(int number);

static void getportcap(char *name, port *p)
{
    capability *cap;

    if ((cap = (capability *) getcap(name)) == NULL) {
        sys_panic("getcap failed");
    }
    *p = cap->cap_port;
}


static void make_hdr(header *hdr, int com, port *dest, int len)
{
   hdr->COM = com;
   hdr->ADR = *dest;
   hdr->LEN = len;
}


static void rpc_comm_daemon(void *arg)
{
    pan_msg_p request;
    pan_upcall_t upcall;
    header hdr;
    int ret, len;
    void *buffer;
    port my_get_port, my_put_port;
    int nr_platforms = pan_nr_platforms();
#ifdef TRACING
    int me = (int) arg;
    char buf[30];
#endif

#ifdef TRACING
    (void) sprintf(buf, "rpc daemon %d", me);
    trc_new_thread(0, buf);
#endif

    uniqport( &my_get_port);
    priv2pub( &my_get_port, &my_put_port);

#ifndef NDEBUG
    upcall.lock = pan_mutex_create();
    upcall.cond = pan_cond_create( upcall.lock);
#else
    mu_init(&upcall.lock);
    mu_lock(&upcall.lock);
#endif
    request = pan_msg_create();
    for (;;) {
       len = SYS_DEFAULT_MESSAGE_SIZE;
       buffer = pan_msg_push(request, len, 1);

       /* listen to your getport */
       hdr.ADR = rpc_ports[nr_platforms];

       ret = rpc_getreq(&hdr, buffer, len);
#ifdef KOEN
        pan_time_get(start);
        valid = 1;
#endif

       if (ERR_STATUS(ret)) {
           sys_panic("rpc_getreq failed: %s", err_why(ERR_CONVERT(ret)));
       }

       if ( hdr.COM == LARGE_RPC) {
	   /* This message is just a header specifying the length of the
	    * data to be send in a next message. Send acknowledgement back
	    * holding a unique port where I will be listening for the large
	    * data message.
	    */
	   len = hdr.LEN;
	   hdr.COM = UNIQ_PORT;
           rpc_putrep(&hdr, (char *) &my_put_port, sizeof(port));

      	   pan_msg_empty(request);
           buffer = pan_msg_push(request, len, 1);
           hdr.ADR = my_get_port;
           ret = rpc_getreq(&hdr, buffer, len);

           if (ERR_STATUS(ret)) {
               sys_panic("rpc_getreq failed: %s", err_why(ERR_CONVERT(ret)));
           }
       }

       if (hdr.COM == STOP) {
           rpc_putrep(&hdr, (char *) 0, 0);
    	   pan_msg_clear(request);
	   pan_thread_exit();
	   return;
       } else {
	   assert( hdr.COM == UPCALL);
           pan_mutex_lock(lock);
           if (++busy_daemons == nr_daemons) {
               add_daemons(INC_NR_DAEMONS);
#ifdef VERBOSE
               Printf("Adding daemons\n");
#endif
           }
           pan_mutex_unlock(lock);


#ifndef NDEBUG
           pan_msg_truncate(request, ret);
           upcall.reply = NULL;
#endif
           request_handler(&upcall, request);

	   /* wait for asynchronous reply, to be send back by this thread
	    */
#ifndef NDEBUG
	   pan_mutex_lock(upcall.lock);
	   while (upcall.reply == NULL) {
		pan_cond_wait( upcall.cond);
	   }
	   pan_mutex_unlock(upcall.lock);
#else
	   mu_lock( &upcall.lock);
#endif

	   len = pan_msg_data_len(upcall.reply);
	   if ( len > SYS_DEFAULT_MESSAGE_SIZE) {
Printf( "hmm, reply too large (%d bytes)\n", len);
		hdr.COM = LARGE_REPLY;
		hdr.LEN = len;
		rpc_putrep(&hdr, (char *) &my_put_port, sizeof( port));
		hdr.ADR = my_get_port;
		rpc_getreq(&hdr, NULL, 0);
	   }
#ifdef KOEN
     if (valid) {
        pan_time_get(stop);
        pan_time_sub(stop, start);
        pan_time_add(total,stop);
        nr_timings++;
        valid = 0;
     }
#endif
           rpc_putrep(&hdr, (char *) pan_msg_look(upcall.reply,len,1), len);

	   /* recycle reply message for the next rpc request */
	   request = upcall.reply;
	   pan_msg_empty( request);

           pan_mutex_lock(lock);
           --busy_daemons;
           pan_mutex_unlock(lock);
       }
    }
}


static void add_daemons(int number)

{
    int i;

    if (nr_daemons == 0)
        rpc_daemons = (pan_thread_p *)sys_malloc(number * sizeof(pan_thread_p));
    else
        rpc_daemons = (pan_thread_p *) sys_realloc(rpc_daemons,
                      		(nr_daemons + number) * sizeof(pan_thread_p));

    for (i = nr_daemons; i < nr_daemons + number; i++) {
         rpc_daemons[i] = pan_thread_create(rpc_comm_daemon,
                           (void *) i, 0, pan_thread_maxprio() - 2, 0);
    }
    nr_daemons += number;
}



void pan_rpc_init(void)
{
    int i;
    char name[10];

#ifdef KOEN
    start = pan_time_create();
    stop = pan_time_create();
    total = pan_time_create();
    pan_time_set(total, 0, 0);
#endif
    rpc_ports = (port *) sys_malloc((pan_nr_platforms() + 1) * sizeof(port));
    for (i = 0; i < pan_nr_platforms(); i++) {
         (void) sprintf((char *) &name, "%s%d", "MEMBER", i);
         getportcap(name, (port *) &rpc_ports[i]);
         if (i == pan_my_pid())
             rpc_ports[pan_nr_platforms()] = rpc_ports[i];
         priv2pub(&rpc_ports[i], &rpc_ports[i]);
    }

    lock = pan_mutex_create();

    pan_mutex_lock(lock);
    add_daemons(START_NR_DAEMONS);
    pan_mutex_unlock(lock);

}


void pan_rpc_end(void)
{
    header hdr;
    int    i, ret;

#ifdef KOEN
    if (nr_timings>0)
        printf( "%d: total response time %f in %d ticks, per message %f\n",
                pan_my_pid(), pan_time_t2d(total), nr_timings,
                pan_time_t2d(total)/nr_timings);
    pan_time_clear(start);
    pan_time_clear(stop);
    pan_time_clear(total);
#endif
    for (i = 0; i < nr_daemons; i++) {
         make_hdr(&hdr, STOP, &rpc_ports[pan_my_pid()], 0);
         ret = rpc_trans(&hdr, (char *) 0, 0, &hdr, (char *) 0, 0);
         if (ERR_STATUS(ret)) {
             sys_panic("rpc_trans failed: %s", err_why(ERR_CONVERT(ret)));
         }
    }

    for (i = 0; i < nr_daemons; i++) {
         pan_thread_join(rpc_daemons[i]);
    }

    free(rpc_daemons);
    free(rpc_ports);

    pan_mutex_clear(lock);

}


void pan_rpc_register(pan_rpc_handler_f handler)
{
    /* install RPC message handlers */
    assert (!request_handler);
    request_handler = handler;
}


void pan_rpc_trans(int dest, pan_msg_p request, pan_msg_p *reply)
{
    header hdr;
    int    ret;
    void  *buffer;
    int    buf_len;
    char  *data;
    port svr_port, *addr;
    int len = pan_msg_data_len(request);

    assert((dest >= 0) && (dest < pan_nr_platforms()));

#ifndef NDEBUG
    if (pan_my_pid() == dest) {
        Printf( "pan_rpc_trans() to own platform??\n");
    }
#endif

    *reply = pan_msg_create();
    buf_len = SYS_DEFAULT_MESSAGE_SIZE;
    buffer = pan_msg_push(*reply, buf_len, 1);
    addr = &rpc_ports[dest];

    if (len > SYS_DEFAULT_MESSAGE_SIZE) {
	/* Special protocol for large messages: notify destination of data size
	 * and record to which port the message may be send.
	 */
    	for (;;) {
           make_hdr(&hdr, LARGE_RPC, addr, len);
           ret = rpc_trans(&hdr, NULL, 0,
			   &hdr, (char *) &svr_port, sizeof(port));
           if (ERR_CONVERT(ret) == RPC_NOTFOUND) {
             Printf("server of pid %d not found\n", dest);
             sleep(2);
           }
    	   else if (ERR_STATUS(ret)) {
        	sys_panic("rpc_trans failed: %s", err_why(ERR_CONVERT(ret)));
    	   }
	   else {
	     break;
           }
    	}
	assert( hdr.COM == UNIQ_PORT);
	addr = &svr_port;
    }

    data = (char *) pan_msg_look(request, len, 1);
    for (;;) {
         make_hdr(&hdr, UPCALL, addr, len);
#ifdef VERBOSE
         Printf("trying trans %d bytes to %d\n", len, dest);
#endif
#ifdef KOEN
     if (valid) {
        pan_time_get(stop);
        pan_time_sub(stop, start);
        pan_time_add(total,stop);
        nr_timings++;
        valid = 0;
     }
#endif
         ret = rpc_trans(&hdr, data, len, &hdr, (char *) buffer, buf_len);
#ifdef KOEN
        pan_time_get(start);
        valid = 1;
#endif
         if (ERR_CONVERT(ret) == RPC_NOTFOUND) {
             Printf("server of pid %d not found\n", dest);
             sleep(2);
         }
	 else if (ERR_STATUS(ret)) {
             /* sys_panic("rpc_trans failed: %s", err_why(ERR_CONVERT(ret))); */
             sys_panic("rpc_trans failed: %d", ret);
    	 }
	 else if ( hdr.COM == LARGE_REPLY) {
	     assert( ret == sizeof(port));
	     svr_port = *(port *)buffer;
	     buf_len = hdr.LEN;
	     pan_msg_empty( *reply);
    	     buffer = pan_msg_push(*reply, buf_len, 1);
	     addr = &svr_port;
	     data = NULL;
	     len = 0;
Printf( "oops large reply (%d bytes) from %d\n", buf_len, dest);
	 }
	 else
	     break;
    }

#ifdef VERBOSE
    Printf("truncated reply to %d bytes\n", ret);
#endif
#ifndef NDEBUG
    pan_msg_truncate(*reply, ret);
#endif
}


void pan_rpc_reply(pan_upcall_p upcall, pan_msg_p reply)
{
#ifndef NDEBUG
    pan_mutex_lock(upcall->lock);
    upcall->reply = reply;
    pan_cond_broadcast( upcall->cond);
    pan_mutex_unlock(upcall->lock);
#else
    upcall->reply = reply;
    mu_unlock(&upcall->lock);
#endif
}
