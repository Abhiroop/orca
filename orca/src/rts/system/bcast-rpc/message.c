/* $Id: message.c,v 1.32 1996/07/15 14:51:17 ceriel Exp $ */

#include "amoeba.h"
#include "thread.h"
#include "module/rnd.h"
#include "module/rpc.h"
#include "module/syscall.h"
#include "module/mutex.h"
#include "stderr.h"
#include "cmdreg.h"
#include "group.h"
#include "semaphore.h"
#include "remote.h"
#include "interface.h"
#include "scheduler.h"
#include "message.h"
#include "policy.h"

/* This file implements the communication part of an orca rts that is based
 * on bcast. The rts can be used in three ways: 1) all object are replicated
 * and bcast is used to keep them consistent; 2) same as 1), but every 
 * cp_interval a consistent snapshot is made of the complete application; 
 * 3) objects are replicated or stored on a specific processor; the rts
 * makes a decision on receiving a fork.
 */
    
#define MAXGROUP        128
#define LARGE		2800
#define REPLYTIMEOUT	10000
#define SYNCTIMEOUT	10000
#define NTIME_UPDATE	100
#define SIZEBINSIZE     20

extern int this_cpu, ncpus;
extern int logmaxbuf;
extern int maxmesssize, logmaxmesssize;
extern int trace_on;
extern struct ObjectEntry *ObjectTable[];
extern FILE *file;
extern int rts_verbose;
extern int grp_debug;
extern int n_rpc_listeners;
#ifdef CHK_FORK_MSGS
extern int chksums;
#endif

extern interval
	grp_var_sync_timeout,
        grp_var_reply_timeout,
        grp_var_alive_timeout,
        grp_var_reset_timeout,
        grp_var_locate_timeout,
        grp_var_max_retrial;

/* Exported variables: */
semaphore sem_end;
semaphore allow_fork;
capability *me, *groupcap;
int NoForkYet;
int BcastMoreMessages;
int sw_protocol_sz = LARGE;

/* Global variables: */
static port group;			/* port identifying group */
#ifndef PREEMPTIVE
static int blocked;	   	/* nuisance due to nonpreemptive scheduling */
#endif
static g_indx_t memlist[MAXGROUP];	/* list of members */
static port memport[MAXGROUP];		/* list of public ports */
static port my_priv_port;		/* my private port */
static int gd;				/* group descriptor */
static int alive;			/* number of joined members */
static int finish;			/* done? */
static int nwait_sequence; 		/* #thread waiting for obj to arrive */
static mutex mu_seq;			/* to protect nwait_sequence */
static int nfork;			/* #fork remote statements */
static grpstate_t grpstate;		/* group info */
static g_seqcnt_t sequence_number;	/* the global sequence number */
static int rpc_getreq_bin[SIZEBINSIZE];	/* statistics on rpc message sizes */
static int rpc_putrep_bin[SIZEBINSIZE];	/* statistics on rpc message sizes */
static int rpc_putreq_bin[SIZEBINSIZE];	/* statistics on rpc message sizes */
static int rpc_getrep_bin[SIZEBINSIZE];	/* statistics on rpc message sizes */
static int grp_rcv_bin[SIZEBINSIZE];	/* statistics on grp message size */
static int grp_snd_bin[SIZEBINSIZE];	/* statistics on grp message size */
static semaphore sem_finish;
static semaphore sem_print;
static semaphore sem_wait_sequence;
static semaphore sem_start;
static long maxprio;
static semaphore rcve_one;
static int rcvd_msgs;


/* Variables needed for makeing checkpoints. */
extern int checkpointing, cp_interval;

static int freeze_process;
static int nfrozen;
static int nsender;
static int incarno;
static semaphore sem_frozen;
static semaphore sem_makecp;
static semaphore sem_cpdone;
static mutex mu_tab;

static void handlemess();

static void
freeze()
{
    /* Freeze this thread, so that a check point can be made. If this
     * is the last thread that has to be frozen, wake the thread up that
     * makes the check point.
     */

    if(nsender == 0) 
	sema_up(&sem_makecp);
    nfrozen++;
    sema_down(&sem_frozen);
}


static void
unfreeze()
{
    /* Unfreeze process. */

    int i;

    freeze_process = 0;
    for(i=0; i < nfrozen; i++)
	sema_up(&sem_frozen);
    nfrozen = 0;
}


static twologenplusone(n)
    long n;
{
    register log;
    
    log = -1;
    n++;
    do {
	n >>= 1;
	log++;
    } while(n != 0);
    assert(log >= 0);
    assert(log < SIZEBINSIZE);
    return log;
}

#define INCSIZEBIN(x, size)     do { \
				  if (rts_verbose) x[twologenplusone(size)]++; \
				} while (0)


/*
 * Initialization stuff
 */

#include <groupvar.h>

errstat
grp_set_new(var, val)
int var;
interval val;
{
    errstat err;

    err = grp_set(gd, &group, (interval) -1, (interval) var, val, 0);
    if (err != STD_OK) {
	printf("warning: grp_set(%d, %d) failed (%s)\n", 
		var, val, err_why(err));
    }
}

static startmember()
{
    header hdr;
    
    alive = 0;
    hdr.h_command = JOIN;
    hdr.h_extra = this_cpu;
    hdr.h_port = group;
    hdr.h_signature = memport[this_cpu];
    sema_init(&rcve_one, 0);
    if(this_cpu == 0) {
	gd = grp_create(&group, 0, ncpus, logmaxbuf, logmaxmesssize);
	if (gd < 0) {
	    fprintf(stderr, "grp_create error: %s\n", err_why(ERR_CONVERT(gd)));
	    doexit(1);
	}
    }
    else {
	gd = grp_join(&hdr);
	if (gd < 0) {
	    fprintf(stderr, "grp_join error: %s\n", err_why(ERR_CONVERT(gd)));
	    doexit(1);
	}
    }
    grp_set(gd, &group, SYNCTIMEOUT,
	    grp_var_reply_timeout != 0 ? grp_var_reply_timeout : REPLYTIMEOUT,
	    grp_debug, sw_protocol_sz);
    if (grp_var_alive_timeout != 0) {
	grp_set_new(GRP_VAR_ALIVE_TIMEOUT, grp_var_alive_timeout);
    }
    if (grp_var_reset_timeout != 0) {
	grp_set_new(GRP_VAR_RESET_TIMEOUT, grp_var_reset_timeout);
    }
    if (grp_var_locate_timeout != 0) {
	grp_set_new(GRP_VAR_LOCATE_TIMEOUT, grp_var_locate_timeout);
    }
    if (grp_var_max_retrial != 0) {
	grp_set_new(GRP_VAR_MAX_RETRIAL, grp_var_max_retrial);
    }
    grp_info(gd, &group, &grpstate, memlist, MAXGROUP);
    alive = grpstate.st_total;
    sequence_number = grpstate.st_expect;
}


static void buildgroup()
{
    header hdr;
    int nmess, r;
    int i;
    
    hdr.h_port = group;
    while (grpstate.st_total < ncpus) {
	r = grp_receive(gd, &hdr, 0, 0, &nmess);
	if (r < 0) {
	    fprintf(stderr, "%d: buildgroup: grp_receive error: %s\n", this_cpu, err_why(ERR_CONVERT(r)));
	    doexit(1);
	}
    	grp_info(gd, &group, &grpstate, memlist, MAXGROUP);
	alive = grpstate.st_total;
	INCSIZEBIN(grp_rcv_bin, 0);
	handlemess(&hdr, (char *)0, 0, (char **) 0, (int *) 0);
	sequence_number++;
    }
    /* switch to regular sync rate */
    grp_set_new(GRP_VAR_SYNC_TIMEOUT,
	       grp_var_sync_timeout != 0 ? grp_var_sync_timeout : SYNCTIMEOUT);
}


/* 
 * Routines to make check points. The cp code consists of two parts: making
 * cp's and rolling back to a previous incarnation. Because it is not possible,
 * to check point kernel state of a thread/process in amoeba, we have to make
 * sure that a cp is made when all threads are in user space. When a process
 * is rollbacked, the recovery code has to rebuild the kernel status of a
 * process (e.g., rebuilding the group, blocking on semaphores).
 */


/*
 * Making a cp:
 *	1) the cp_thread on cpu 0 bcasts een SAVESTATE msg (every cp_interval
 *	   secs)
 *	2) when grp_listener() receives this message it accepts no other
 *	   messages and waits until all user threads are blocket in user space
 *	   (it has to wait until all grp_send()'s are done). 
 *	   To achieve this, each time a thread calls grp_send a variable
 *	   nsender is increased. If it is finished it decreases nsender. If a
 *	   threads wants to call grp_send, but grp_listener is waiting to make a
 *	   cp (indicated by the variable freeze_process), the thread waits
 *	   with calling grp_send and blocks on the semaphore sem_frozen.
 *         If a thread is blocked in a semaphore nothing special needs to be
 *	   done. The semaphore code itself takes care of signals. 
 *	3) send cp request to gax (all threads but grp_listener are blocked).
 *	4) block in a semaphore until gax tells us that the cp has been made.
 *	5) gax makes cp and it is its job to store it somewhere save.
 *	6) gax sends a continue msg to cp_server thread of this process.
 *	7) grp_listener is unblocked by cp_server.
 *	8) grp_listener unblocks threads that are blocked on the semaphore
 *	   sem_frozen.
 */


void printcheckstat()
{
	fprintf(file, "%d: # cp(s) = %d\n", this_cpu, incarno);
}


static void
cp()
{
    /* Make a check point. */

    freeze_process = 1;
    if(nsender > 0) 
	sema_down(&sem_makecp);
    cp_docp(me, incarno);
    incarno++;
    unfreeze();
}


void
cp_continue()
{
    /* called by the cp_server to tell that the cp has been made. */

    sema_up(&sem_cpdone);
}


/* Recovery:
 * 	1) when grp_receive indicates that one of the members has died,
 *	   send a msg to gax to rollback.
 *	2) gax finds the last complete incarnation of this application.
 *	3) gax kills all running processes of this application
 *	4) gax starts an earlier check point of the application.
 *	5) gax sends the cp_server of each process a recover message (instead
 *	   of a continue message)
 *	6) cp_thread creates/joins the new group and returns success to gax.
 *	7) cp_thread waits until the complete group is rebuild.
 *	8) cp_thread unfreezes any threads that are blocked in the semaphore
 *	   sem_frozen.
 *	9) unblock all threads that are blocked on SendDone. The message sent
 *	   by these blocked threads are lost. They have to be sent again.
 *	10) the process continues.
 */

void 
cp_initrestart()
{
    /* This routine is called after a process is rolled back, but before
     * gax is told that the rollback succeeded.
     */

    if(ncpus > 1) startmember();
}


void 
cp_restart()
{
    /* This routine is called after a process is rolled back, but before
     * it continues to run again.
     */
    
    if(ncpus > 1) {
	buildgroup();
	unfreeze();
	wakeup_sendingprocess();
    }
    sema_up(&sem_cpdone);
}


static void
recover()
{
    if (! checkpointing) exit(1);
    fprintf(file, "%d: recover: rollback to %d\n", this_cpu, incarno);
    if(!cp_rollback(me, incarno)) {
	m_syserr("recover: cp_rollback failed\n");
    }
    exit(0);	/* gax will start this process again */
}


/* 
 * Routines to map object id's on object pointers. 
 */

int hash(i)
    int i;
{
    /* The hashing function */

    return i % MAXENTRIES;
}

t_object *
AddObject(object)
    t_object *object;
{
    int index = hash(object->o_id);
    struct ObjectEntry *prev = (struct ObjectEntry *) 0, *entry;

    mu_lock(&mu_tab);
    entry = ObjectTable[index];

    while (entry != (struct ObjectEntry *) 0) {
	if (entry->obj_id == object->o_id) {
		mu_unlock(&mu_tab);
		return (&entry->obj_descr);
	}
	prev = entry;
	entry = entry->obj_next;
    }
    /* Add object to table */
    entry = (struct ObjectEntry *) m_malloc(sizeof(struct ObjectEntry));
    entry->obj_id = object->o_id;
    entry->obj_descr = *object;
    entry->obj_next = (struct ObjectEntry *) 0;
    if (prev) prev->obj_next = entry;
    else ObjectTable[index] = entry;
    mu_unlock(&mu_tab);
    return 0;
}


/* Shared objects are stored on in an object table and get an id. */
InstallObject(obj)
    t_object *obj;
{
    obj->o_id = GenerateId();
    (void) AddObject(obj);
}


t_object *
GetObjectDescr(id)
    int id;
{
    struct ObjectEntry *entry;
    
    mu_lock(&mu_tab);
    entry = ObjectTable[hash(id)];
    while (entry != (struct ObjectEntry *) 0) {
	if (entry->obj_id == id) {
    		mu_unlock(&mu_tab);
		return &entry->obj_descr;
	}
	entry = entry->obj_next;
    }
    mu_unlock(&mu_tab);
    return (t_object *) 0;
}


void DeleteObject(object)
	t_object *object;
{
    struct ObjectEntry *entry;
    struct ObjectEntry *prev = 0;
    
    mu_lock(&mu_tab);
    entry = ObjectTable[hash(object->o_id)];
    while (entry != (struct ObjectEntry *) 0) {
	if (entry->obj_id == object->o_id) {
		if(prev == 0) {
			ObjectTable[hash(object->o_id)] = entry->obj_next;
		} else {
			prev->obj_next = entry->obj_next;
		}
		m_free(entry);
    		mu_unlock(&mu_tab);
		return;
	}
	prev = entry;
	entry = entry->obj_next;
    }
    mu_unlock(&mu_tab);
}


/*
 * handlemess
 */

struct bufqueue {
    struct bufqueue *next;
    header *hdr;
    char *request;
    int reqlen;
};

static struct bufqueue *head, *tail;
static int must_buffer;

static void
queue_buffer(hdr, request, reqlen)
    header *hdr;
    char *request;
{
    struct bufqueue *e = m_malloc(sizeof(struct bufqueue));

    e->hdr = m_malloc(sizeof(header));
    memcpy(e->hdr, hdr, sizeof(header));
    e->request = m_malloc(reqlen);
    memcpy(e->request, request, reqlen);
    e->reqlen = reqlen;
    e->next = 0;
    if (! head) head = e;
    else tail->next = e;
    tail = e;
}

static void
process_queue()
{
    must_buffer = 0;
    while (head && ! must_buffer) {
	struct bufqueue *e = head;
	head = e->next;

	handlemess(e->hdr, e->request, e->reqlen, (char **) 0, (int *) 0);
	m_free((char *)(e->hdr));
	m_free(e->request);
	m_free((char *)e);
    }
}

static void 
r_fork(hdr, request, reqlen)
    header *hdr;
    char *request;
{
    int cpu;
    char *p = request;
    prc_dscr *pd;
    void **argv;
#ifdef OPTIMIZED
    int cnt;
#endif
    int pdi;
    process_p proc;
    static int count_down = 0;
    static int want_fork = 0;
   
    cpu = hdr->h_size;
    assert(cpu >= 0 && cpu <= ncpus);
    if (hdr->h_command == WANT_FORK) {
	want_fork++;
	if (hdr->h_extra == this_cpu) {
		if (this_cpu != cpu) {
			while (reqlen > 0) {
				int obid;
				t_object *o;
				memcpy(&obid, request, sizeof(int));
				reqlen -= sizeof(int);
				request += sizeof(int);
				o = GetObjectDescr(obid);
				if (o) mu_lock(&o->o_mutex);
			}
		}
		if (want_fork == 1) sema_up(&allow_fork);
		else count_down = want_fork - 1;
		proc = (process_p) hdr->h_offset;
		proc->received = 1;
		sema_up(&proc->SendDone);
	}
	if (cpu == this_cpu) {
		must_buffer = 1;
	}
	return;
    }

    if (count_down) {
	count_down--;
	if (count_down == 0) sema_up(&allow_fork);
    }
    want_fork--;
#ifdef OPTIMIZED
#ifdef CHK_FORK_MSGS
    if (chksums) {
	int a, b;
	
	memcpy(&a, p, sizeof(int));
	b = ((unsigned) a >> 16);
	a &= 0xFFFF;
	memcpy(p, &a, sizeof(int));
	a = chksum(p, reqlen) & 0xFFFF;

	if (a != b) {
		FILE *f = fopen("gotbuf", "w");
		if (f) {
			fwrite(p, 1, reqlen, f);
			fclose(f);
		}
		fprintf(file, "%d: checksum error in fork to CPU %d\n", this_cpu, cpu);
		m_syserr("checksum error");
	}
    }
#endif
    nfork++;
    memcpy(&cnt, p, sizeof(int));
    p += cnt;
#endif
    memcpy(&pdi, p, sizeof(int));
    pd = m_getptr(pdi);
    p += sizeof(int);
    
    if (cpu == this_cpu) {
	argv = m_malloc(td_nparams(pd->prc_func) * sizeof(void *));
	p = (*(pd->prc_unmarshall_args))(p, argv);
	create_obj(p, argv, pd);
    }
    
#ifdef OPTIMIZED
    process_obj_state(nfork, pd, cnt - sizeof(int), request + sizeof(int), cpu);
#endif
    
    process_queue();
    if(this_cpu == cpu) {
	create_local_process(pd, argv);
    }

    if(hdr->h_extra == this_cpu)  {
	proc = (process_p) hdr->h_offset;
	proc->received = 1;
	sema_up(&proc->SendDone);
    }
}



static void
r_start(hdr, request)
    header *hdr;
    char *request;
{
    int i;
    process_p proc;
    
    
    if(this_cpu != 0) {
	for(i=0; i < ncpus; i++) {
	    memcpy(&memport[i], request + sizeof(port) * i, sizeof(port));
	}
    }
    if(hdr->h_extra == this_cpu)  {
	proc = (process_p) hdr->h_offset;
	proc->received = 1;
	sema_up(&proc->SendDone);
    }
}


static double average;
static double deviation;
static int ntime_update;
long time_clock_diff;

static void
r_time(hdr, request)
    header *hdr;
    char *request;
{
    int i;
    process_p proc;
    unsigned long sys_milli();
    unsigned long time;
    unsigned long mytime;
    
    if(ntime_update == 0) {
	ntime_update++;
	average = 0.0;
	deviation = 0.0;
	time_clock_diff = 0;
	return;
    }
    if(this_cpu != 0) {
    	memcpy((char *) &time, request, sizeof(time));
	mytime = sys_milli();
	average += ((double) time - (double) mytime);
	deviation += ((double) time - (double) mytime) * 
					((double) time - (double) mytime);

	if(++ntime_update >= NTIME_UPDATE) {
	    average = average /(ntime_update - 1);
	    time_clock_diff = average;
	    deviation = (deviation /(ntime_update - 1)) - average * average;
	    fprintf(file, "diff = %f deviation^2 = %f time_clock_diff = %ld\n", 
		   average, deviation, time_clock_diff);
	    ntime_update = 0;
	}
    }
}


static void 
handlemess(hdr, request, reqlen, reply, replen)
    header *hdr;
    char *request;
    int reqlen;
    char **reply;
    int *replen;
{
    int i;
#ifdef OPTIMIZED
    g_seqcnt_t seqno;
#endif

    /* Handle the request included in the received packet. */

   /* if(this_cpu == 1)
	fprintf(file, "%d(%d): cmd: %d from %d\n", this_cpu, sequence_number,
	       hdr->h_command, hdr->h_extra); */

    switch (hdr->h_command) {
    case WARMUP:
	++rcvd_msgs;
	if (this_cpu != hdr->h_extra) sema_up(&rcve_one);
	break;
    case WANT_FORK :
	if (must_buffer) {
		queue_buffer(hdr, request, reqlen);
	}
	/* fall through */
    case FORK :				/* remote fork request */
	r_fork(hdr, request, reqlen);
	break;
    case UPDATE:			/* remote update request */
	if (must_buffer && ! GetObjectDescr((int)(hdr->h_size))) {
		queue_buffer(hdr, request, reqlen);
	}
	else r_update(hdr, request, reqlen);
	break;
    case OPERATION:			/* remote operation request */
#ifdef OPTIMIZED
	wait_for_sequence((g_seqcnt_t) hdr->h_offset);
#endif
	r_operation(hdr, request, reqlen, reply, replen);
	break;
#ifdef OPTIMIZED
    case HAVEACOPY:
	wait_for_sequence((g_seqcnt_t) hdr->h_offset);
	r_object(hdr, request, reqlen);
	break;
#endif
    case FINISH :			/* remote finish request */
	if(this_cpu == hdr->h_extra) {
	    sema_up(&sem_finish);
	}
	print_op_stat();
#ifdef OPTIMIZED
	print_bin_stat(SIZEBINSIZE, rpc_getreq_bin, rpc_putrep_bin,
		       rpc_putreq_bin, rpc_getrep_bin, grp_rcv_bin,
		       grp_snd_bin);
	print_obj_stat();
#endif
	sema_up(&sem_end);
	break;
    case JOIN:				/* a new member */
	memport[hdr->h_extra] = hdr->h_signature;
	break;
    case LEAVE:				/* a member leaves */
	if(hdr->h_extra == this_cpu) {
	    finish = 1;
	    if(checkpointing) cp_finish();
	    sema_up(&sem_finish);
	}
	break;
    case START:				/* start running the application */
	r_start(hdr, request);
	break;
    case TIME:
	r_time(hdr, request);
	break;
    case SAVESTATE:			/* make a check point */
	cp();
	break;
    case EXIT:				/* force RPC thread to exit */
	break;
    default :
	fprintf(stderr, "%d: Orca rts: McastHandle() receive illegal command %d\n",
	       this_cpu, hdr->h_command); 
	exit(1);
    }
}


void grp_update(request, cpu, buf, len, me)
    int request;
    int cpu;
    char *buf;
    int len;
    process_p me;
{
    /* Send operation or fork. */

    header hdr;		/* BUG: static header hdr; */
    int r;
    
    assert(me->received);
    if(freeze_process) 
	freeze();
    me->received = 0;
    do {
	nsender++;
	hdr.h_port = group;
	hdr.h_command = request;
	hdr.h_size = cpu;
	hdr.h_offset = (long) me;
	hdr.h_extra = this_cpu;
	r = grp_send(gd, &hdr, buf, len);
	if (r < 0) {
		fprintf(stderr, "%d: grp_send failed %s\n", this_cpu,
			err_why(ERR_CONVERT(r)));
		doexit(1);
	}
	INCSIZEBIN(grp_snd_bin, len);
	nsender--;
	if(freeze_process)
	    freeze();
	sema_down(&me->SendDone);
    } while(!me->received);
#ifndef PROGRAM_MUST_SWITCH
#ifndef PREEMPTIVE
    if(!blocked) {
	/* non-preemptive scheduling is a pain */
	threadswitch();
    }
#endif
#endif
    assert(me->received);
}

#ifdef TEST_SEQNO
/* Sequence number checking on RPC's.
*/
static int seqn = 1;
static mutex mu_seqnolock;
#endif

int rpc_doop(request, owner, buf, len, replen)
    int request, owner;
    char *buf;
    int len, *replen;
{
    /* send remote operation */

    header hdr;
    long r;
    int ntries = 5;

    assert(!checkpointing);
    assert(owner >=0 && owner < ncpus);
    hdr.h_port = memport[owner];
    hdr.h_command = request;
    hdr.h_extra = this_cpu;
    hdr.h_size = len;
    hdr.h_offset = sequence_number;
#ifdef TEST_SEQNO
    mu_lock(&mu_seqnolock);
    *((int *)(&(hdr.h_signature._portbytes[2]))) = seqn++;
#endif
    INCSIZEBIN(rpc_putreq_bin, len);
    r = rpc_trans(&hdr, buf, (unsigned long)len, &hdr, buf, (unsigned long)maxmesssize);
    while (r < 0 && ntries--) {
	if (r != RPC_NOTFOUND && r != RPC_TRYAGAIN) {
	    break;
	}
	fprintf(stderr, "%d: rpc_doop: retransmission ...\n", this_cpu);
    	r = rpc_trans(&hdr, buf, (unsigned long)len, &hdr, buf, (unsigned long)maxmesssize);
    }
#ifdef TEST_SEQNO
    mu_unlock(&mu_seqnolock);
#endif
    if(r < 0) {
	fprintf(stderr, "%d: rpc_doop: request = %d, rpc_trans failed %s\n", this_cpu, request,
	       err_why(r));
	fprintf(stderr, "owner = %d, len = %d, r = %d\n", owner, len, r);
	doexit(1);
    }
    INCSIZEBIN(rpc_getrep_bin, r);
    *replen = hdr.h_size;
    return(((short) hdr.h_status));
}

static void synchronize_clocks()
{
    unsigned long t, sys_milli();
    int i, j;
    int replen;
    static buf[sizeof(t)];

    for(i=1; i < ncpus; i++) {
	for(j=0; j < NTIME_UPDATE; j++) {
	    t = sys_milli();
	    memcpy(buf, &t, sizeof(t));
	    rpc_doop(TIME, i, buf, sizeof(t), &replen);
	}
    }
}


void stopgroup()
{
    /* Stop the group. */

    header hdr;
    int r;
    
    if(ncpus > 1) {
    	if (this_cpu == 0) {
		if (trace_on) synchronize_clocks();
	}
	hdr.h_port = group;
	hdr.h_command = FINISH;
	hdr.h_extra = this_cpu;
	r = grp_send(gd, &hdr, 0, 0);
	INCSIZEBIN(grp_snd_bin, 0);
	assert(r == 0);
	sema_down(&sem_finish);
    } else {
	finish = 1;
	if(checkpointing) cp_finish();
	sema_up(&sem_end);
    }
}

#ifdef TEST_SEQNO
static int received[256][6];
#endif

static
rpc_listener(param, psize)
    char *param;
    int psize;
{
    /* Thread that accepts point-to-point msgs. */

    header rhdr;
    char *rbuf;
    char *reply;
    long r;
    int replen;
    int command = 0;
    process_p me;
#ifdef PREEMPTIVE
    long oldprio;
    
    thread_set_priority(maxprio, &oldprio);
#endif
    assert(!checkpointing);
    me = GET_TASKDATA();
    rbuf = me->buf;
    while (command != EXIT) {
#ifdef TEST_SEQNO
	int sq;
	int *p;
#endif

	rhdr.h_port = my_priv_port; /* memport[this_cpu]; */
	r = rpc_getreq(&rhdr, rbuf, (unsigned long)maxmesssize);
	if(r < 0) {
	    fprintf(stderr, "%d: rpc_listener: rpc_getreq failed %s\n", this_cpu, err_why(r));
	    doexit(1);
	}
	if (r != rhdr.h_size) {
	    fprintf(stderr, "%d: rpc_listener: rpc_getreq truncation: r = %d, h_size = %d\n", this_cpu, r, rhdr.h_size);
	    doexit(1);
	}
#ifdef TEST_SEQNO
	sq = *((int *)(&rhdr.h_signature._portbytes[2]));
	if (sq < received[rhdr.h_extra][0]) {
		fprintf(stderr, "%d: received message from %d out of sequence\n",
			this_cpu, rhdr.h_extra);
	}
	p = &received[rhdr.h_extra][0];
	p[5] = p[4];
	p[4] = p[3];
	p[3] = p[2];
	p[2] = p[1];
	p[1] = p[0];
	p[0] = sq;
#endif

	command = rhdr.h_command;
	INCSIZEBIN(rpc_getreq_bin, r);
	replen = 0;
	reply = 0;
	handlemess(&rhdr, rbuf, rhdr.h_size, &reply, &replen);
	rhdr.h_size = replen;
	INCSIZEBIN(rpc_putrep_bin, replen);
	r = rpc_putrep(&rhdr, reply, (unsigned long)replen);
	if(r < 0) {
	    fprintf(stderr, "%d: rpc_listener: rpc_putrep failed %s\n", this_cpu, err_why(r));
	    doexit(1);
	}

	if (reply) m_free(reply);
    }
    terminate_current_process();
}


static
grp_listener(param, psize)
    char *param;
    int psize;
{
    /* Thread that waits on messages from the group. */

    header rhdr;
    int r;
    char *rbuf;
    process_p me;
#ifdef PREEMPTIVE
    long oldprio;
    
    thread_set_priority(maxprio, &oldprio);
#endif
    
    me = GET_TASKDATA();
    rbuf = me->buf;
    rhdr.h_port = group;

    while(!finish) {
#ifndef PREEMPTIVE
	blocked = 1;
#endif
	BcastMoreMessages = 0;
	r = grp_receive(gd, &rhdr, rbuf, maxmesssize, &BcastMoreMessages);
#ifndef PREEMPTIVE
	blocked = 0;
#endif
	if (r >= 0) {
	    INCSIZEBIN(grp_rcv_bin, r);
	    handlemess(&rhdr, rbuf, r, (char **)0, (int *)0);
	    sequence_number++;
#ifdef OPTIMIZED
	    unblock_wait_for_sequence();
#endif
	}
	else {
	    fprintf(stderr, "%d: grp_receive failed: %s\n", this_cpu, err_why(ERR_CONVERT(r)));
	    if(r == BC_ABORT)
		recover();
	}
    }
    terminate_current_process();
}


static 
cp_thread(param, psize)
    char *param;
    int psize;
{
    /* This thread issues every cp_interval seconds a checkpoint
     * of the application.
     */

    header hdr;
    interval delay;
    int s;


    while(!finish) {
	delay = cp_interval;
	sleep(delay);
	if(alive < ncpus) {	/* did the group start? */
	    continue;
	}
	if(finish) break;
	if(incarno > 1)
		cp_flush(me, incarno - 2);
	if(ncpus > 1) {
	    hdr.h_port = group;
	    hdr.h_command = SAVESTATE;
	    hdr.h_extra = this_cpu;
	    s = grp_send(gd, &hdr, (char *) 0, 0);
	    if(s != 0) fprintf(file, "cp_thread WARNING returns %d\n", s);
	    INCSIZEBIN(grp_snd_bin, 0);
	} else {
	    cp();
	}
	sema_down(&sem_cpdone);
    }
    terminate_current_process();
}
	
#ifdef OPTIMIZED
wait_for_sequence(seqno)
    g_seqcnt_t	seqno;
{
    while (seqno > sequence_number) {
	mu_lock(&mu_seq);
	if (seqno > sequence_number) {
    	    nwait_sequence++;
	    mu_unlock(&mu_seq);
    	    sema_down(&sem_wait_sequence);
	}
	else {
	    mu_unlock(&mu_seq);
	    break;
	}
    }
}


unblock_wait_for_sequence()
{
    int i;

    mu_lock(&mu_seq);
    i = nwait_sequence;
    nwait_sequence = 0;
    mu_unlock(&mu_seq);
    for (; i > 0; i--) {
	sema_up(&sem_wait_sequence);
    }
}
#endif


static int startapplication(param, psize)
    char *param;
    int psize;
{
    process_p me;
    int i;

    me = GET_TASKDATA();
    sema_init(&me->SendDone, 0);
    for(i=0; i < ncpus; i++) {
	memcpy(me->buf + sizeof(port)*i, &memport[i], sizeof(port));
    }
    grp_update(START, this_cpu, me->buf, sizeof(port) * ncpus, me);
    sema_up(&sem_start);
    terminate_current_process();
}


static void
warmup()
{
    header hdr;
    int r;
    char buf[4000];

    hdr.h_port = group;
    while (rcvd_msgs < ncpus) {
	if (rcvd_msgs == this_cpu) {
		hdr.h_command = WARMUP;
		hdr.h_extra = this_cpu;
		r = grp_send(gd, &hdr, buf, maxmesssize < 4000 ? maxmesssize : 4000);
		if (r < 0) {
			fprintf(stderr, "%d: grp_send failed %s\n", this_cpu,
				err_why(ERR_CONVERT(r)));
			doexit(1);
		}
	}
	else sema_down(&rcve_one);
    }
}


initcommunication()
{
    /* Create a group and start a listener thread to receive messages. If this
     * cpu is the first member, start also a thread that issues check points.
     */

    process_p par;
    capability cap;
    int i, j, replen;
    
    assert(ncpus <= MAXGROUP);

    finish = 0;
    alive = 0;
    if(this_cpu == 0) NoForkYet = 1;
    else NoForkYet = 0;
    group = groupcap->cap_port;
    uniqport(&my_priv_port);
    priv2pub(&my_priv_port, &memport[this_cpu]);
    sema_init(&sem_finish, 0);
    sema_init(&allow_fork, 0);
    sema_init(&sem_end, 0);
    sema_init(&sem_frozen, 0);
    sema_init(&sem_makecp, 0);
    sema_init(&sem_cpdone, 0);
    sema_init(&sem_print, 1);
    sema_init(&sem_start, 0);
    sema_init(&sem_wait_sequence, 0);
    mu_init(&mu_seq);
    mu_init(&mu_tab);
#ifdef TEST_SEQNO
    mu_init(&mu_seqnolock);
#endif
#ifdef PREEMPTIVE
    thread_get_max_priority(&maxprio);
    thread_enable_preemption();
#endif
    if(checkpointing && this_cpu == 0) {
	create_rts_process(cp_thread);
    }
    if(checkpointing) {
	cp_init();
    }
    if(ncpus > 1) {
	if(!checkpointing) {
	    i = n_rpc_listeners;
	    if (i > MAX_RPC_THREADS) i = MAX_RPC_THREADS;
	    for(; i > 0; i--) {
		create_rts_process(rpc_listener);
#ifndef PREEMPTIVE
	    	threadswitch();
#endif
	    }
	}
	/* Race condition fixed? What happens if the grp_listener is already
	   present but the rpc_listeners are not, and an object becomes
	   non-replicated?
	   Fix: create rpc_listeners first.(?)
	*/
    	startmember();
    	buildgroup();
	create_rts_process(grp_listener);
	warmup();
    	if(this_cpu == 0) {
	    if(trace_on) synchronize_clocks();
	    create_rts_process(startapplication);
	    sema_down(&sem_start);
    	}
    } else {
	alive = 1;
    }
}

void endcommunication()
{
    /* Finish off. */
    
    header hdr;
    long r;
    int i;
    
    if(ncpus > 1) {
	hdr.h_extra = this_cpu;
	hdr.h_command = LEAVE;
	hdr.h_port = group;
	r = grp_leave(gd, &hdr);
	assert(r == 0);
	sema_down(&sem_finish);
	if(!checkpointing) {
	    i = n_rpc_listeners;
	    if (i > MAX_RPC_THREADS) i = MAX_RPC_THREADS;
	    for(; i > 0; i--) {
		hdr.h_port = memport[this_cpu];
		hdr.h_command = EXIT;
#ifdef TEST_SEQNO
    		*((int *)(&hdr.h_signature._portbytes[2])) = seqn++;
#endif
		r = rpc_trans(&hdr, 0, (unsigned long)0, &hdr, 0, (unsigned long)0);
		if(r < 0) {
		    fprintf(stderr, "%d: endcommunication: rpc_trans failed %s\n",
			    this_cpu, err_why(ERR_CONVERT(r)));
		    doexit(1);
		}
	    }
	}

    }
}
