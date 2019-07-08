/* Based on code by Marco Oey.
 * Modifications by Koen to avoid the copying of ethernet packets when
 * receiving large messages.
 *
 * ------------------------------------
 *
 * Removed Marco's lookup mechanism which performed a linear search on
 * a list of rawflip addresses to find the sender of a message. All
 * nodes now store their platform id in the a_space field of their
 * rawflip unicast addresses. The multicast address is given space id
 * pan_sys_nr to distinguish multicasts from unicasts (0 <= space id <
 * pan_sys_nr). Since flip_oneway preserves the space id byte in the
 * addresses it crypts, receivers can find the sender of a message by
 * checking the a_space field of the source address.
 *
 * Raoul
 */

#include "pan_sys.h"		/* Provides a system interface */

#include "pan_sync.h"		/* Use fast macros in system layer */
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_threads.h"
#include "pan_nsap.h"
#include "pan_fragment.h"

#include "pan_trace.h"

#include <amoeba.h>
#include <semaphore.h>
#include <protocols/flip.h>
#include <module/rawflip.h>
#include <limits.h>
#include <string.h>
#include <stderr.h>


#define LTIME             10000     /* msec */
#define ONE_PACKET	   1400	    /* max size returned by rawflip downcall */
#define DAEMON_STACK_SIZE 65536     /* bytes */


typedef struct entry {		/* structure that holds partial msg */
    struct entry *next;
    int        key;		/* identification: msg-id + source-id */
    int        size;		/* length of partial message */
    char      *ptr;		/* position of expected fragment */
    pan_fragment_p frgm;	/* fragment to which the data buffer belongs */
				/* Field frgm added RFHH */
} entry_t, *entry_p;


#define getkey(a)  ( ( (a)->rf_messid << 8 ) | (a)->rf_src.a_space )


static entry_p free_list, fragment_list;

static adr_t        *pan_addr;
static int          ifno, ucast_ep, mcast_ep;
static pan_thread_p network_daemon;

static trc_event_t  trc_start_upcall;
static trc_event_t  trc_end_upcall;


#ifdef BROADCAST_SKIP_SRC
/*
 * Producer-consumer buffer between multicast routines and local_upcall().
 */

struct buf_entry {
    pan_nsap_p      be_nsap;      /* pointer to destination nsap */
    char           *be_buffer;
    pan_fragment_p  be_frag;
};

#define MAX_LOCAL_SIZE      8       /* messages */

static pan_mutex_p      local_buffer_lock, upcall_lock;
static semaphore        num_empty, num_local;
static int              local_in, local_out;
static struct buf_entry local_buffer[MAX_LOCAL_SIZE];
static pan_thread_p     local_daemon;
#endif


#ifdef STATISTICS
static int uni_no_deliver   = 0;
static int multi_no_deliver = 0;
static int uni_nopackets    = 0;
static int multi_nopackets  = 0;
static pan_mutex_p statist_lock;

static int n_uni_packets    = 0;
static int n_multi_packets  = 0;
static int n_upcalls        = 0;
static int n_unicasts       = 0;
static int n_multicasts     = 0;
#endif

#ifdef KOEN
pan_time_p up_start, up_stop, up_total;
int nr_up;

pan_time_p raoul_down_start, raoul_down_stop, raoul_down_total;
int raoul_nr_down;

static pan_time_p start, stop, total;
static int nr_timings = 0;
static int valid = 0;
#endif



/*************************************************/
/* Wrapper routines around rawflip() system call */
/*************************************************/

static int
rawflip_init(void)
{
   struct rawflip_parms args;
   args.rf_opcode = RF_OPC_INIT;
   rawflip(&args);
   return args.rf_if;
}


static int
rawflip_end(int ifno)
{
   struct rawflip_parms args;
   args.rf_opcode = RF_OPC_END;
   args.rf_if = ifno;
   rawflip(&args);
   return args.rf_result;
}


static int
rawflip_register(int ifno, adr_p adr)
{
   struct rawflip_parms args;
   args.rf_opcode = RF_OPC_REGISTER;
   args.rf_if = ifno;
   args.rf_src = *adr;
   rawflip(&args);
   return args.rf_ep;
}


static int
rawflip_unregister(int ifno, int ep)
{
   struct rawflip_parms args;
   args.rf_opcode = RF_OPC_UNREGISTER;
   args.rf_if = ifno;
   args.rf_ep = ep;
   rawflip(&args);
   return args.rf_result;
}


static int
rawflip_unicast(int       ifno,
		char     *pkt,
		int       flags,
		adr_p     dst,
		int       ep,
		f_size_t  length,
		interval  ltime)
{
    struct rawflip_parms args;

    args.rf_opcode = RF_OPC_UNICAST;
    args.rf_if = ifno;
    args.rf_ep = ep;
    args.rf_pkt = pkt;
    args.rf_len = length;
    args.rf_dst = *dst;
    args.rf_flags = flags;
    assert(flags & FLIP_SYNC);
    args.rf_ltime = ltime;
#ifdef KOEN
    pan_time_get(raoul_down_stop);
    pan_time_sub(raoul_down_stop, raoul_down_start);
    pan_time_add(raoul_down_total, raoul_down_stop);
    raoul_nr_down++;

/*
    if (valid) {
    	pan_time_get(stop);
    	pan_time_sub(stop, start);
    	pan_time_add(total,stop);
    	nr_timings++;
	valid = 0;
    }
*/
#endif
    rawflip(&args);
    assert(args.rf_flags & FLIP_SYNC);
    return args.rf_result;
}


static int
rawflip_multicast(int       ifno,
		  char     *pkt,
		  int       flags,
		  adr_p     dst,
		  int       ep,
		  f_size_t  length,
		  int       n,
		  interval  ltime)
{
   struct rawflip_parms args;

   args.rf_opcode = RF_OPC_MULTICAST;
   args.rf_if = ifno;
   args.rf_ep = ep;
   args.rf_pkt = pkt;
   args.rf_len = length;
   args.rf_dst = *dst;
   args.rf_flags = flags;
   assert(flags & FLIP_SYNC);
   args.rf_ltime = ltime;
   args.rf_n = n;
#ifdef KOEN
    if (valid) {
    	pan_time_get(stop);
    	pan_time_sub(stop, start);
    	pan_time_add(total,stop);
    	nr_timings++;
	valid = 0;
    }
#endif
   rawflip(&args);
   assert(args.rf_flags & FLIP_SYNC);
   return args.rf_result;
}



/**************/
/* Statistics */
/**************/

static void
sys_stat_init(void)
{
#ifdef KOEN
    up_start = pan_time_create();
    up_stop = pan_time_create();
    up_total = pan_time_create();
    pan_time_set(up_total, 0, 0);

    raoul_down_start = pan_time_create();
    raoul_down_stop = pan_time_create();
    raoul_down_total = pan_time_create();
    pan_time_set(raoul_down_total, 0, 0);

/*
    start = pan_time_create();
    stop = pan_time_create();
    total = pan_time_create();
    pan_time_set(total, 0, 0);
*/
#endif
#ifdef STATISTICS
    statist_lock = pan_mutex_create();
#endif
}

static void
sys_stat_end(void)
{
#ifdef KOEN
    if (raoul_nr_down>0)
    	printf( "%d: total downcall time %f in %d ticks, per request %f\n",
    	        pan_sys_pid, pan_time_t2d(raoul_down_total), raoul_nr_down,
	        pan_time_t2d(raoul_down_total)/raoul_nr_down);
    pan_time_clear(raoul_down_start);
    pan_time_clear(raoul_down_stop);
    pan_time_clear(raoul_down_total);

    if (nr_up > 0)
    	printf( "%d: total upcall time %f in %d ticks, per reply %f\n",
    	        pan_sys_pid, pan_time_t2d(up_total), nr_up,
	        pan_time_t2d(up_total)/nr_up);
    pan_time_clear(up_start);
    pan_time_clear(up_stop);
    pan_time_clear(up_total);

/*
    if (nr_timings>0)
    	printf( "%d: total response time %f in %d ticks, per message %f\n",
    	        pan_sys_pid, pan_time_t2d(total), nr_timings,
	        pan_time_t2d(total)/nr_timings);
    pan_time_clear(start);
    pan_time_clear(stop);
    pan_time_clear(total);
*/
#endif
#ifdef STATISTICS
    pan_mutex_clear(statist_lock);
#endif
}


int
sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return (2 + 1 + 1 + 6 + 5 * (1 + 6) + 1 + 6 + 4 * (1 + 6) + 3) * 2;
#else
    return 0;
#endif
}

void
sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
    sprintf(buf, "%2d: packet %6s %6s %6s %6s %6s discrd %6s %6s %6s %6s\n",
	    pan_my_pid(),
	    "send/u", "send/m", "rcv/u", "rcv/m", "upcall",
	    "!dlv/u", "!dlv/m", "!pkt/u", "!pkt/m");
    buf = strchr(buf, '\0');
    sprintf(buf, "%2d: comm.d %6d %6d %6d %6d %6d        %6d %6d %6d %6d\n",
	    pan_my_pid(),
	    n_unicasts, n_multicasts, n_uni_packets, n_multi_packets, n_upcalls,
	    uni_no_deliver, multi_no_deliver, uni_nopackets, multi_nopackets);
#endif
}

static void
sys_print_stats(void)
{
#ifdef STATISTICS
    if (uni_no_deliver > 0 || multi_no_deliver > 0 ||
	uni_nopackets > 0 || multi_nopackets > 0) {
	printf("%2d: discrd %6s %6s %6s %6s\n", pan_my_pid(),
		"!dlv/u", "!dlv/m", "!pkt/u", "!pkt/m");
	printf("%2d: comm.d %6d %6d %6d %6d\n", pan_my_pid(),
		uni_no_deliver, multi_no_deliver,
		uni_nopackets, multi_nopackets);
    }
#endif
}


/*****************************/
/* Fragment storage routines */
/*****************************/

static void
tadd(int key, int size, char *p, pan_fragment_p frgm)	/* frgm added RFHH */
{
    entry_p e;

    if (free_list) {
	e = free_list;
	free_list = free_list->next;
    } else {
	e = (entry_p)pan_malloc(sizeof(entry_t));
    }

    e->key  = key;
    e->size = size;
    e->ptr  = p;
    e->frgm = frgm;

    e->next = fragment_list;
    fragment_list = e;
}


static int
tdel(int key, int *size, char **p, pan_fragment_p *frgm) /* frgm added RFHH */
/* search and delete an entry from the table */
{
    entry_p e, prev;

    for (prev = NULL, e = fragment_list; e; prev = e, e = e->next) {
	if (e->key == key) {
	    *size = e->size;
	    *p    = e->ptr;
	    *frgm = e->frgm;

	    if (prev) {
		prev->next = e->next;
	    } else {
		fragment_list = e->next;
	    }
	    e->next = free_list;
	    free_list = e;

	    return 1;
	}
    }
    return 0;
}


static void
tclear(void)		/* clear the complete queue */
{
    entry_p e, next;

    for (e = fragment_list; e; e = next) {
	next = e->next;
	pan_fragment_clear(e->frgm);	/* change free to fragment_clear RFHH */
	pan_free(e);
    }
    fragment_list = 0;

    for (e = free_list; e; e = next) {
	next = e->next;
					/* Don't clear fragment - free list is
					 * only for t entry structs. */
	pan_free(e);
    }
    free_list = 0;
}


/* Getflip:
 *
 *     Build a rawflip address from the capability associated with
 *     environment variable 'name'.
 */
static void
getflip(char *name, adr_t *flip, int space_id)
{
    capability *cap;

    assert(0 <= space_id && space_id <= UCHAR_MAX);

    if (!(cap = (capability *)getcap(name))) {
	pan_panic("getflip: getcap %s failed\n", name);
    }
    memset(flip, '\0', (size_t)sizeof(adr_t));
    *(port *) &flip->a_abytes[1] = (port)cap->cap_port;
    flip->a_space = (unsigned char)space_id;
}


static void
notdeliver(struct rawflip_parms *args)
{
    int pid = (int)args->rf_dst.a_space;

    if (pid < 0 || pid > pan_sys_nr) {
	pan_panic("notdeliver: packet not delivered: address unknown");
    }

    if (pid == pan_sys_nr) {
#ifndef NOVERBOSE
	printf("%d: warning: could not deliver multicast message\n", 
	       pan_sys_pid);
#endif

#ifdef STATISTICS
	pan_mutex_lock(statist_lock);
	++uni_no_deliver;
	pan_mutex_unlock(statist_lock);
#endif

    } else {

#ifndef NOVERBOSE
	printf("%d: warning: could not deliver unicast message to pid %d\n", 
	       pan_sys_pid, pid);
#endif

#ifdef STATISTICS
	pan_mutex_lock(statist_lock);
	++multi_no_deliver;
	pan_mutex_unlock(statist_lock);
#endif
    }
}

				/* RECEIVE */
static void
comm_daemon(void *arg)
{
    int size = 0, size_;
    pan_fragment_p fragment, fragment_;
    struct rawflip_parms args;
    char *packet, *packet_;
    long key, key_;
    pan_nsap_p nsap;

    trc_new_thread( 0, "communication daemon");

    fragment = pan_fragment_create();
    packet   = fragment->data;
    size = 0;

    key = key_ = 0L;

    /* rawflip info that is fixed until termination out of the
     * main loop.
     */
    args.rf_opcode = RF_OPC_UPCALL;
    args.rf_if = ifno;

    for (;;) {
	args.rf_pkt = packet;
	args.rf_len = PACKET_SIZE - size;

	rawflip(&args);
#ifdef KOEN
	pan_time_get(start);
	valid = 1;
	pan_time_get(up_start);
#endif

#ifdef STATISTICS
	pan_mutex_lock(statist_lock);
	if (args.rf_dst.a_space < pan_sys_nr) {
	    ++n_uni_packets;
	} else {
	    ++n_multi_packets;
	}
	pan_mutex_unlock(statist_lock);
#endif

	switch (args.rf_result) {
	  case UPCALL_RESULT_EXIT:       /* We're done: clean up and quit */
	    pan_fragment_clear(fragment);
	    tclear();
	    pan_thread_exit();
	    break;

	  case UPCALL_RESULT_ABORTED:    /* got signal from gdb */
	    continue;

	  case UPCALL_RESULT_RECEIVED:	 /* see below */
	    break;

	  case UPCALL_RESULT_NDLIVERED:  /* packet not delivered */
	    notdeliver(&args);
	    continue;

	  default:
	    pan_panic("comm_daemon: rawflip result unknown");
	}


	/*
	 * Got a packet. Its offset can be wrong or it may
	 * have been copied into the wrong packet buffer (wrong key).
	 */
	if (args.rf_offset != size || (size>0 && key != getkey(&args))) {
	    if (args.rf_offset == 0) { /* a new fragment */
		/* 
		 * Store the cached fragment buffer into the partial
		 * fragment table, then get a new buffer for the
		 * packet that has just arrived.  
		 */
		tadd(key, size, packet, fragment);

		fragment_ = pan_fragment_create();
		packet_   = fragment_->data;
		memcpy(packet_, packet, args.rf_len);

		size     = args.rf_len;
		packet   = packet_ + size;
		fragment = fragment_;
	    } else {
		/* 
		 * Not a first packet. Try to find its predecessor(s)
		 * in the partial fragment table. If we can find the
		 * predecessor buffer, we must check if the new packet
		 * has the expected offset.
		 */
		key_ = getkey(&args);
		if (!tdel(key_, &size_, &packet_, &fragment_)) {
		    continue; /* no fragment, so ignore */
		}
		if (size_ != args.rf_offset) {
		    pan_fragment_clear(fragment_);
		    continue;
		}


		/*
		 * We found the packet's predecessor(s). Now append
		 * the packet. The fragment to which it belongs is
		 * now made the 'current' fragment. The previous
		 * current fragment is stored in the partial fragment
		 * table.
		 */
		memcpy(packet_, packet, args.rf_len);  /* append */
		if (size > 0) {
		    tadd(key, size, packet, fragment); /* store */
		} else {
		  /* nothing received yet, free packet */
		    assert(size == 0);
		    pan_fragment_clear(fragment);
		}
		size     = size_ + args.rf_len;
		packet   = packet_ + args.rf_len;
		key      = key_;
		fragment = fragment_;
	    }
	} else {
	    size   += args.rf_len;
	    packet += args.rf_len;
	}


	if (size == args.rf_tlen) {
	    /*
	     * A complete fragment has been received: make an upcall.
	     */

#ifdef STATISTICS
	    pan_mutex_lock(statist_lock);
	    n_upcalls++;
	    pan_mutex_unlock(statist_lock);
#endif
	    nsap = pan_sys_fragment_nsap_pop(fragment, size);

#ifdef BROADCAST_SKIP_SRC
	    pan_mutex_lock(upcall_lock);
#endif
	    trc_event(trc_start_upcall, &nsap);
	    if (nsap->type == PAN_NSAP_SMALL){
		/*
		 * Lifetime of data is lifetime of upcall.
		 */
		assert(fragment->size == nsap->data_len);
		nsap->rec_small(fragment->data);
	    } else {
		nsap->rec_frag(fragment);
	    }
	    trc_event(trc_end_upcall, &nsap);
#ifdef BROADCAST_SKIP_SRC
	    pan_mutex_unlock(upcall_lock);
#endif
	    size = 0;
	    packet = fragment->data;	/* Upcall may have swapped data
					 * pointers with another fragment */
	} else {
	    if (size == args.rf_len) {
	    	/* partial packet, save key for checking next fragment */
	    	key = getkey(&args);
	    }

	    if (size + ONE_PACKET > PACKET_SIZE) {
		/*
		 * Appending the next packet may overflow the current
		 * buffer, so we allocate a fresh one and store the old
		 * one in the partial fragment table.
		 */
	    	tadd(key, size, packet, fragment);

		fragment = pan_fragment_create();
		packet   = fragment->data;
	    	size = 0;
	    }
	}
    }
}


#ifdef BROADCAST_SKIP_SRC

/***************************/
/* Local delivery routines */
/***************************/

static void
local_upcall(void *arg)
{
    struct buf_entry *entryp;
    pan_nsap_p nsap;

    for (;;) {
	sema_down(&num_local);          /* wait for a message */

	pan_mutex_lock(local_buffer_lock);
	entryp = &local_buffer[local_out];
	local_out = (local_out + 1) % MAX_LOCAL_SIZE;
	pan_mutex_unlock(local_buffer_lock);

	nsap = entryp->be_nsap;
	if (!nsap) {
	    break;   /* null nsap => quit */
	}

	pan_mutex_lock(upcall_lock);
	if (nsap->type == PAN_NSAP_SMALL){
	    nsap->rec_small(entryp->be_buffer);
	} else {
	    nsap->rec_frag(entryp->be_frag);
	    pan_fragment_clear(entryp->be_frag);
	    entryp->be_frag = 0;
	}
	pan_mutex_unlock(upcall_lock);

	sema_up(&num_empty);
    }
    pan_thread_exit();
}

static void
deliver_small_at_home(pan_nsap_p nsap, char *data, int size)
{
    if (sema_trydown(&num_empty, 0) == 0) {
	pan_mutex_lock(local_buffer_lock);

	local_buffer[local_in].be_nsap = nsap;
	memcpy(local_buffer[local_in].be_buffer, data, size);

	local_in = (local_in + 1) % MAX_LOCAL_SIZE;

	pan_mutex_unlock(local_buffer_lock);
	sema_up(&num_local);
    }
#ifdef COMM_VERBOSE
    else {
	printf("%d: buffer full: dropped small local multicast message\n",
	       pan_sys_pid);
    }
#endif
}

static void
deliver_frag_at_home(pan_nsap_p nsap, pan_fragment_p frag)
{
    pan_fragment_p copy;

    if (sema_trydown(&num_empty, 0) == 0) {
	pan_mutex_lock(local_buffer_lock);

	copy = pan_fragment_create();
	(void)pan_fragment_copy(frag, copy, 1);

	local_buffer[local_in].be_nsap = nsap;
	local_buffer[local_in].be_frag = copy;

	local_in = (local_in + 1) % MAX_LOCAL_SIZE;

	pan_mutex_unlock(local_buffer_lock);
	sema_up(&num_local);
    }
#ifdef COMM_VERBOSE
    else {
	printf("%d: buffer full: dropped local fragment multicast message\n",
	       pan_sys_pid);
    }
#endif
}

static void
start_local_daemon(void)
{
    int i;

    upcall_lock = pan_mutex_create();
    local_buffer_lock = pan_mutex_create();
    sema_init(&num_local, 0);
    sema_init(&num_empty, MAX_LOCAL_SIZE);

    for (i = 0; i < MAX_LOCAL_SIZE; i++) {
	local_buffer[i].be_nsap = 0;
	local_buffer[i].be_frag = 0;
	local_buffer[i].be_buffer = (char *)pan_malloc(MAX_SMALL_SIZE + 
						       NSAP_HDR_SIZE);
    }

    local_daemon = pan_thread_create(local_upcall, (void *)0,
				     DAEMON_STACK_SIZE,
				     pan_thread_maxprio(), 0);
}

static void
kill_local_daemon(void)
{
    int i;

    sema_down(&num_empty);
    pan_mutex_lock(local_buffer_lock);

    local_buffer[local_in].be_nsap = 0;
    local_buffer[local_in].be_frag = 0;
    
    local_in = (local_in + 1) % MAX_LOCAL_SIZE;
    
    pan_mutex_unlock(local_buffer_lock);
    sema_up(&num_local);

    pan_thread_join(local_daemon);
    pan_thread_clear(local_daemon);

    for (i = 0; i < MAX_LOCAL_SIZE; i++) {
	local_buffer[i].be_nsap = 0;
	local_buffer[i].be_frag = 0;
	pan_free(local_buffer[i].be_buffer);
    }
    pan_mutex_clear(upcall_lock);
    pan_mutex_clear(local_buffer_lock);
}

#endif /* BROADCAST_SKIP_SRC */

				/* SEND */
static void
unicast(int pid, char *data, int size)
{
    int ret;

    assert(pid >= 0 && pid < pan_sys_nr);

#ifdef STATISTICS
    pan_mutex_lock(statist_lock);
    ++n_unicasts;
    pan_mutex_unlock(statist_lock);
#endif

    ret = rawflip_unicast(ifno, data, FLIP_SYNC, &pan_addr[pid], ucast_ep,
			  size, LTIME);
    if (ret == FLIP_OK) {
	return;
    }

    if (ret == FLIP_FAIL) {
	pan_panic("unicast: rawflip_unicast to pid %d failed", pid);
    } else if (ret == FLIP_NOPACKET) {
#ifdef STATISTICS
	pan_mutex_lock(statist_lock);
	++uni_nopackets;
	pan_mutex_unlock(statist_lock);
#endif
	return;
    }

    if (ret == FLIP_TIMEOUT) {
#ifndef NOVERBOSE
	printf("%d: warning: unicast failed because of a timeout\n", 
	       pan_sys_pid);
#endif
    } else {
        pan_panic("unicast: unknown rawflip result: %d pid: %d", ret, pid);
    }
}

#ifdef CERIEL
/* Separate unicast for small messages, small time-out. */
static void
small_unicast(int pid, char *data, int size)
{
    int ret;

    assert(pid >= 0 && pid < pan_sys_nr);

#ifdef STATISTICS
    pan_mutex_lock(statist_lock);
    ++n_unicasts;
    pan_mutex_unlock(statist_lock);
#endif

    ret = rawflip_unicast(ifno, data, FLIP_SYNC, &pan_addr[pid], ucast_ep,
			  size, 100);
    if (ret == FLIP_OK) {
	return;
    }

    if (ret == FLIP_FAIL) {
	pan_panic("unicast: rawflip_unicast to pid %d failed", pid);
    } else if (ret == FLIP_NOPACKET) {
#ifdef STATISTICS
	pan_mutex_lock(statist_lock);
	++uni_nopackets;
	pan_mutex_unlock(statist_lock);
#endif
	return;
    }

    if (ret == FLIP_TIMEOUT) {
#ifndef NOVERBOSE
	printf("%d: warning: unicast failed because of a timeout\n", 
	       pan_sys_pid);
#endif
    } else {
        pan_panic("unicast: unknown rawflip result: %d pid: %d", ret, pid);
    }
}
#endif


static void
broadcast(char *data, int size)
{
    int ret;

#ifdef STATISTICS
    pan_mutex_lock(statist_lock);
    ++n_multicasts;
    pan_mutex_unlock(statist_lock);
#endif

#ifdef BROADCAST_SKIP_SRC
    ret = rawflip_multicast(ifno, data, FLIP_SYNC | FLIP_SKIP_SRC, 
			    &pan_addr[pan_sys_nr],
			    ucast_ep, size, pan_sys_nr, LTIME);
					/* Changed mcast_ep to ucast_ep RFHH */
#else
    ret = rawflip_multicast(ifno, data, FLIP_SYNC, &pan_addr[pan_sys_nr],
			    ucast_ep, size, pan_sys_nr, LTIME);
					/* Changed mcast_ep to ucast_ep RFHH */
#endif

    if (ret == FLIP_OK) return;
    
    if (ret == FLIP_FAIL) {
	pan_panic("broadcast: rawflip_multicast failed\n");
    } 

    if (ret == FLIP_NOPACKET) {
#ifdef STATISTICS
        pan_mutex_lock(statist_lock);
	multi_nopackets++;
        pan_mutex_unlock(statist_lock);
#endif
	return;
    }

    if (ret == FLIP_TIMEOUT) {
#ifndef NOVERBOSE
	printf("%d: warning: multicast failed because of a timeout\n",
	       pan_sys_pid);
#endif
    } else {
    	pan_panic("multicast: unknown rawflip result: %d", ret);
    }
}


void
pan_sys_comm_start(void)
{
    trc_start_upcall = trc_new_event(2999, sizeof(pan_nsap_p), "start upcall",
				     "start upcall nsap 0x%p");
    trc_end_upcall   = trc_new_event(2999, sizeof(pan_nsap_p), "end upcall",
				     "end upcall nsap 0x%p");
}


void
pan_sys_comm_wakeup(void)
{
    char ucast_name[10];
    adr_t ucast_addr, mcast_addr;
    int pid;

    sys_stat_init();
    if ((ifno = rawflip_init()) == FLIP_FAIL) {
	pan_panic("pan_sys_comm_wakeup: rawflip_init failed\n");
    }

    /*
     * All nodes build the following flip addresses:
     * 1. a source address (and corresponding endpoint). This identifies
     *    the sender of a message in the rawflip upcall. The space id
     *    is set to 'this_cpu'. This address is different on all nodes.
     *
     * 2. (ncpus - 1) unicast destination addresses. The public
     *    versions of these addresses are stored in array
     *    pan_addr. The private unicast address is the
     *    concatenation of the platform number and the port associated
     *    with the platform's MEMBER environment variable. The public
     *    address is obtained by applying flip_oneway to the private
     *    address.
     *
     * 3. 1 multicast destination addresses. All nodes listen
     *    to this address.
     *
     * Private and public versions of an address always have identical
     * space ids.
     */

    pan_addr = (adr_t *)pan_malloc((pan_sys_nr + 1) * sizeof(adr_t));

    for (pid = 0; pid < pan_sys_nr; pid++) {
	(void)sprintf(ucast_name, "%s%d", "MEMBER", pid);

	getflip(ucast_name, &ucast_addr, pid);     /* private ucast addr */
        flip_oneway(&ucast_addr, &pan_addr[pid]);  /* public ucast addr  */

	if (pid == pan_sys_pid) {
	    ucast_ep = rawflip_register(ifno, &ucast_addr);
	    if (ucast_ep == FLIP_FAIL) {
		pan_panic("pan_sys_comm_wakeup: rawflip_register failed\n");
	    }
	}
    }

    getflip("GROUPCAP", &mcast_addr, pan_sys_nr);
    mcast_ep = rawflip_register(ifno, &mcast_addr);
    if (mcast_ep == FLIP_FAIL){
	pan_panic("pan_sys_comm_wakeup: rawflip_register failed\n");
    }
    flip_oneway(&mcast_addr, &pan_addr[pan_sys_nr]);

    network_daemon = pan_thread_create(comm_daemon, (void *)0,
				       DAEMON_STACK_SIZE,
				       pan_thread_maxprio(), 0);
#ifdef BROADCAST_SKIP_SRC
    start_local_daemon();
#endif
}


void
pan_sys_comm_end(void)
{
    if (rawflip_unregister(ifno, ucast_ep) == FLIP_FAIL) {
	pan_panic("sys_comm_end: rawflip_unregister failed");
    }
    if (rawflip_unregister(ifno, mcast_ep) == FLIP_FAIL) {
	pan_panic("sys_comm_end: rawflip_unregister failed");
    }
    if (rawflip_end(ifno) == FLIP_FAIL) {
        pan_panic("sys_comm_end: rawflip_end failed");
    }

#ifdef BROADCAST_SKIP_SRC
    kill_local_daemon();
#endif
    
    pan_thread_join(network_daemon);
    pan_thread_clear(network_daemon);

    sys_print_stats();
    sys_stat_end();

    pan_free(pan_addr);
}


void
pan_comm_unicast_fragment(int dest, pan_fragment_p fragment)
{
    int size;

    size = pan_sys_fragment_nsap_push(fragment);

    unicast(dest, fragment->data, size);
}

void
pan_comm_multicast_fragment(pan_pset_p pset, pan_fragment_p fragment)
{
    int size;

    size = pan_sys_fragment_nsap_push(fragment);

    broadcast(fragment->data, size);

#ifdef BROADCAST_SKIP_SRC
    if (pan_pset_ismember(pset, pan_sys_pid)) {
	deliver_frag_at_home(fragment->nsap, fragment);
    }
#endif
}

void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
    char small[MAX_SMALL_SIZE + NSAP_HDR_SIZE];

    /*
    * I would like to optimize this routine, by using a different
    * socket for each nsap. It is not necessary then to put the nsap
    * id in front of the message, and so the header doesn't have to
    * be copied. With active messages, this is even easier.
    */
    
    assert(nsap->type == PAN_NSAP_SMALL);
    assert(nsap->data_len <= sizeof(small));

    memcpy(small, data, nsap->data_len);
    *(short *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);
#ifdef CERIEL
    small_unicast(dest, small, nsap->data_len + NSAP_HDR_SIZE);
#else
    unicast(dest, small, nsap->data_len + NSAP_HDR_SIZE);
#endif
}

void
pan_comm_multicast_small(pan_pset_p pset, pan_nsap_p nsap, void *data)
{
    char small[MAX_SMALL_SIZE + NSAP_HDR_SIZE];

    /*
     * I would like to optimize this routine, by using a different
     * socket for each nsap. It is not necessary then to put the nsap
     * id in front of the message, and so the header doesn't have to
     * be copied. With active messages, this is even easier.
     */
    
    assert(nsap->type == PAN_NSAP_SMALL);
    assert(nsap->data_len <= sizeof(small));

    memcpy(small, data, nsap->data_len);
    *(short *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);
    broadcast(small, nsap->data_len + NSAP_HDR_SIZE);

#ifdef BROADCAST_SKIP_SRC
    if (pan_pset_ismember(pset, pan_sys_pid)) {
	deliver_small_at_home(nsap, small, nsap->data_len + NSAP_HDR_SIZE);
    }
#endif
}
