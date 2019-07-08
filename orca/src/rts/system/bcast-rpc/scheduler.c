/* $Id: scheduler.c,v 1.10 1996/07/04 08:53:02 ceriel Exp $ */

#include "amoeba.h"
#include "thread.h"
#include "interface.h"
#include "semaphore.h"
#include "scheduler.h"
#include "module/mutex.h"

extern FILE *file;
extern int stacksize;

static process_p	processlist;	/* list of all orca processes */
static mutex		mu_proc;	/* mutex to protect proc table */


void initproc()
{
     mu_init(&mu_proc);
}


static void printproclist()
{
    process_p x;

    fprintf(file, "proclist %X (cpu %d)\n", processlist, this_cpu);
    for(x = processlist; x != 0; x = x->next) {
	fprintf(file, "pid %d, p_id %08x\n", x->descr->prc_func, x->process_id);
    }
    fflush(file);
}


void print_proc_list()
{
    mu_lock(&mu_proc);
    printproclist();
    mu_unlock(&mu_proc);
}


wakeup_sendingprocess()
{
    process_p x;

    for(x = processlist; x != 0; x = x->next) {
	if(!x->received) {
#ifdef DEBUGHACK
	    assert(sema_level(&x->SendDone) == 0);
#endif
	    sema_up(&x->SendDone);
	}
    }
}


go_to_sleep(me)
    process_p me;
{
    sema_down(&me->suspend);
}


local_wakeup(p)
    process_p p;
{
#ifdef DEBUGHACK
    assert(sema_level(&p->suspend) == 0);
#endif
    sema_up(&p->suspend);
}

void
startproc(p)
    process_p p;
{
#ifdef PREEMPTIVE
    long oldprio;

    thread_set_priority(0L, &oldprio);
#endif
    (*(p->descr->prc_name))(p->args);
    terminate_current_process();
    thread_exit();
}


cleanup_task()
{
    register process_p cp = GET_TASKDATA();

    free_args(cp->descr, cp->args, 0); /* free argument vector */
}


static int curpid = 1;
extern int maxmesssize;

/* Create an Orca process. */
create_local_process(descr, args)
    prc_dscr *descr;
    void **args;
{
    process_p par;
    
    mu_lock(&mu_proc);

    par = (process_p) malloc(sizeof(struct process));
	/* Use malloc because system cleans up glocal area */
    par->descr = descr;
    par->args = args;
    par->process_id = curpid++;
    par->buf = m_malloc(maxmesssize);
    par->received = 1;
    par->blocking = 0;
    par->DoingOperation = 0;
    assert(par->buf);
    sema_init(&par->SendDone, 0);
    sema_init(&par->suspend, 0);

    /* append it to the process list */
    par->next = processlist;
    processlist = par;
    thread_newthread(startproc, stacksize, (char *)par, sizeof(struct process));

    mu_unlock(&mu_proc);
}


/* Create an rts process. */
create_rts_process(func)
    void (*func)();
{
    process_p par;
    
    mu_lock(&mu_proc);

    par = (process_p) malloc(sizeof(struct process));
	/* Use malloc because system cleans up glocal area */
    assert(par);
    par->descr = 0;
    par->blocking = 0;
    par->process_id = curpid++;
    par->buf = m_malloc(maxmesssize);
    par->received = 1;
    par->DoingOperation = 0;
    assert(par->buf);
    sema_init(&par->SendDone, 0);
    sema_init(&par->suspend, 0);

    /* append it to the process list */
    par->next = processlist;
    processlist = par;

    thread_newthread(func, stacksize, (char *)par, sizeof(struct process));

    mu_unlock(&mu_proc);
}


terminate_current_process()
{
    process_p x, y;
    process_p proc = GET_TASKDATA();

    mu_lock(&mu_proc);

    if(proc->descr != 0)	{ /* is it an Orca process */
	cleanup_task();
    }

    /* remove it from the process list */
    for(x = processlist, y = 0; x != 0; y = x, x = x->next) {
	if(x->process_id == proc->process_id) {
	    if(y == 0) {	/* is this first entry in the list? */
		processlist = x->next;
	    } else {
		y->next = x->next;
	    }
	    m_free(proc->buf);
	    break;
	}
    }

    mu_unlock(&mu_proc);
    thread_exit();
}
