#include "pan_sys.h"		/* Provides a system interface */

#include "pan_system.h"
#include "pan_sync.h"
#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_threads.h"
#include "pan_time.h"

#include "pan_parix.h"

#ifdef PARIX_T800
#include <sys/rlink.h>
#endif
#include <sys/select.h>
#include <stdio.h>
#include <assert.h>

#undef pan_mutex_lock
#undef pan_mutex_unlock
#undef pan_mutex_trylock

/* wake_up_modus  of  pan_cond_t: */
#define SEMA_MODUS 1		/* wake up by Signal on Semaphore */
#define LINK_MODUS 2		/* wake up by Send on Link */
/* special value for count of an invalid pan_cond_t: */
#define COND_INVALID  (-1)

void
pan_sys_sync_start(void)
{
}

void
pan_sys_sync_end(void)
{
}


pan_mutex_p
pan_mutex_create(void)
{
    pan_mutex_p mutex;
    
    mutex = pan_malloc(sizeof(struct pan_mutex));
    InitSem(&mutex->sema, 1);
    return mutex;
}

void
pan_mutex_clear(pan_mutex_p mutex)
{
    if (! TestWait(&mutex->sema)) {
	pan_panic("cannot free locked mutex");
    }
    if (TestWait(&mutex->sema)) {
	pan_panic("detected improper use of mutex");
    }
    pan_free(mutex);
}

void 
pan_mutex_lock(pan_mutex_p mutex)
{
    Wait(&mutex->sema);
}


void 
pan_mutex_unlock(pan_mutex_p mutex)
{
    Signal(&mutex->sema);
}

int
pan_mutex_trylock(pan_mutex_p mutex)
{
    return TestWait(&mutex->sema);
}

pan_cond_p
pan_cond_create(pan_mutex_p mutex)
{
    pan_cond_p cond;

    cond = pan_malloc(sizeof(struct pan_cond));
    /* initialize List of waiting threads: */
    cond->thread_list_head = NULL;
    cond->thread_list_tail = NULL;
    cond->count = 0;
    cond->monitor = &mutex->sema;

    return cond;
}

void
pan_cond_clear(pan_cond_p cond)
{
    /* check if List of waiting threads is empty: */
    if (cond->count != 0 ||
	    cond->thread_list_head != NULL ||
	    cond->thread_list_tail != NULL) {
	pan_panic("cannot clear cond_var in use");
    }
    cond->count = COND_INVALID;
    pan_free(cond);
}

void
pan_cond_wait(pan_cond_p cond)
{
    int          old_priority;
    pan_thread_p myself;

    /* determine yourself: */
    myself = pan_thread_self();

    /* add yourself to List of waiting threads: */
    myself->wake_up_next = NULL;
    myself->wake_up_modus = SEMA_MODUS;
    myself->result = NULL;	/* Trick 17 */
    old_priority = ChangePriority(HIGH_PRIORITY);

    if (cond->thread_list_tail == NULL)
	cond->thread_list_tail = cond->thread_list_head = myself;
    else {
	cond->thread_list_tail->wake_up_next = myself;
	cond->thread_list_tail = myself;
    }
    cond->count++;

    /* and wait until being signaled: */
    Signal(cond->monitor);
    Wait(&myself->wake_up_sema);
    ChangePriority(old_priority);

    Wait(cond->monitor);
}

/* Trick 17: force right decisions, even if THE TIMEOUT occurs during
	     the wake-up process!
*/

int
pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime)
{
    int          old_priority, timeout, feedback, een_int;
    pan_thread_p myself, previous;
    Option_t    timeout_option, signal_option;

    /* determine yourself: */
    myself = pan_thread_self();

    /* add yourself to List of waiting threads: */
    myself->wake_up_next = NULL;
    myself->wake_up_modus = LINK_MODUS;
    myself->result = NULL;	/* Trick 17 */
    if (LocalLink(myself->wake_up_link) < 0) {
	pan_panic("cannot create local link for timedwait");
    }
    signal_option = ReceiveOption(myself->wake_up_link[0]);
    old_priority = ChangePriority(HIGH_PRIORITY);

    if (cond->thread_list_tail == NULL) {
	previous = NULL;
	cond->thread_list_tail = cond->thread_list_head = myself;
    } else {
	cond->thread_list_tail->wake_up_next = myself;
	previous = cond->thread_list_tail;
	cond->thread_list_tail = myself;
    }
    cond->count++;
    timeout_option = TimeAfterOption(TRANSTIME(abstime));

    /* wait until being signaled or timeout: */
    Signal(cond->monitor);
    timeout = Select(2, signal_option, timeout_option);
    if (timeout == 0 || myself->result != NULL) {	/* Trick 17 */
	feedback = RecvLink(myself->wake_up_link[0], &een_int, sizeof(int));
	if (feedback == -1) {
	    ChangePriority(old_priority);
	    pan_panic("error in receive on timedwait link");
	}
    } else {			/* Timeout: delete yourself from List of
				 * waiting threads */
	{
	    /* TIM added. Compute previous again */
	    pan_thread_p p;
	    
	    previous = NULL;
	    for (p = cond->thread_list_head; p != myself; p = p->wake_up_next){
		previous = p;
	    }
	}
		
	if (previous == NULL) {
	    if (cond->thread_list_tail == cond->thread_list_head)
		cond->thread_list_tail = cond->thread_list_head = NULL;
	    else
		cond->thread_list_head = myself->wake_up_next;
	} else
	    previous->wake_up_next = myself->wake_up_next;
	cond->count--;
    }
    assert(cond->count > 0 || 
	   (cond->thread_list_head == NULL && cond->thread_list_tail == NULL));
    assert(cond->count != 1 || 
	   cond->thread_list_head == cond->thread_list_tail);

    ChangePriority(old_priority);

    Wait(cond->monitor);

    if (BreakLink(myself->wake_up_link[0]) < 0 ||
	    BreakLink(myself->wake_up_link[1]) < 0) {
	pan_panic("cannot destroy timedwait link");
    }
    return timeout ? 0 : 1;
}

void
pan_cond_signal(pan_cond_p cond)
{
    int          old_priority, feedback, een_int;
    pan_thread_p het_process;

    old_priority = ChangePriority(HIGH_PRIORITY);

    het_process = cond->thread_list_head;
    if (het_process != NULL) {
	/* delete thread from List of waiting threads: */
	if (cond->thread_list_head == cond->thread_list_tail)
	    cond->thread_list_head = cond->thread_list_tail = NULL;
	else
	    cond->thread_list_head = cond->thread_list_head->wake_up_next;
	cond->count--;

	if (het_process->wake_up_modus == SEMA_MODUS) {
	    Signal(&het_process->wake_up_sema);
	} else {
	    if (het_process->wake_up_modus != LINK_MODUS) {
		ChangePriority(old_priority);
		pan_panic("cond signal: illegal wake_up_modus");
	    }
	    het_process->result = (void *)het_process;	/* Trick 17 */
	    feedback = SendLink(het_process->wake_up_link[1],
				&een_int, sizeof(int));
	    if (feedback == -1) {
		ChangePriority(old_priority);
		pan_panic("cond signal: send error on local link");
	    }
	}
    }
    ChangePriority(old_priority);
}

void
pan_cond_broadcast(pan_cond_p cond)
{
    int          old_priority, feedback, een_int;
    pan_thread_p het_process;

    old_priority = ChangePriority(HIGH_PRIORITY);

    het_process = cond->thread_list_head;
    while (het_process != NULL) {
	/* delete thread from List of waiting threads: */
	if (cond->thread_list_head == cond->thread_list_tail)
	    cond->thread_list_head = cond->thread_list_tail = NULL;
	else
	    cond->thread_list_head = cond->thread_list_head->wake_up_next;
	cond->count--;

	if (het_process->wake_up_modus == SEMA_MODUS) {
	    Signal(&het_process->wake_up_sema);
	} else {
	    if (het_process->wake_up_modus != LINK_MODUS) {
		ChangePriority(old_priority);
		pan_panic("cond signal: illegal wake_up_modus");
	    }
	    het_process->result = (void *)het_process;	/* Trick 17 */
	    feedback = SendLink(het_process->wake_up_link[1],
				&een_int, sizeof(int));
	    if (feedback == -1) {
		ChangePriority(old_priority);
		pan_panic("cond signal: send error on local link");
	    }
	}
	het_process = cond->thread_list_head;
    }
    ChangePriority(old_priority);
}
