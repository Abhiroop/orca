#include <assert.h>
#include "panda/panda.h"
#include "thrpool.h"

#include "trace/trace.h"

#define TP_SIZE	       sys_nr_platforms

#include <string.h>

static void *
thrpool_worker(void *arg)
{
    thrpool_t *tp = (thrpool_t *)arg;
    tp_job_t *job;

#ifdef TRACING
    char buf[32];
#endif
    
#ifdef TRACING
    /* No number available, so use local stack for identification */
    sprintf(buf, "worker thread %p", &tp);
    trc_new_thread( 0, buf);
#endif

    sys_mutex_lock(&tp->tp_lock);
    while (!tp->tp_done) {

        if (tp->tp_head == (tp_job_t *)0) {
	    /*
	     * Job list is empty. Block and wait for a new job.
	     */
	    tp->tp_idle++;
	    sys_cond_wait(&tp->tp_cond, &tp->tp_lock);
	    tp->tp_idle--;
	    continue;			/* check tp_done too */
	}

	job = tp->tp_head;   /* head of job list */
	tp->tp_head = job->tpj_next;

	sys_mutex_unlock(&tp->tp_lock);
	(*tp->tp_upcall)(&job->tpj_buf);
	sys_mutex_lock(&tp->tp_lock);

	job->tpj_next   = tp->tp_freelist;
	tp->tp_freelist = job;
    }
    sys_mutex_unlock(&tp->tp_lock);
    sys_thread_exit((void *)0);

    /* NOT REACHED (keep gcc happy) */
    return (void *)0;
}


void
add_workers(thrpool_t *tp)
{
    int i, incr;

    incr = tp->tp_increment;
    if (tp->tp_workers + incr > tp->tp_maxsize) {
	incr = tp->tp_maxsize - tp->tp_workers;
    }

    for (i = 0; i < incr; i++) {
	sys_thread_create(&tp->tp_id[tp->tp_workers++], thrpool_worker,
			  (void *) tp, 0, tp->tp_priority);
    }
}

void
add_jobs(thrpool_t *tp)
{
    tp_job_t *job;
    int i;
    
    assert(tp->tp_freelist == 0);
    for (i = 0; i < TP_SIZE;i++) {
	job = (tp_job_t *)sys_malloc(sizeof(tp_job_t));
	job->tpj_next   = tp->tp_freelist;
	tp->tp_freelist = job;
    }
}

void
thrpool_init(thrpool_t *tp, thrpool_func_p upcall, int jobsize,
	     int maxsize, int increment, int prio)
{
    sys_mutex_init(&tp->tp_lock);
    sys_mutex_lock(&tp->tp_lock);

    assert(jobsize <= TP_MAXJOBSIZE);

    tp->tp_priority  = prio;
    tp->tp_maxsize   = maxsize;
    tp->tp_increment = increment;
    tp->tp_upcall    = upcall;

    sys_cond_init(&tp->tp_cond);
    tp->tp_done      = 0;

    tp->tp_jobsize   = jobsize;
    tp->tp_head      = (tp_job_t *)0;
    tp->tp_tail      = (tp_job_t *)0;
    tp->tp_freelist  = 0;

    tp->tp_id 	     = (thread_p) sys_malloc(maxsize * sizeof(thread_t));
    tp->tp_workers   = 0;
    tp->tp_idle      = 0;
    
    add_jobs(tp);
    add_workers(tp);
    sys_mutex_unlock(&tp->tp_lock);
}


void
thrpool_clear(thrpool_t *tp)
{
    tp_job_t *job, *next;
    int i;

    sys_mutex_lock(&tp->tp_lock);
    tp->tp_done = 1;
    sys_cond_broadcast(&tp->tp_cond);
    sys_mutex_unlock(&tp->tp_lock);

    for (i = 0; i < tp->tp_workers; i++) {
	sys_thread_join(&tp->tp_id[i]);
    }

    for (job = tp->tp_freelist; job; job = next) {
	next = job->tpj_next;
	sys_free(job);
    }
    
    if (tp->tp_head) {
	printf("%d) (warning) jobs left in job queue\n", sys_my_pid);
    }

    sys_free(tp->tp_id);
    sys_cond_clear(&tp->tp_cond);
    sys_mutex_clear(&tp->tp_lock);
}

