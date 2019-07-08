#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_system.h"
#include "pan_nsap.h"
#include "pan_fragment.h"
#include "pan_time.h"

#include <pthread.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#include <string.h>

#include "pan_trace.h"
#include "pan_threads.h"

static struct sockaddr_in	ucast_addr, mcast_addr;
static struct sockaddr_in      *pan_addr;
static int			ucast_rcv_so, mcast_rcv_so, snd_so;
static pan_thread_p		rcv_daemon;
static pthread_mutex_t		mutex;
static pthread_cond_t           cond;
static int			communication_finished;

#define SHARED_NAME		"panda_adm"

#define LIN_BACKOFF   100
#define EXP_BACKOFF   600


#ifdef STATISTICS
static int n_upcalls	     = 0;
static int n_unicasts	     = 0;
static int n_multicasts	     = 0;
static int n_uni_packets     = 0;
static int n_multi_packets   = 0;

static int n_upc_small	     = 0;
static int n_uni_rcv_small   = 0;
static int n_multi_rcv_small = 0;
static int n_uni_small	     = 0;
static int n_multi_small     = 0;
#endif						 

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
make_socket_async(int socket)
{
#if 0                           /* ###erik: use select instead of SIGIO */
    if (fcntl(socket, F_SETOWN, getpid()) != 0){
	pan_panic("cannot set owner");
    }

    if (fcntl(socket, F_SETFL, O_NONBLOCK | FASYNC) != 0){
	/*
         * XXX: is FASYNC bsd specific? How to get this behavior with
         * sysV calls?
         */
	pan_panic("cannot set nonblocking async mode");
    }                                 
#endif
}


static void
write_mcast_addr(struct sockaddr_in *mcast_addr)
{
    FILE *shared_adm;

    if((shared_adm = fopen(SHARED_NAME, "w")) == NULL) {
	pan_panic("cannot open shared administration file");
    }

    if(fprintf(shared_adm, "%d %s\n", ntohs (mcast_addr->sin_port),
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
    mcast_addr->sin_port = htons (port);
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
    int gethostname(char *, size_t);
    FILE *shared_adm;
#if 0
    char name[256];
#endif

#ifdef NO
    struct hostent *this_host;

    /* get internet address of this host */
    if (gethostname(name, sizeof(name)) != 0) {
	pan_panic("gethostname failed");
    }
    this_host = gethostbyname(name);
    if (!this_host) {
	pan_panic("gethostbyname failed\n");
    }
#endif

    /* write internet address and port to file */
    /* avoid overwrites by having each process write at the appropiate place */
    backoff(1);
    for (;;) {
	int cnt, c;

	if ((shared_adm = fopen(SHARED_NAME, "r+")) == NULL) {
	    pan_panic("cannot open adm. file");
	}
	for ( cnt = 0; (c = fgetc( shared_adm)) != EOF;)
		if ( c == '\n')
			cnt++;
	if ( cnt == pan_sys_pid + 1)
		break;

	if (fclose(shared_adm) == EOF) {
	    pan_panic("cannot close shared administration file");
	}
	backoff(0);
    }
    if (fprintf(shared_adm, "%d %d %s\n", pan_sys_pid,
                ntohs (ucast_addr->sin_port),
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
		remote_addrs[pid].sin_port   = htons (port);
		remote_addrs[pid].sin_family = AF_INET;
		remote_addrs[pid].sin_addr.s_addr = inet_addr(name);
		i++;
	    }
	}

	if (fclose(shared_adm) == EOF) {
	    pan_panic("cannot close shared administration file");
	}

	backoff(0);
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
		   (char *) &sockopt, sizeof(int)) != 0) {
	pan_panic("setsockopt failed");
    }

    if (bind(rcv_socket, (struct sockaddr *) &rcv_addr, sizeof(rcv_addr)) < 0){
	pan_panic("bind");
    }
}

void
pan_sys_comm_start(void)
{
    if (pthread_mutex_init(&mutex, NULL) != 0){
	pan_panic("comm mutex init");
    }

    if (pthread_cond_init(&cond, NULL) != 0){
	pan_panic("comm cond init");
    }

    communication_finished = 0;
}

static int
gethostaddr (struct in_addr *in)
{
  char hostname[512];
  int rc;
  struct hostent *addr;

  rc = gethostname (hostname, sizeof (hostname));
  if (rc < 0)
    return rc;

  addr = gethostbyname (hostname);
  if (!addr)                    /* should set errno... */
    return -1;                  /* (h_errno is set instead) */

  assert (addr->h_addrtype == AF_INET);
  assert (addr->h_length == sizeof (__u32));

  in->s_addr = *(__u32 *)addr->h_addr_list[0];
  return 0;
}

static void
ignore_sigalrm (int s)
{
}

void
pan_sys_comm_wakeup(void)
{
    int addr_len = sizeof(struct sockaddr_in);
    int bcast_opt = 1;          /* ###erik */
    struct ip_mreq mreq;
#if 1
    struct sigaction sa;
    sigset_t sigs;

    /* ###erik: ignore SIGALRM because sometimes we get those at
       strange time, while no one seems to use them... maybe a bug in
       pthreads? */
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGALRM);

    sa.sa_handler  = ignore_sigalrm;
    sa.sa_mask     = sigs;
    sa.sa_flags    = SA_RESTART;
    sa.sa_restorer = NULL;

    if (sigaction(SIGALRM, &sa, NULL) != 0){
        pan_panic("sigaction");
    }
#endif

    /* SEND SOCKET */

    sys_stat_init();
    if ((snd_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	pan_panic("cannot create send socket");
    }

    /* ###erik: Linux requires you to set the SO_BROADCAST socket option
       before you can broadcast over a socket. */
    if (setsockopt (snd_so, SOL_SOCKET, SO_BROADCAST, &bcast_opt,
                    sizeof (bcast_opt)) < 0) {
        pan_panic ("cannot set broadcast option on send socket");
    }

    /* MULTICAST RECEIVE SOCKET */

    if (pan_sys_pid == 0) {
	/* Create port and broadcast address */
	srand(getpid());
	mcast_addr.sin_family = AF_INET;
	mcast_addr.sin_port   = (rand() | IPPORT_USERRESERVED) & 0xFFFF;

#ifndef MULTICAST

#ifdef LOOPBACK
	mcast_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
	mcast_addr.sin_addr.s_addr = inet_addr ("255.255.255.255");
#endif

#else  /* MULTICAST */
	mcast_addr.sin_addr.s_addr = htonl((IN_MULTICAST_NET | (rand() + 2))
                                           & 0xefffffff);
#endif

	write_mcast_addr(&mcast_addr);
    } else {
	read_mcast_addr(&mcast_addr);
    }

    /* initialise multicast receive socket (add mcast address to it) */
    if ((mcast_rcv_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
	pan_panic("cannot create mcast receive socket");
    }

    make_socket_async(mcast_rcv_so);

#ifdef MULTICAST
    mreq.imr_multiaddr = mcast_addr.sin_addr;
    mreq.imr_interface.s_addr = htonl (INADDR_ANY);

    if (setsockopt(mcast_rcv_so, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
		   sizeof(struct ip_mreq)) != 0){
	pan_panic("cannot add broadcast membership");
    }
#endif

    bind_socket_to_port(mcast_rcv_so, mcast_addr.sin_port);

    /* UNICAST RECEIVE SOCKET */

    if ((ucast_rcv_so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
	pan_panic("cannot create ucast receive socket");
    }
    make_socket_async(ucast_rcv_so);

    /* create address; system will fill in the port */
    ucast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ucast_addr.sin_family = AF_INET;
    ucast_addr.sin_port = 0;
    if (bind(ucast_rcv_so, (struct sockaddr *)&ucast_addr,
	     sizeof(ucast_addr)) != 0) {
	pan_panic("bind failed");
    }
    if (getsockname(ucast_rcv_so, (struct sockaddr *)&ucast_addr,
		    &addr_len) != 0) {
	pan_panic("getsockname failed\n");
    }

    /* ###erik: on Linux, getsockname returns 127.0.0.1 as the sockets
       address, which won't work when transfered to another machine.  */
#ifndef LOOPBACK
    if (gethostaddr(&ucast_addr.sin_addr) < 0) {
        pan_panic("gethostaddr failed\n");
    }
#endif /* LOOPBACK */

    write_ucast_address(&ucast_addr);
    pan_addr = (struct sockaddr_in *)
	pan_malloc(pan_sys_nr * sizeof(struct sockaddr_in));
    read_ucast_adresses(pan_addr);

    /* RECEIVE DAEMON */
    rcv_daemon = pan_thread_create(comm_daemon, 0, 0, pan_thread_maxprio(), 1);
}

void
pan_sys_comm_end(void)
{
    pthread_mutex_lock(&mutex);
    communication_finished = 1;
    pthread_mutex_unlock(&mutex);

    pthread_cond_signal(&cond);
    pan_thread_join(rcv_daemon);

    close(snd_so);
    close(ucast_rcv_so);
    close(mcast_rcv_so);

    pan_free(pan_addr);

    if (pan_sys_pid == 0){
	unlink(SHARED_NAME);
    }
    sys_stat_end();
}




static void
handle_packet(int so, pan_fragment_p fragment, int rlen)
{
    pan_nsap_p nsap;

    assert(rlen >= 0);

    nsap = pan_sys_fragment_nsap_pop(fragment, rlen);

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
}



static int
try_receive(int socket, char *buf, int size)
{
    int rlen;
    int fromlen = 0;

    do {
	rlen = recvfrom(socket, buf, size, 0, NULL, &fromlen);
    } while (rlen < 0 && errno == EINTR);

    if (rlen < 0 && errno != EWOULDBLOCK) {
	pan_panic("recvfrom error");
    }

    return rlen;
}

#if 0
static void
io_handler(int signal)
{
    assert(signal == SIGIO);

    /*
     * Wakeup comm_daemon. We cannot execute the upcall from the current
     * stack, because the current thread may have acquired some
     * locks. Would SA_STACK solve that problem? 
     */
    pthread_cond_signal(&cond);
}
#endif

static void
comm_daemon(void *arg)
{
    int try_mcast, try_ucast;
    pan_fragment_p fragment;
    int rlen;
#if 0
    struct sigaction sa;
    sigset_t sigs;
    int sig;
#endif
    fd_set rcv_set;
    struct timeval timeout;
    int incoming, nr_fds;

    trc_new_thread( 0, "communication daemon");

#if 0
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGIO);

    sa.sa_handler  = io_handler;
    sa.sa_mask     = sigs;
    sa.sa_flags    = SA_RESTART;
    sa.sa_restorer = NULL;

    if (sigaction(SIGIO, &sa, NULL) != 0){
        pan_panic("sigaction");
    }
#endif

    fragment = pan_fragment_create();
    pan_sys_pool_mark(fragment, EXTERNAL_ENTRY);

    try_mcast = try_ucast = 1;

    /* Determine the number of fds select must check. */
    nr_fds = ucast_rcv_so;
    if (mcast_rcv_so > nr_fds)
        nr_fds = mcast_rcv_so;
    nr_fds++;

    while (1) {
        FD_ZERO(&rcv_set);
        FD_SET(ucast_rcv_so, &rcv_set);
        FD_SET(mcast_rcv_so, &rcv_set);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        incoming = select(nr_fds, &rcv_set, NULL, NULL, &timeout) > 0;
        
        /* Check if communication is finished _before_ handling
           incoming packets. */
        pthread_mutex_lock(&mutex);
        if (communication_finished)
            break;

        if (incoming)
        {
            if (FD_ISSET(ucast_rcv_so, &rcv_set)) {
	        rlen = try_receive(ucast_rcv_so, fragment->data, PACKET_SIZE);
	        if (rlen >= 0) {
		    handle_packet(ucast_rcv_so, fragment, rlen);
	        }
            }
            
            if (FD_ISSET(mcast_rcv_so, &rcv_set)) {
                rlen = try_receive(mcast_rcv_so, fragment->data, PACKET_SIZE);
                if (rlen >= 0) {
                    handle_packet(mcast_rcv_so, fragment, rlen);
                }
            }
	}
        pthread_mutex_unlock(&mutex);
    }

    pan_sys_pool_mark(fragment, OUT_POOL_ENTRY);
    pan_fragment_clear(fragment);

    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

static void
unicast(int pid, char *data, int size)
{
    int r;

    assert(!communication_finished);
    assert(pid >= 0 && pid < pan_sys_nr);

    do{
	r = sendto(snd_so, data, size, 0, (struct sockaddr *) &(pan_addr[pid]),
		   sizeof(struct sockaddr_in));
    }while ((r < 0) && (errno == EINTR));

    if (r < 0){
        /* ###erik: added ntohs */
	pan_panic("unicast send failed: pid %d address: %s, port: %d",
		  pid, inet_ntoa(pan_addr[pid].sin_addr),
		  ntohs (pan_addr[pid].sin_port));
    }
}


static void
broadcast(char *data, int size)
{
    int r;

    assert(!communication_finished);

    do{
	r = sendto(snd_so, data, size, 0, (struct sockaddr *) &mcast_addr,
		   sizeof mcast_addr);
    }while ((r < 0) && (errno == EINTR));

    if (r < 0){
        /* ###erik: added ntohs */
	pan_panic("multicast send failed: address: %s, port: %d",
		  inet_ntoa(mcast_addr.sin_addr), ntohs (mcast_addr.sin_port));
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
    *(int *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);

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
    *(int *)(small + nsap->data_len) = pan_sys_nsap_index(nsap);

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


#ifdef NO
int
sys_sprint_comm_stat_size(void)
{
#ifdef STATISTICS
    return (2 + 1 + 1 + 6 + 5 * (1 + 5) + 1 + 5 + 5 * (1 + 5) + 3) * 2;
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
#endif
