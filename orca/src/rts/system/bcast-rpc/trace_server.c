/* $Id: trace_server.c,v 1.9 1994/10/03 14:39:19 ceriel Exp $ */

#include <amoeba.h>
#include "thread.h"
#include "module/rnd.h"
#include "module/name.h"
#include "module/rpc.h"
#include "module/syscall.h"
#include "module/mutex.h"
#include "module/buffers.h"
#include <cmdreg.h>
#include <stderr.h>
#include <stdcom.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <semaphore.h>

#include <interface.h>
    
#include "trace.h"
#include "trace_rec.h"
    
#define STD_INFO_REPL   "trace server"
#define CONF_FILE	"trace_conf"
#define SERVERSTACKSZ 	10000

extern FILE *file;

extern char *buf_get_short();
    
int 			trace_on = 0;			/* tracing on/off */
int 			trace_event[NTRACE_EVENT];

static capability 	tracecap;
static port 		privport;
static char 		*tracebuf;
static char 		*trace_last;
static int 		tracesize;
static char 		*traceptr;
static semaphore  	sema_done, sema_release, sema_client;
static mutex		mu_trace_put;
static int 		done = 0;

static void trace_server()
{
    errstat err;
    header hdr;
    long n;
    long replysize;
    bufptr replybuf = 0;
    int com;
    trace_rec_t flush_trace;	/* buffer for flush record */
    int startflush;
    
    while (!done) {
	hdr.h_port = privport;
	n = rpc_getreq(&hdr, NILBUF, 0);
	if (n < 0) {
	    fprintf(file, "trace_server: getrequest failed: %s\n",
		   err_why(ERR_CONVERT(n)));
	    continue;
	}
	replybuf = 0;
	replysize = 0;
	switch (com = hdr.h_command) {
	case STD_INFO:
	    hdr.h_status = STD_OK;
	    replybuf = STD_INFO_REPL;
	    replysize = strlen(STD_INFO_REPL);
	    break;
	case TRACE_GET:
	    sema_up(&sema_client);
	    sema_down(&sema_release);
	    startflush = sys_milli() + time_clock_diff;
	    hdr.h_status = STD_OK;
	    replybuf = tracebuf;
	    replysize = (traceptr - tracebuf); 
	    hdr.h_extra = done;
	    break;
	default:
	    hdr.h_status = STD_COMBAD;
	    break;
	}
	verify_buffer(replybuf, replysize);
	fprintf(file, "trace_server %ld sends %d bytes, done=%d\n",
		this_cpu, replysize, done);
	rpc_putrep(&hdr, replybuf, replysize);
	if (com == TRACE_GET) {
	    traceptr = tracebuf;
	    put_flush_rec(&flush_trace, this_cpu, startflush);
	    sema_up(&sema_done);
	}
    }
    thread_exit();
}

verify_buffer(buf, size)
	char *buf;
	long size;
{
	long n;
	char *p;
	short recsize;

	n = size;

	p = buf;
	while (n > 0) {
		p = buf_get_short(p, buf+size, &recsize);
		verify_record(p);
		p += recsize;
		n -= sizeof(short) + recsize;
	}
}

verify_record(rec)
	char *rec;
{
    int i;

    memcpy(&i, rec, sizeof(int));
    switch(i) {
    case FORK_RECORD:
    case OP_RECORD:
    case SUSPEND_RECORD:
    case OPEREXIT_RECORD:
    case FLUSH_RECORD:
	break;
    default:
	fprintf(file, "ERROR: trace_server tries to send unkown record type%d\n", i);
	exit(1);
	break;
    }
}

static int trace_event_init()
{
    FILE *in;
    int eventno;

    in = fopen(CONF_FILE, "r");
    if(in == NULL) return(0);

    while(fscanf(in, "%d", &eventno) != EOF) {
	trace_event[eventno] = 1;
    }
    return(fclose(in) == 0);
}


int trace_init(capname, size)
    char *capname;
    int size;
{
    errstat r;
    int i;
    
    for(i=0; i < NTRACE_EVENT; i++)
	trace_event[i] = 0;

    uniqport(&tracecap.cap_port);
    privport = tracecap.cap_port;
    priv2pub(&tracecap.cap_port, &tracecap.cap_port);
    r = name_append(capname, &tracecap);
    if (r < 0) {
	fprintf(file, "name_append() = %s\n", err_why(r));
	return 0;
    }
    tracesize = size;
    traceptr = tracebuf = (char *) malloc(tracesize);
    trace_last = tracebuf + tracesize;
    sema_init(&sema_done, 0);
    sema_init(&sema_release, 0);
    sema_init(&sema_client, 0);
    mu_init(&mu_trace_put);
    if(!trace_event_init()) {
	fprintf(file, "trace_init: trace_event_init() failed (no conf file?)\n");
	return(0);
    }
    trace_on = 1;
    return thread_newthread(trace_server, SERVERSTACKSZ, 0, 0);
}


void trace_put(rec, recsize)
    char *rec;
    short recsize;
{
    mu_lock(&mu_trace_put);
    assert(recsize + sizeof(short) <= tracesize);
    if (traceptr + recsize + sizeof(short) > trace_last) {
	sema_down(&sema_client);
	sema_up(&sema_release);
	sema_down(&sema_done);
    }
    traceptr = buf_put_short(traceptr, trace_last, recsize);
    memcpy(traceptr, rec, recsize);
    traceptr += recsize;
    mu_unlock(&mu_trace_put);
}

/* special trace_put to be called by server after flushing the buffer */

unsafe_trace_put(rec, recsize)
    char *rec;
    short recsize;
{
    traceptr = buf_put_short(traceptr, trace_last, recsize);
    memcpy(traceptr, rec, recsize);
    traceptr += recsize;
}

void trace_end(capname)
    char *capname;
{
    mu_lock(&mu_trace_put);
    done = 1;
    name_delete(capname);
    sema_down(&sema_client);
    sema_up(&sema_release);
    sema_down(&sema_done);
    mu_unlock(&mu_trace_put);
    trace_on = 0;
}
