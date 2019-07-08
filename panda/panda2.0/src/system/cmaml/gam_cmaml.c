#ifndef LAM

int pan_dummy_ansi;  /* Otherwise this file would be empty -> not ANSI */

#else

#include <am.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "gam_cmaml.h"

struct rport {
    int                rp_size;       /* buffer size */
    int                rp_count;      /* received bytes count */
    char              *rp_addr;       /* target buffer */
    void             (*rp_handler)(int rport, void *arg2);    /* handler */
    void              *rp_arg2;       /* second handler argument */
    struct rport      *rp_next;
};

#define MAX_TRANSFER_SIZE 2048   /* Max. byte count accepted by am_store(). */
#define MAX_PORT           128   /* This many free rports per processor. */

#define LCP_DATA  "/profile/group/user/versto/home/test/myrinet/LAM/lcp.dat"

static struct rport ports[MAX_PORT];
static struct rport *free_rports;

static int partition_size = 0;

static volatile int sync_flag;
static int *arrived[2];
static int num_arrived[2];

static char *topology = "HGFEDCB01234567";

/*
 * My lucky day. This happens to be in the library, but not in the
 * interface. I need this, so:
 */
extern int am_request_5(int dest, void (*fun)(),
			int arg0, int arg1, int arg2, int arg3, int arg4);

static void
create_config_file(int nhosts, int me)
{
    char *topology_base = topology + 7;
    char fname[128], *hostname, *p;
    int first_host, i;
    FILE *fp;

    /*
     * Find the name of the host we're running on.
     */
    if (!(hostname = getenv("AX_HOST"))) {
	fprintf(stderr, "%d: create_config_file: AX_HOST not set\n", me);
	exit(1);
    }

    if ((p = strrchr(hostname, '/')) != 0) {
	hostname = p + 1;
    }

    if (strncmp(hostname, "zoo", 3) != 0) {
	fprintf(stderr, "%d: create_config_file: unexpected host name\n", me);
	exit(1);
    }

    /*
     * Open the configuration file for this host.
     */
    (void)sprintf(fname, "lam.conf.%s", hostname);

    if (!(fp = fopen(fname, "w"))) {
	fprintf(stderr, "%d: create_config_file: cannot open %s for writing\n",
		me, fname);
	exit(1);
    }

    /*
     * Create the host entries in the configuration file.
     */
    fprintf(fp, "%s\n", LCP_DATA);
    fprintf(fp, "%d\n", nhosts);
    
    first_host = atoi(hostname + 3) - me;
    for (i = 0; i < nhosts; i++) {
	fprintf(fp, "%c zoo%d\n", topology_base[i-me], first_host + i);
    }

    (void)fclose(fp);
}

void
gam_cmaml_init(int part_size, int me)
{
    int ncpu, i;

    create_config_file(part_size, me);

    /* 
     * I don't know what the integer return value means...
     */
    (void)am_enable();
    ncpu = am_procs();

    if (am_my_proc() == 0) {
	printf("using GAM-CMAML emulation...\n");
    }

    assert(part_size >= am_procs());
    partition_size = part_size;

    if (am_my_proc() == 0) {
	arrived[0] = (int *)calloc(ncpu, sizeof(int));
	if (!arrived[0]) {
	    fprintf(stderr, "%d) gam_cmaml_init: calloc failed", am_my_proc());
	    exit(1);
	}
	arrived[1] = (int *)calloc(ncpu, sizeof(int));
	if (!arrived[1]) {
	    fprintf(stderr, "%d) gam_cmaml_init: calloc failed", am_my_proc());
	    exit(1);
	}
    }

    for (i = 0; i < MAX_PORT; i++) {
	ports[i].rp_next = free_rports;
	free_rports = &ports[i];
    }
}

void gam_cmaml_end(void)
{
    if (am_my_proc() == 0) {
	free(arrived[0]);
	free(arrived[1]);
	arrived[0] = 0;
	arrived[1] = 0;
    }

    (void)am_disable();
}

int
CMMD_partition_size(void)
{
    return partition_size;
}

int CMMD_self_address(void)
{
    return am_my_proc();
}

void
CMMD_error(const char *fmt, ...)
{
    va_list ap;
 
    va_start(ap, fmt);
    fprintf(stderr, "%d) ", am_my_proc());
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();  /* dump core */
}

static void
ignore(vnn_t src)
{
}

static void
sync_handler(vnn_t src)
{
    static int round = 0;
    static int next_round = 1;
 
    if (arrived[round][src]) {
        arrived[round][src] = 0;
        arrived[next_round][src] = 1;
        num_arrived[next_round]++;
    } else {
        arrived[round][src] = 1;
        arrived[next_round][src] = 0;
        num_arrived[round]++;
    }
    if (num_arrived[round] == am_procs()) {
        num_arrived[round] = 0;
        round = next_round;
        next_round = 1 - next_round;
 
        sync_flag = 1;
    }

    if (src != am_my_proc()) {
	am_reply_0(src, ignore);
    }
}

static void
sync_done_handler(vnn_t src)
{
    sync_flag = 1;
    am_reply_0(src, ignore);
}

void
CMMD_sync_with_nodes(void)
{
    int i;
    int me = am_my_proc();
    int ncpu = am_procs();

    assert(sync_flag == 0);

    if (me == 0) {
        sync_handler(0);
    } else {
        am_request_0(0, sync_handler);
    }

    do {
	am_poll();
    } while (!sync_flag);
    sync_flag = 0;

    if (me == 0) {
        for (i = 1; i < ncpu; i++) {
	    am_request_0(i, sync_done_handler);
        }
    }
}

void
CMAML_poll_while_lt(volatile int *flag, int value)
{
    while (*flag < value) {
	am_poll();
    }
}

void
CMAML_poll(void)
{
    am_poll();
}

int
CMAML_interrupt_status(void)
{
    return FALSE;   /* interrupts are disabled */
}

int
CMAML_disable_interrupts(void)
{
    return TRUE;
}

static void
handle_request(vnn_t src, int a0, int a1, int a2, int a3, int handler_int)
{
    handler_t handler = (handler_t)handler_int;

    (*handler)(a0, a1, a2, a3);
}

static void
handle_reply(vnn_t src, int a0, int a1, int a2, int handler_int)
{
    handler_t handler = (handler_t)handler_int;

    (*handler)(a0, a1, a2, 0);
}

void
CMAML_request(int dest,   void (*handler)(int, int, int, int),
                          int a0, int a1, int a2, int a3)
{
    int ret;

    ret = am_request_5(dest, handle_request, a0, a1, a2, a3, (int)handler);
    if (ret == -1) {
	fprintf(stderr, "%d) CMAML_request: am_request_5 failed\n",
		am_my_proc());
	exit(1);
    }
}

void
CMAML_reply(int dest,   void (*handler)(int, int, int, int),
                        int a0, int a1, int a2, int ignored)
{
    int ret;

    ret = am_reply_4(dest, handle_reply, a0, a1, a2, (int)handler);
    if (ret == -1) {
	fprintf(stderr, "%d) CMAML_reply: am_reply_4 failed\n",
		am_my_proc());
	exit(1);
    }
}

static void
handle_store(vnn_t src, void *addr, int nbytes, void *arg)
{
    struct rport *rp;
    int i;

    for (i = 0; i < MAX_PORT; i++) {
	rp = &ports[i];
	if (rp->rp_addr + rp->rp_count == addr) {      /* found right port? */
	    rp->rp_count += nbytes;
	    assert(rp->rp_count <= rp->rp_size);
	    if (rp->rp_count == rp->rp_size) {         /* all data present? */
		(*rp->rp_handler)((int)rp->rp_addr, rp->rp_arg2);
	    }
	    return;
	}
    }

    fprintf(stderr, "%d: handle_store: address %p not found\n",
	    am_my_proc(), addr);
    exit(1);
}

int
CMAML_scopy(int dest, int rport, int align, int offset,
	    int addr, int nbytes,
	    void (*sent_handler)(void *arg),
	    void *arg)
{
    int ret;
    void *local_addr  = (void *)addr;
    void *remote_addr = (void *)rport;   /* hack */

    assert(!sent_handler);
    assert(!arg);

    while (nbytes > MAX_TRANSFER_SIZE) {
	ret = am_store(dest, local_addr, remote_addr, MAX_TRANSFER_SIZE,
		       handle_store, 0);
	if (ret == -1) {
	    fprintf(stderr, "%d: CMAML_scopy: am_store failed\n", am_my_proc());
	    exit(1);
	}
	local_addr = (char *)local_addr + MAX_TRANSFER_SIZE;
	remote_addr = (char *)remote_addr + MAX_TRANSFER_SIZE;
	nbytes -= MAX_TRANSFER_SIZE;
    }

    ret = am_store(dest, local_addr, remote_addr, nbytes, handle_store, 0);
    if (ret == -1) {
	fprintf(stderr, "%d: CMAML_scopy: am_store failed\n", am_my_proc());
	exit(1);
    }

    return TRUE;
}

int
alloc_port(void *buf, int size,
	   void (*handler)(int rport, void *arg), void *arg2)
{
    struct rport *rp;

    if (!free_rports) {
	return -1;
    }

    rp = free_rports;
    free_rports = rp->rp_next;

    rp->rp_size    = size;
    rp->rp_count   = 0;
    rp->rp_addr    = buf;
    rp->rp_handler = handler;
    rp->rp_arg2    = arg2;
    rp->rp_next    = 0;

    return (int)buf;           /* hack: use buffer as identifier */
}

int
CMAML_free_rport(int rport)
{
    struct rport *rp;
    void *addr = (void *)rport;
    int i;

    for (i = 0; i < MAX_PORT; i++) {
	rp = &ports[i];
	if (rp->rp_addr == addr) {
	    rp->rp_addr    = 0;
	    rp->rp_handler = 0;
	    rp->rp_arg2    = 0;
	    rp->rp_next    = free_rports;
	    free_rports = rp;
	    return TRUE;
	}
    }

    return FALSE;
}

#endif /* LAM */
