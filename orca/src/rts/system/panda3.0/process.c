/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* All threads that perform Orca operations need some glocal data. This
 * glocal data is stored in structures of type process_t. This module
 * manages these structures. We need to distinguish between threads created
 * through Orca FORK statements and RTS threads that handle incoming
 * messages. The RST threads don't use all the fields in the process_t
 * descriptor and cannot explicitly initialise such structures.
 */
#ifdef PURE_WRITE
#include <assert.h>
#include <stdio.h>
#endif
#include "pan_sys.h"
#include "process.h"
#include "fork_exit.h"
#include "rts_trace.h"


#define DELTA_THREADS 4             /* Length incr. of op_threads array     */

static pan_key_p orca_process_key;  /* glocal data key                      */

static process_p *op_threads;       /* thread descrs for operation threads  */
static unsigned maxthreads;         /* current length of op_threads         */
static unsigned nthreads = 0;       /* no. of thread descrs in op_threads   */

static void startproc(void *arg);   /* wrapper thread for Orca processes    */



/* p_init: create a new operation thread descriptor. If tid equals
 * (thread_t)0 then a matching RTS thread already exists. Otherwise
 * create one.
 */
process_p
p_init(pan_thread_p tid, prc_dscr *orca_descr, void **argv)
{
    process_p proc = m_malloc(sizeof(process_t));

    proc->pdscr    = orca_descr;
    proc->argv     = argv;

    if (nthreads == maxthreads) {
	maxthreads += DELTA_THREADS;
	op_threads = m_realloc(op_threads, maxthreads * sizeof(process_p));
    }
    op_threads[nthreads++] = proc;
    
    if (tid) {
	proc->tid = tid;
    } else {
					/* prio 0 has lost its special meaning:
					 * now explicitly specify min_prio.
					 * detach since join is too difficult.
					 *				RFHH */
	proc->tid = pan_thread_create(startproc, proc, 0, pan_thread_minprio(),
				      1, proc->pdscr->prc_trc_name);
    }

#ifdef PURE_WRITE
    proc->p_pipelined = 0;
    proc->p_nwrites = 0;
    proc->p_flushed = rts_cond_create();
#endif

    return proc;
}


void
p_clear(process_p proc)
{
#ifdef PURE_WRITE
    assert(!rts_trylock());

    while(proc->p_nwrites > 0) {
	pan_cond_wait(proc->p_flushed);
    }

    printf("%d) Orca process %p: pipelined %d writes\n", rts_my_pid, proc,
	   proc->p_pipelined);

    rts_cond_clear(proc->p_flushed);
#endif
}


/*
 * p_self: return a thread's glocal process structure. Create one
 * if necessary.
 */
process_p 
p_self(void)
{
    process_p self;

    self = (process_p)pan_key_getspecific(orca_process_key);
    if (!self) {
	/* This is not an Orca process thread.
	 */
	self = p_init(pan_thread_self(), 0, 0);
	pan_key_setspecific(orca_process_key, self);
    }
    return self;
}


/*
 * startproc: wrapper thread function for Orca "processes."
 */
static void
startproc(void *arg)
{
    process_p self = (process_p)arg;

    rts_lock();

    pan_key_setspecific(orca_process_key, self);

    trc_event(proc_create, str2traceinfo( self->pdscr->prc_trc_name));

    /*
     * Run the Orca process.
     */
    rts_unlock();
    (*(self->pdscr->prc_name))(self->argv);
    trc_event( proc_exit, str2traceinfo( self->pdscr->prc_trc_name));
    DoExit( self->pdscr, self->argv);
    rts_lock();

    p_clear(self);
    pan_key_setspecific(orca_process_key, NULL);

    rts_unlock();
    pan_thread_exit();
}


void 
p_start(void)
{
    orca_process_key = pan_key_create();
    maxthreads = DELTA_THREADS;
    op_threads = m_malloc(maxthreads * sizeof(process_p));
}


void 
p_end(void) 
{
    unsigned i;

    for (i = 0; i < nthreads; i++) {
	m_free(op_threads[i]);
    }
    m_free(op_threads);
}
