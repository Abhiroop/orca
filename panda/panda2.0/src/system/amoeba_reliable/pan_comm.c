#include <unistd.h>
#include <amoeba.h>
#include <module/ar.h>
#include <stderr.h>
#include <group.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include "pan_sys.h"

#include "pan_message.h"
#include "pan_fragment.h"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sys_amoeba_wrapper.h"

#include "pan_trace.h"

#define   JOIN   1
#define   LEAVE  2
#define   DATA   3
#define   STOP   4
#define   INIT   5

/* often used amoeba header fields */
#define   COM    h_command
#define   PID    h_extra
#define   LEN    h_size
#define   ADR    h_port


/* #define   MAX(x,y)(((x) > (y)) ? (x) : (y)) */




/***************************************************/


static g_id_t       g_id;
static port         bcast_port;
static pan_thread_p bcast_daemon;
static pan_mutex_p  bcast_lock;
static pan_cond_p   bcast_size_changed;

static port        *unicast_port;
static port         get_port;
static pan_thread_p ucast_rcve_thread;

static int          lognbuf;
static int          bb_large = 1250;
static unsigned int sync_timeout = 20000;
static unsigned int send_timeout = 2000;


static trc_event_t  trc_start_upcall;
static trc_event_t  trc_end_upcall;


#ifdef STATISTICS
static pan_mutex_p  statist_lock;

static int n_uni_packets    = 0;
static int n_multi_packets  = 0;
static int n_upcalls        = 0;
static int n_unicasts       = 0;
static int n_multicasts     = 0;
#endif

 
static void
getportcap(char *name, port *p)
{
   capability *cap;
 
   if ((cap = (capability *) getcap(name)) == NULL) {
       sys_panic("getcap failed");
   }
   *p = cap->cap_port;
}


static int
twolog(int n)
{
    int         log;

    for (log = 0; n > 1; log++)
	n >>= 1;
    return log;
}


static void
make_bcast_hdr(header_p hdr, int com, int len)
{
    hdr->COM = com;
    hdr->ADR = bcast_port;
    hdr->PID = pan_my_pid();
    hdr->LEN = len;
}


static int
handle_stop(header_p hdr, pan_fragment_p fragment)
{
    pan_mutex_lock(bcast_lock);
    pan_cond_signal(bcast_size_changed);
    pan_mutex_unlock(bcast_lock);

    return (hdr->PID != pan_my_pid());
}


static void
handle_data(header_p hdr, pan_fragment_p fragment)
{
    pan_nsap_p nsap;

    nsap = pan_sys_fragment_nsap_pop(fragment, hdr->LEN);
    trc_event(trc_start_upcall, &nsap);

    if (nsap->type == PAN_NSAP_SMALL) {
	/* Life time of data is life time of upcall */
	nsap->rec_small(fragment->data);
    } else {
	/* Life time of fragment is life time of ucall */
	nsap->rec_frag(fragment);
    }
    trc_event(trc_end_upcall, &nsap);
}


static void
handle_init(header_p hdr, pan_fragment_p fragment)
{
    pan_mutex_lock(bcast_lock);
    pan_cond_signal(bcast_size_changed);
    pan_mutex_unlock(bcast_lock);
}


static int
dispatch(header_p hdr, pan_fragment_p fragment)
{
    switch (hdr->COM) {
    case DATA:
	handle_data(hdr, fragment);
	break;
    case STOP:
	return handle_stop(hdr, fragment);
    case INIT:
	handle_init(hdr, fragment);
	break;
    default:
	sys_panic("unknown group message");
    }
    return TRUE;
}


static void
bcast_comm_daemon(void *arg)
{
    int              rlen;
    int              more;
    pan_fragment_p   fragment;
    void            *p;
    header           hdr;

    trc_new_thread(0, "bcast daemon");

    fragment = pan_fragment_create();

    for (;;) {
	hdr.ADR = bcast_port;
	p = fragment->data;
	if ((rlen = grp_receive(g_id, &hdr, p, PACKET_SIZE, &more)) < 0) {
	    sys_panic("grp_receive failed: %s", err_why(ERR_CONVERT(rlen)));
	}
	if (hdr.LEN != rlen) {
	    sys_panic("message truncated from %d to %d bytes", hdr.LEN, rlen);
	}
	if (! dispatch(&hdr, fragment)) {
	    pan_fragment_clear(fragment);
	    pan_thread_exit();
	}
    }
}


static void
bcast(void *msg, int data_len)
{
    header      hdr;
    int         ret;

#ifdef VERBOSE
    printf("bcast %d bytes\n", data_len);
#endif

    make_bcast_hdr(&hdr, DATA, data_len);

    ret = grp_send(g_id, &hdr, msg, data_len);

    if (ERR_STATUS(ret)) {
	sys_panic("grp_send failed: %s", err_why(ERR_CONVERT(ret)));
    }
}


void
pan_comm_multicast_fragment(pan_pset_p rcvrs, pan_fragment_p fragment)
{
    int size;
 
    size = pan_sys_fragment_nsap_push(fragment);
 
    bcast(fragment->data, size);
 
#ifdef BROADCAST_SKIP_SRC
    if (pan_pset_ismember(pset, pan_sys_pid)) {
	deliver_frag_at_home(fragment->nsap, fragment);
    }
#endif
}


void
pan_comm_multicast_small(pan_pset_p rcvrs, pan_nsap_p nsap, void *data)
{
    char small[MAX_SMALL_SIZE + NSAP_HDR_SIZE];

    memcpy(small, data, nsap->data_len);
    *(short *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);
    bcast(small, nsap->data_len + NSAP_HDR_SIZE);
 
#ifdef BROADCAST_SKIP_SRC
    if (pan_pset_ismember(pset, pan_sys_pid)) {
	deliver_small_at_home(nsap, small, nsap->data_len + NSAP_HDR_SIZE);
    }
#endif
}


static int
bcast_size(void)
{
    int         ret;
    grpstate_t  grpstate;
    g_indx_t    memlist[256];

    ret = grp_info(g_id, &bcast_port, &grpstate, memlist, 256);
    if (ERR_STATUS(ret)) {
	sys_panic("grp_info failed: %s", err_why(ERR_CONVERT(ret)));
    }
    return grpstate.st_total;
}


static void
create_bcast_channel(void)
{
    capability *cap;
    header      hdr;
    int         ret;

    if ((cap = (capability *)getcap("GROUPCAP")) == NULL) {
	sys_panic("getcap failed");
    }
    bcast_port = cap->cap_port;

    if (pan_my_pid() == 0) {
	lognbuf = twolog(8 * pan_nr_platforms());
	if (lognbuf < 5) lognbuf = 5;
	if ((g_id = grp_create(&bcast_port, 0, MAX(2, pan_nr_platforms()),
			       lognbuf, twolog(2 * PACKET_SIZE - 1))) < 0) {
	    sys_panic("grp_create failed: %s\n", err_why(ERR_CONVERT(g_id)));
	}
    } else {
	sleep(1);
	make_bcast_hdr(&hdr, INIT, 0);
	if ((g_id = grp_join(&hdr)) < 0) {
	    sys_panic("grp_join failed: %s\n", err_why(ERR_CONVERT(g_id)));
	}
    }

    ret = grp_set(g_id, &bcast_port, sync_timeout, send_timeout, 0, bb_large);
    if (ERR_STATUS(ret)) {
	sys_panic("grp_set: %d", err_why(ERR_CONVERT(ret)));
    }
    bcast_lock = pan_mutex_create();
    bcast_size_changed = pan_cond_create(bcast_lock);

    bcast_daemon = pan_thread_create(bcast_comm_daemon, NULL, 0L,
				     pan_thread_maxprio() - 1, 0);

    pan_mutex_lock(bcast_lock);
    while (bcast_size() != pan_nr_platforms())
	pan_cond_wait(bcast_size_changed);
    pan_mutex_unlock(bcast_lock);

#ifdef VERBOSE
    printf("group created and joined by all\n");
#endif
}


static void
clear_bcast_channel(void)
{
    header      hdr;
    int         ret;

    if (pan_my_pid() == 0) {	/* i am the sequencer */
	pan_mutex_lock(bcast_lock);
	while (bcast_size() != 1)
	    pan_cond_wait(bcast_size_changed);
	pan_mutex_unlock(bcast_lock);
    }

    /* send suicide message to the group daemon */
    make_bcast_hdr(&hdr, STOP, 0);
    ret = grp_leave(g_id, &hdr);
    if (ERR_STATUS(ret)) {
	sys_panic("grp_leave failed: %s", err_why(ERR_CONVERT(ret)));
    }

    pan_thread_join(bcast_daemon);
    pan_thread_clear(bcast_daemon);

    pan_cond_clear(bcast_size_changed);
    pan_mutex_clear(bcast_lock);
}


static void
make_unicast_hdr(header_p hdr, int com, int dest, int len)
{
   hdr->COM = com;
   hdr->ADR = unicast_port[dest];
   hdr->PID = pan_my_pid();
   hdr->LEN = len;
}


static void
unicast_comm_daemon(void *arg)
{
    int         rlen;
    pan_fragment_p   fragment;
    void       *p;
    header      hdr;
    header      ack_hdr;

    trc_new_thread(0, "unicast daemon");

    fragment = pan_fragment_create();

    for (;;) {
	p = fragment->data;
	do {
	    hdr.ADR = get_port;
	    rlen = getreq(&hdr, p, PACKET_SIZE);
	} while (ERR_CONVERT(rlen) == RPC_ABORTED);

	if (ERR_STATUS(rlen)) {
	    sys_panic("getreq failed: %s\n", err_why(ERR_CONVERT(rlen)));
	}
	if (hdr.LEN != rlen) {
	    sys_panic("message truncated from %d to %d bytes", hdr.LEN, rlen);
	}

					/* Send ack */
	ack_hdr = hdr;
	ack_hdr.LEN = 0;
	putrep(&ack_hdr, NULL, 0);

	if (hdr.COM == STOP) {
	    assert(hdr.PID == pan_my_pid());
	    pan_fragment_clear(fragment);
	    pan_thread_exit();
	}

	assert(hdr.COM == DATA);
	dispatch(&hdr, fragment);
    }
}


static void
do_unicast(int pid, void *msg, int data_len)
{
    header hdr;
    int    rlen;

    do {
	 make_unicast_hdr(&hdr, DATA, pid, data_len);

	 rlen = trans(&hdr, msg, hdr.LEN, &hdr, (char *) 0, 0);

	 if (ERR_CONVERT(rlen) == RPC_NOTFOUND) {
	     printf("warning: trans to pid %d failed, server not found\n", pid);
	     sleep(2);
	 }
    } while ((ERR_CONVERT(rlen) == RPC_ABORTED) ||
	     (ERR_CONVERT(rlen) == RPC_NOTFOUND));
 
    if (ERR_STATUS(rlen)) {
	sys_panic("unicast pid %d port %s failed: %s",
		  pid,
		  ar_port(&unicast_port[pid]),
		  err_why(ERR_CONVERT(rlen)));
    }
}


typedef struct UCAST_MSG_T ucast_msg_t, *ucast_msg_p;

struct UCAST_MSG_T {
    int           pid;
    int           data_len;
    unsigned int  flags;
    ucast_msg_p   next;
    char          data[PACKET_SIZE];
};


static pan_mutex_p  ucast_send_lock;
static pan_cond_p   ucast_send_nonempty;
static ucast_msg_p  ucast_send_q;
static ucast_msg_p  ucast_free_list;
static pan_thread_p ucast_send_thread;

#define UCAST_STOP	1



static void
clear_ucast_msg(ucast_msg_p msg)
{
    msg->next = ucast_free_list;
    ucast_free_list = msg;
}


static ucast_msg_p
get_ucast_msg(int data_len)
{
    ucast_msg_p msg;

    assert(data_len <= PACKET_SIZE);
    if (ucast_free_list == NULL) {
	msg = pan_malloc(sizeof(ucast_msg_t));
    } else {
	msg = ucast_free_list;
	ucast_free_list = ucast_free_list->next;
    }

    return msg;
}


static void
ucast_send_enq(int pid, void *data, int data_len, unsigned int flags)
{
    ucast_msg_p scan;
    ucast_msg_p msg;

    pan_mutex_lock(ucast_send_lock);

    msg = get_ucast_msg(data_len);

    msg->pid      = pid;
    msg->data_len = data_len;
    msg->flags    = flags;
    msg->next     = NULL;
    memcpy(msg->data, data, data_len);

    if (ucast_send_q == NULL) {
	ucast_send_q = msg;
    } else {
	scan = ucast_send_q;
	while (scan->next != NULL) {
	    scan = scan->next;
	}
	scan->next = msg;
    }
    pan_cond_signal(ucast_send_nonempty);
    pan_mutex_unlock(ucast_send_lock);
}


/*ARGSUSED*/
static void
unicast_send_daemon(void *arg)
{
    ucast_msg_p msg;

    trc_new_thread(0, "ucast send daemon");
    pan_mutex_lock(ucast_send_lock);
    do {
	while (ucast_send_q == NULL) {
	    pan_cond_wait(ucast_send_nonempty);
	}
	msg = ucast_send_q;
	ucast_send_q = ucast_send_q->next;

	if (msg->flags & UCAST_STOP) {
	    break;
	}
	pan_mutex_unlock(ucast_send_lock);

	do_unicast(msg->pid, msg->data, msg->data_len);

	pan_mutex_lock(ucast_send_lock);
	clear_ucast_msg(msg);
    } while (TRUE);
    pan_mutex_unlock(ucast_send_lock);

    pan_thread_exit();
}
	


static void
unicast(int pid, void *msg, int data_len)
{
    if (pan_thread_self() != ucast_rcve_thread) {
	do_unicast(pid, msg, data_len);
    } else {
	ucast_send_enq(pid, msg, data_len, 0);
    }
}


void
pan_comm_unicast_fragment(int dest, pan_fragment_p fragment)
{
    int size;
 
    size = pan_sys_fragment_nsap_push(fragment);
 
    unicast(dest, fragment->data, size);
}


void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
    char small[MAX_SMALL_SIZE + NSAP_HDR_SIZE];

    assert(nsap->type == PAN_NSAP_SMALL);
    assert(nsap->data_len <= sizeof(small));
 
    memcpy(small, data, nsap->data_len);
    *(short *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);
    unicast(dest, small, nsap->data_len + NSAP_HDR_SIZE);
}


static void
create_unicast_channel(void)
{
    int  i;
    char name[10];

    unicast_port = pan_malloc(pan_nr_platforms() * sizeof(port));

    for (i = 0; i < pan_nr_platforms(); i++) {
	sprintf(name, "MEMBER%d", i);
	getportcap(name, &unicast_port[i]);
	if (i == pan_my_pid()) {
	    get_port = unicast_port[i];
	}
	priv2pub(&unicast_port[i], &unicast_port[i]);
    }
    ucast_rcve_thread = pan_thread_create(unicast_comm_daemon, NULL, 0L,
				       pan_thread_maxprio() - 1, 0);

    ucast_send_lock     = pan_mutex_create();
    ucast_send_nonempty = pan_cond_create(ucast_send_lock);
    ucast_send_q        = NULL;
    ucast_send_thread   = pan_thread_create(unicast_send_daemon, NULL, 0L,
					    pan_thread_maxprio() - 1, 0);
}


static void
clear_unicast_channel(void)
{
    header        hdr;
    int           error;
    ucast_msg_p   save;

    make_unicast_hdr(&hdr, STOP, pan_my_pid(), 0);
    error = trans(&hdr, NULL, 0, &hdr, NULL, 0);
    if (ERR_STATUS(error)) {
	sys_panic("suicide trans failed: %s\n", err_why(ERR_CONVERT(error)));
    }

    pan_thread_join(ucast_rcve_thread);
    pan_thread_clear(ucast_rcve_thread);

    ucast_send_enq(pan_my_pid(), NULL, 0, UCAST_STOP);

    pan_thread_join(ucast_send_thread);
    pan_thread_clear(ucast_send_thread);
    pan_cond_clear(ucast_send_nonempty);
    pan_mutex_clear(ucast_send_lock);

    while (ucast_free_list != NULL) {
	save = ucast_free_list;
	ucast_free_list = ucast_free_list->next;
	pan_free(save);
    }

    pan_free(unicast_port);
}



/**************/
/* Statistics */
/**************/

static void
sys_stat_init(void)
{
#ifdef STATISTICS
    statist_lock = pan_mutex_create();
#endif
}

static void
sys_stat_end(void)
{
#ifdef STATISTICS
    pan_mutex_clear(statist_lock);
#endif
}


int
sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return (2 + 1 + 1 + 6 + 5 * (1 + 6) + 1) * 2;
#else
    return 0;
#endif
}

void
sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
    sprintf(buf, "%2d: packet %6s %6s %6s %6s %6s\n",
	    pan_my_pid(),
	    "send/u", "send/m", "rcv/u", "rcv/m", "upcall");
    buf = strchr(buf, '\0');
    sprintf(buf, "%2d: comm.d %6d %6d %6d %6d %6d\n",
	    pan_my_pid(),
	    n_unicasts, n_multicasts, n_uni_packets, n_multi_packets, n_upcalls);
#endif
}

static void
sys_print_stats(void)
{
#ifdef STATISTICS
#endif
}


void
pan_sys_comm_start(void)
{
    trc_start_upcall = trc_new_event(2999, sizeof(pan_nsap_p), "start upcall",
				     "start upcall nsap 0x%p");
    trc_end_upcall   = trc_new_event(2999, sizeof(pan_nsap_p), "end upcall",
				     "end upcall nsap 0x%p");
    sys_stat_init();
}


void
pan_sys_comm_wakeup(void)
{
    create_bcast_channel();
    create_unicast_channel();
}


void
pan_sys_comm_end(void)
{
    clear_unicast_channel();
    clear_bcast_channel();
    sys_print_stats();
    sys_stat_end();
}
