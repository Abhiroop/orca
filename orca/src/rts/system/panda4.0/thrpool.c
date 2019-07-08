/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "pan_sys.h"
#include "pan_trace.h"
#include "thrpool.h"

#define TP_SIZE	       rts_nr_platforms

#include <string.h>

static void
thrpool_worker(void *arg)
{
    thrpool_t *tp = (thrpool_t *)arg;
    tp_job_t *job;

    pan_mutex_lock(tp->tp_lock);
    while (!tp->tp_done) {

	if (tp->tp_head == (tp_job_t *)0) {
	    /*
	     * Job list is empty. Block and wait for a new job.
	     */
	    tp->tp_idle++;
	    pan_cond_wait(tp->tp_cond);
	    tp->tp_idle--;
	    continue;			/* check tp_done too */
	}

	job = tp->tp_head;   /* head of job list */
	tp->tp_head = job->tpj_next;

	pan_mutex_unlock(tp->tp_lock);
	(*tp->tp_upcall)(&job->tpj_buf);
	pan_mutex_lock(tp->tp_lock);

	job->tpj_next   = tp->tp_freelist;
	tp->tp_freelist = job;
    }
    pan_mutex_unlock(tp->tp_lock);
    pan_thread_exit();
}


void
add_workers(thrpool_t *tp)
{
    char buf[32];
    int i, incr;

    incr = tp->tp_increment;
    if (tp->tp_workers + incr > tp->tp_maxsize) {
	incr = tp->tp_maxsize - tp->tp_workers;
    }

    for (i = 0; i < incr; i++) {
	sprintf(buf, "worker thread %d", i);
	tp->tp_id[tp->tp_workers++] =
	      pan_thread_create(thrpool_worker,
				(void *) tp, 0, tp->tp_priority, 0, buf);
    }
}

void
add_jobs(thrpool_t *tp)
{
    tp_job_t *job;
    int i;
    
    assert(tp->tp_freelist == 0);
    for (i = 0; i < TP_SIZE;i++) {
	job = (tp_job_t *)m_malloc(sizeof(tp_job_t));
	job->tpj_next   = tp->tp_freelist;
	tp->tp_freelist = job;
    }
}

void
thrpool_init(thrpool_t *tp, thrpool_func_p upcall, int jobsize,
	     int maxsize, int increment, int prio)
{
    tp->tp_lock = pan_mutex_create();
    pan_mutex_lock(tp->tp_lock);

    assert(jobsize <= TP_MAXJOBSIZE);

    tp->tp_priority  = prio;
    tp->tp_maxsize   = maxsize;
    tp->tp_increment = increment;
    tp->tp_upcall    = upcall;

    tp->tp_cond = pan_cond_create(tp->tp_lock);
    tp->tp_done      = 0;

    tp->tp_jobsize   = jobsize;
    tp->tp_head      = 0;
    tp->tp_tail      = 0;
    tp->tp_freelist  = 0;

    tp->tp_id 	     = (pan_thread_p *)m_malloc(maxsize * sizeof(pan_thread_p));
    tp->tp_workers   = 0;
    tp->tp_idle      = 0;
    
    add_jobs(tp);
    add_workers(tp);
    pan_mutex_unlock(tp->tp_lock);
}


void
thrpool_clear(thrpool_t *tp)
{
    tp_job_t *job, *next;
    int i;

    pan_mutex_lock(tp->tp_lock);
    tp->tp_done = 1;
    pan_cond_broadcast(tp->tp_cond);
    pan_mutex_unlock(tp->tp_lock);

    for (i = 0; i < tp->tp_workers; i++) {
	pan_thread_join(tp->tp_id[i]);
    }

    for (job = tp->tp_freelist; job; job = next) {
	next = job->tpj_next;
	m_free(job);
    }
    
    if (tp->tp_head) {
	printf("%d) (warning) jobs left in job queue\n", rts_my_pid);
    }

    m_free(tp->tp_id);
    pan_cond_clear(tp->tp_cond);
    pan_mutex_clear(tp->tp_lock);
}

