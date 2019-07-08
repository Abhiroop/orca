/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_nsap.h"
#include "pan_fragment.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <signal.h>
#include <siginfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <string.h>

#include "pan_trace.h"
#include "pan_threads.h"

static struct sockaddr_in       ucast_addr, mcast_addr;
static struct sockaddr_in      *pan_addr;
static int                      ucast_rcv_so, mcast_rcv_so, snd_so;
static mutex_t                  comm_d_lock;

static int                      communication_finished;
static thread_t                 ucast_daemon, mcast_daemon;
static struct pan_thread        pan_ucast_daemon, pan_mcast_daemon;

#define SHARED_NAME             "panda_adm"

#define LIN_BACKOFF   100
#define EXP_BACKOFF   600


static trc_event_t  trc_start_upcall;
static trc_event_t  trc_end_upcall;


#ifdef STATISTICS
static int n_upcalls         = 0;
static int n_unicasts        = 0;
static int n_multicasts      = 0;
static int n_uni_packets     = 0;
static int n_multi_packets   = 0;

static int n_upc_small       = 0;
static int n_uni_rcv_small   = 0;
static int n_multi_rcv_small = 0;
static int n_uni_small       = 0;
static int n_multi_small     = 0;
#endif


typedef void *(*sol_thr_p)(void *);

static void comm_daemon(void *arg);
static void sys_stat_init(void);
static void sys_stat_end(void);

static void
backoff(int restart)
{
    static int sleep_time;

    if (restart) {
	sleep_time = 0;
	return;
    }

    (void)sleep(sleep_time);

    if (sleep_time < LIN_BACKOFF) {
	sleep_time++;
    } else if (sleep_time < EXP_BACKOFF) {
	sleep_time *= 2;
    }
}


static void
write_mcast_addr(struct sockaddr_in *mcast_addr)
{
    FILE *shared_adm;

    if((shared_adm = fopen(SHARED_NAME, "w")) == NULL) {
	pan_panic("cannot open shared administration file");
    }
    
    if(fprintf(shared_adm, "%d %s\n", mcast_addr->sin_port, 
	       inet_ntoa(mcast_addr->sin_addr)) == EOF) { 
	pan_panic("cannot put mcast port number");
    }
    
    if(fclose(shared_adm) == EOF) {
	pan_panic("cannot close shared administration file");
    }
}


static void
read_mcast_addr(struct sockaddr_in *mcast_addr)
{
    FILE *shared_adm;
    char name[256];
    int port;

    /* Read mcast port and mcast address */
    backoff(1);
    while((shared_adm = fopen(SHARED_NAME, "r")) == NULL) {
	backoff(0);
    }
    backoff(1);
    while(fscanf(shared_adm, "%d %s", &port, name) != 2){
	backoff(0);
	fclose(shared_adm);
	if ((shared_adm = fopen(SHARED_NAME, "r")) == NULL){
	    pan_panic("reopen adm. file");
	}
    }

    mcast_addr->sin_family = AF_INET;
    mcast_addr->sin_port = port;
    mcast_addr->sin_addr.s_addr = inet_addr(name);
    
    if(fclose(shared_adm) == EOF) {
	pan_panic("cannot close shared administration file");
    }
}


/* Puts platform address in administration file:
      <platform id, port, IP address>
*/
static void
write_ucast_address(struct sockaddr_in *ucast_addr)
{
    FILE *shared_adm;
    int   cnt, c;
 
    /* write internet address and port to file */
    /* avoid overwrites by having each process write at the appropiate place */
    backoff(1);
    for (;;) {
 
        if ((shared_adm = fopen(SHARED_NAME, "r+")) == NULL) {
            pan_panic("cannot open adm. file");
        }
        for ( cnt = 0; (c = fgetc( shared_adm)) != EOF;)
                if ( c == '\n')
                        cnt++;
        if ( cnt == pan_my_pid() + 1)
                break;
 
        if (fclose(shared_adm) == EOF) {
            pan_panic("cannot close shared administration file");
        }
        backoff(0);
    }

    if (fprintf(shared_adm, "%d %d %s\n", pan_sys_pid, ucast_addr->sin_port,
		inet_ntoa(ucast_addr->sin_addr)) == EOF){
	pan_panic("cannot write own address");
    }
    if (fclose(shared_adm) == EOF) {
	pan_panic("cannot close shared administration file");
    }
}


static void
read_ucast_adresses(struct sockaddr_in *remote_addrs)
{
    FILE *shared_adm;
    char name[256];
    int i, pid, port, dummy;

    /* use sin_port as boolean for filled in entry. */
    for(i = 0; i < pan_sys_nr; i++){
	remote_addrs[i].sin_port = 0;
    }

    /* Read all addresses of other hosts */

    backoff(1);
    for(i = 0; i < pan_sys_nr; ){
	if((shared_adm = fopen(SHARED_NAME, "r")) == NULL){
	    pan_panic("cannot open adm. file");
	}

	if (fscanf(shared_adm, "%d %s", &dummy, name) != 2){
	    pan_panic("cannot skip first entry");
	}

	while (fscanf(shared_adm, "%d %d %s\n", &pid, &port, name) == 3){
	    if (remote_addrs[pid].sin_port == 0){
		remote_addrs[pid].sin_port   = port;
		remote_addrs[pid].sin_family = AF_INET;
		remote_addrs[pid].sin_addr.s_addr = inet_addr(name);
		i++;
	    }
	}

	if (fclose(shared_adm) == EOF) {
	    pan_panic("cannot close shared administration file");
	}

	if (i < pan_sys_nr) {
	    backoff(0);
	}
    }
}


static void
bind_socket_to_port(int rcv_socket, int port)
{
    struct sockaddr_in rcv_addr;
    int sockopt = 1;

    rcv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rcv_addr.sin_family = AF_INET;
    rcv_addr.sin_port = port;

    if (setsockopt(rcv_socket, SOL_SOCKET, SO_REUSEADDR,
		   (char *) &sockopt, sizeof(int)) == -1) {
	pan_panic("setsockopt failed");
    }

    if (bind(rcv_socket, (struct sockaddr *) &rcv_addr, sizeof(rcv_addr)) < 0){
	pan_panic("bind");
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
    struct ip_mreq mreq;
    int addr_len = sizeof(struct sockaddr_in);
    char name[256];
    struct hostent *this_host;
    int gethostname(char *name, int namelen);

    /* SEND SOCKET */

    sys_stat_init();
    if ((snd_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        pan_panic("cannot create send socket");
    }

    
    /* MULTICAST RECEIVE SOCKET */

    if (pan_sys_pid == 0) {
	/* Create port and broadcast address */
	srand(getpid());
	mcast_addr.sin_family = AF_INET;
	mcast_addr.sin_port   = (rand() | IPPORT_USERRESERVED) & 0xFFFF;
	mcast_addr.sin_addr.s_addr = 
	  (unsigned long)(((rand() + 2) & 0xEFFFFFFF) |
			  0xE0000000);

	write_mcast_addr(&mcast_addr);
	/* printf("MCAST Addr written\n"); */
    } else {
	read_mcast_addr(&mcast_addr);
    }

    /* initialise multicast receive socket (add mcast address to it) */
    if ((mcast_rcv_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
	pan_panic("cannot create mcast receive socket");
    }

    mreq.imr_multiaddr = mcast_addr.sin_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(mcast_rcv_so, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, 
		   sizeof(struct ip_mreq)) == -1){
	pan_panic("cannot add broadcast membership");
    }
    bind_socket_to_port(mcast_rcv_so, mcast_addr.sin_port);


    /* UNICAST RECEIVE SOCKET */

    if ((ucast_rcv_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
	pan_panic("cannot create ucast receive socket");
    }

    /* get internet address of this host */
    if (gethostname(name, sizeof(name)) == -1) {
	pan_panic("gethostname failed");
    }

    this_host = gethostbyname(name);
    if (!this_host) {
	pan_panic("gethostbyname failed\n");
    }

    /* create address; system will fill in the port */
    memset(&ucast_addr, 0, sizeof(struct sockaddr_in));
    memcpy(&ucast_addr.sin_addr, this_host->h_addr, this_host->h_length);
    ucast_addr.sin_family = AF_INET;
    ucast_addr.sin_port = 0;
    if (bind(ucast_rcv_so, (struct sockaddr *) &ucast_addr,
             sizeof(ucast_addr)) == -1) {
	pan_panic("bind failed");
    }
    if (getsockname(ucast_rcv_so, (struct sockaddr *) &ucast_addr,
                    &addr_len) == -1) {
	pan_panic("getsockname failed\n");
    }

    write_ucast_address(&ucast_addr);
    pan_addr = (struct sockaddr_in *)pan_malloc(pan_sys_nr * 
						sizeof(struct sockaddr_in));
    assert(pan_addr);
    read_ucast_adresses(pan_addr);


    /* RECEIVE DAEMONS */

    mutex_init(&comm_d_lock, USYNC_THREAD, NULL);
    communication_finished = FALSE;

    /* Don't use bound threads for daemon because Solaris then (apparently)
     * issues a kernel call for each (user-level) context switch :-(
     * In the case of unbound threads, the Solaris thread scheduler deals
     * efficiently with threads issueing blocking system calls.
     */
    if (thr_create(NULL, 0L, (sol_thr_p)comm_daemon, (void *)&ucast_rcv_so,
		   0, &ucast_daemon) != 0){
	fprintf(stderr, "Cannot create ucast daemon\n");
	exit(1);
    }
    if (thr_create(NULL, 0L, (sol_thr_p)comm_daemon, (void *)&mcast_rcv_so,
		   0, &mcast_daemon) != 0){
	fprintf(stderr, "Cannot create mcast daemon\n");
	exit(1);
    }
    if (thr_setprio(ucast_daemon, 101) != 0){
	fprintf(stderr, "Cannot setprio ucast daemon\n");
	exit(1);
    }
    if (thr_setprio(mcast_daemon, 101) != 0){
	fprintf(stderr, "Cannot setprio mcast daemon\n");
	exit(1);
    }
}



void
pan_sys_comm_end(void)
{
    communication_finished = TRUE;

    if (thr_kill(ucast_daemon, SIGUSR1) != 0){
	fprintf(stderr, "thr kill ucast daemon failed\n");
	exit(1);
    }
    if (thr_kill(mcast_daemon, SIGUSR1) != 0){
	fprintf(stderr, "thr kill mcast daemon failed\n");
	exit(1);
    }

    if (thr_join(ucast_daemon, NULL, NULL) != 0){
	fprintf(stderr, "Cannot join daemon thread ucast\n");
	exit(1);
    }
    if (thr_join(mcast_daemon, NULL, NULL) != 0){
	fprintf(stderr, "Cannot join daemon thread mcast\n");
	exit(1);
    }

    close(snd_so);
    close(ucast_rcv_so);
    close(mcast_rcv_so);

    mutex_destroy(&comm_d_lock);

    pan_free(pan_addr);

    if (pan_sys_pid == 0){
	unlink(SHARED_NAME);
    }
    sys_stat_end();
}


				/* RECEIVE */


static void
handle_packet(int so, pan_fragment_p fragment, int rlen)
{
    pan_nsap_p nsap;

#ifdef NO
    /* Bad connection */
    if (rand() % 100 < 30) {
	printf("Removed\n");
	return;
    }
#endif

    assert(rlen != -1);

    nsap = pan_sys_fragment_nsap_pop(fragment, rlen);

    trc_event(trc_start_upcall, &nsap);

    if (nsap->type == PAN_NSAP_SMALL){
	assert(fragment->size == nsap->data_len);

#ifdef STATISTICS
	if (so == ucast_rcv_so) {
	    ++n_uni_rcv_small;
	} else {
	    ++n_multi_rcv_small;
	}
	++n_upc_small;
#endif		/* STATISTICS */

	/* Lifetime of data is lifetime of upcall */
	nsap->rec_small(fragment->data);
	
    } else {

#ifdef STATISTICS
	if (so == ucast_rcv_so) {
	    ++n_uni_packets;
	} else {
	    ++n_multi_packets;
	}
	++n_upcalls;
#endif		/* STATISTICS */

	/* Upcall to fragment handler. */
	nsap->rec_frag(fragment);
    }

    trc_event(trc_end_upcall, &nsap);
}

static void
handler(void)
{
}

static void
handler2(int signal, siginfo_t *info, void *p)
{
}
    
static void
comm_daemon(void *arg)
{
    int            rlen;
    int            fromlen;
    pan_fragment_p fragment;
    int            socket = *(int *)arg;
    struct sigaction  vec;
    
    trc_new_thread( 0, "communication daemon");

    if (socket == ucast_rcv_so){
	pan_sys_thread_upgrade(&pan_ucast_daemon);
    }else{
	assert(socket == mcast_rcv_so);
	pan_sys_thread_upgrade(&pan_mcast_daemon);
    }

    /* XXX: should go to handler (SA_SIGINFO is cleared), but instead
     * goes to handler2. Furthermore, recvfrom doesn't return -1, but 0,
     * and it doesn't set errno to EINTR. 
     */
    vec.sa_handler = handler;
    vec.sa_sigaction = handler2;
    sigemptyset(&vec.sa_mask);
    vec.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &vec, NULL) == -1){
	perror("sigvec");
	exit(1);
    }

    fragment = pan_fragment_create();
    pan_sys_pool_mark(fragment, EXTERNAL_ENTRY);
    
    while (! communication_finished) {
	rlen = recvfrom(socket, fragment->data, PACKET_SIZE, 0,
			NULL, &fromlen);
	if (rlen > 0){
	    mutex_lock(&comm_d_lock);
	    handle_packet(socket, fragment, rlen);
	    mutex_unlock(&comm_d_lock);
	}
    }

    printf("%d: communication finished\n", pan_sys_pid);
    pan_sys_pool_mark(fragment, OUT_POOL_ENTRY);
    pan_fragment_clear(fragment);

    thr_exit(NULL);;
}


				/* SEND */
static void
unicast(int pid, char *data, int size)
{
    int r;

    assert(pid >= 0 && pid < pan_sys_nr);

    do{
	r = sendto(snd_so, data, size, 0, (struct sockaddr *) &(pan_addr[pid]),
                   sizeof(struct sockaddr_in));
    }while ((r == -1) && (errno == EINTR));

    if (r == -1){
	pan_panic("unicast send failed: pid %d address: %s, port: %d",
		  pid, inet_ntoa(pan_addr[pid].sin_addr), 
		  pan_addr[pid].sin_port);
    }
}


static void
broadcast(char *data, int size)
{
    int r;

    do{
	r = sendto(snd_so, data, size, 0, (struct sockaddr *) &mcast_addr, 
		   sizeof mcast_addr);
    }while ((r == -1) && (errno == EINTR));

    if (r == -1){
	pan_panic("multicast send failed: address: %s, port: %d",
		  inet_ntoa(mcast_addr.sin_addr), mcast_addr.sin_port);
    }
}


void
pan_comm_unicast_fragment(int dest, pan_fragment_p fragment)
{
    int size;

    size = pan_sys_fragment_nsap_push(fragment);

#ifdef STATISTICS
    ++n_unicasts;
#endif		/* STATISTICS */

    unicast(dest, fragment->data, size);
}

void
pan_comm_multicast_fragment(pan_pset_p pset, pan_fragment_p fragment)
{
    int size;

    size = pan_sys_fragment_nsap_push(fragment);

#ifdef STATISTICS
    ++n_multicasts;
#endif		/* STATISTICS */

    broadcast(fragment->data, size);
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

#ifdef STATISTICS
    ++n_uni_small;
#endif		/* STATISTICS */

    unicast(dest, small, nsap->data_len + NSAP_HDR_SIZE);
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

#ifdef STATISTICS
    ++n_multi_small;
#endif		/* STATISTICS */
    broadcast(small, nsap->data_len + NSAP_HDR_SIZE);
}


/**************/
/* Statistics */
/**************/

static void
sys_stat_init(void)
{
}

static void
sys_stat_end(void)
{
}


int
sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return (2 + 1 + 1 + 6 + 5 * (1 + 5) + 1 + 5 + 5 * (1 + 5) + 3) * 2
	/* TIM */ + 100;
#else
    return 0;
#endif
}

void
sys_sprint_comm_stats(char *buf)
{
#ifdef STATISTICS
    sprintf(buf, "%2d:frgmnt %5s %5s %5s %5s %5s small %5s %5s %5s %5s %5s\n",
	    pan_my_pid(),
	    "snd/u", "snd/m", "rcv/u", "rcv/m", "upcll",
	    "snd/u", "snd/m", "rcv/u", "rcv/m", "upcll");
    buf = strchr(buf, '\0');
    sprintf(buf, "%2d:comm.d %5d %5d %5d %5d %5d       %5d %5d %5d %5d %5d\n",
	    pan_my_pid(),
	    n_unicasts, n_multicasts, n_uni_packets, n_multi_packets, n_upcalls,
	    n_uni_small, n_multi_small, n_uni_rcv_small, n_multi_rcv_small,
	    n_upc_small);
#endif
}
