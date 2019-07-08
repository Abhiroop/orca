/* All threads that perform Orca operations need some glocal data. This
 * glocal data is stored in structures of type process_t. This module
 * manages these structures. We need to distinguish between threads created
 * through Orca FORK statements and RTS threads that handle incoming
 * messages. The RST threads don't use all the fields in the process_t
 * descriptor and cannot explicitly initialise such structures. Therefore,
 * p_self() checks if a structure needs to be allocted for the thread
 * that calls it.
 */
#include "panda/panda.h"
#include "process.h"
#include "fork_exit.h"
#include "rts_trace.h"


#define DELTA_THREADS 4             /* Length incr. of op_threads array     */

static sys_key_t orca_process_key;  /* glocal data key                      */

static process_p *op_threads;       /* thread descrs for operation threads  */
static unsigned maxthreads;         /* current length of op_threads         */
static unsigned nthreads = 0;       /* no. of thread descrs in op_threads   */

static void *startproc(void *arg);  /* wrapper thread for Orca processes    */



/* p_init: create a new operation thread descriptor. If tid equals
 * (thread_t)0 then a matching RTS thread already exists. Otherwise
 * create one.
 */
process_p
p_init(thread_t tid, prc_dscr *orca_descr, void **argv)
{
    process_p proc = (process_p)sys_malloc(sizeof(process_t));

    proc->pdscr    = orca_descr;
    proc->argv     = argv;
    proc->blocking = 0;
    proc->depth    = 0;

    if (nthreads == maxthreads) {
	maxthreads += DELTA_THREADS;
	op_threads = (process_p *)sys_realloc(op_threads,
					      maxthreads * sizeof(process_p));
    }
    op_threads[nthreads++] = proc;
    
    if (tid) {
	proc->tid = tid;
    } else {
	sys_thread_create(&proc->tid, startproc, proc, 0, 0);
    }

    return proc;
}


/* p_self: return a thread's glocal process structure. Create one
 * if necessary.
 */
process_p 
p_self(void)
{
    process_p self;

    sys_getspecific(orca_process_key, (void **)&self);;
    if (!self) {
	/* This is not an Orca process thread.
	 */
	self = p_init(sys_thread_self(), NULL, NULL);
	sys_setspecific(orca_process_key, self);
    }
    return self;
}


/* p_pop_object: returns whether the operation just executed blocked.
 * Resets the blocking flag when this was the top-level operation.
 */
int 
p_pop_object(process_p self) 
{
    int ret = !(self->blocking);

    if (--self->depth == 0) {
	self->blocking = 0;
    }
    return ret;
}


/* startproc: wrapper thread function for Orca "processes."
 */
static void *
startproc( void *arg)
{
    process_p self = (process_p)arg;
    
    sys_setspecific(orca_process_key, self);

    trc_new_thread(0, self->pdscr->prc_trc_name);
    trc_event(proc_create, str2traceinfo( self->pdscr->prc_trc_name));

    /* Run the Orca process.
     */
    (*(self->pdscr->prc_name))(self->argv);
    
    trc_event( proc_exit, str2traceinfo( self->pdscr->prc_trc_name));

    /* Clean up.
     */
    DoExit( self->pdscr, self->argv);
    p_clear(self);
    sys_setspecific(orca_process_key, NULL);
    sys_thread_exit(NULL);

    return NULL;
}


void 
p_start(void)
{
    sys_key_create(&orca_process_key);
    maxthreads = DELTA_THREADS;
    op_threads = sys_malloc(maxthreads * sizeof(process_p));
}


void 
p_end(void) 
{
    unsigned i;

    for (i = 0; i < nthreads; i++) {
	sys_free(op_threads[i]);
    }
    sys_free(op_threads);
}
