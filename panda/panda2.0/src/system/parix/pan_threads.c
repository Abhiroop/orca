/*
 * Upgrade to Parix 1.2:
 * There is now support for glocal data:
 * void *GetLocal(void)and SetLocal(void *).*
 * For each thread, the Parix glocal pointer is set to indicate the Panda
 * thread struct. One field in the Panda thread struct is the Parix thread
 * pointer; this is necessary for joining.
 *
 * 20/3/1995 Rutger
 */



#include "pan_sys.h"

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_threads.h"

#include "pan_parix.h"

#ifdef PARIX_T800
#define DEFAULT_STACKSIZE  (1024 * 10)	/* Maybe too small!!!!! */
#else
#define DEFAULT_STACKSIZE  (1024 * 64)
#endif
#define DEFAULT_PRIORITY  LOW_PRIORITY	/* is this correct ???? */

#define SYS_THREAD_JOIN_ME     345634
#define SYS_THREAD_DETACH_ME   233223

#ifdef pan_thread_self
#  undef pan_thread_self
#endif

static Thread_t          *detacher_thread;
static Thread_t          *last_thread_thread;
static Semaphore_t        last_thread_block;
static int                run_detach_thread;
static int                threads_to_detach;	/* unprotected counter? */
static struct pan_thread  main_thread;




/*ARGSUSED*/
static int
detacher_function(void *arg)
{
    Thread_t   *anyone;
    int         result;

    while (run_detach_thread || threads_to_detach > 0) {
	anyone = WaitThread(NULL, &result);
	if (result == SYS_THREAD_DETACH_ME) {
	    WaitThread(anyone, &result);
	    /* pan_free()of thread structure: elsewhere */
	    threads_to_detach--;
	}
    }
    return SYS_THREAD_JOIN_ME;
}


/* this thread blocks until it is awaked to awake the detacher: */

/*ARGSUSED*/
static int
last_thread_function(void *arg)
{
    Wait(&last_thread_block);
    return SYS_THREAD_DETACH_ME;
}


/*
  pan_thread_start: Should be called before any other routine in this module.
  Initialises some data structures and starts 2 threads.
*/

void
pan_sys_thread_start(void)
{
    int         error, i;

    InitSem(&last_thread_block, 0);
    run_detach_thread = 1;
    threads_to_detach = 1;
    detacher_thread = CreateThread(NULL, 2048, detacher_function, &error, NULL);
    if (detacher_thread == NULL) {
	pan_panic("cannot start thread; free heap space = %d\n", mallinfo().freearena);
    }
    last_thread_thread = CreateThread(NULL, 1024, last_thread_function, &error,
				      NULL);
    if (last_thread_thread == NULL) {
	pan_panic("cannot start thread; free heap space = %d\n", mallinfo().freearena);
    }
    SetLocal(&main_thread);
    
    /*
     * Don't bother to set parix_identification: main thread cannot be joined
     * RFHH
     */


    for (i = 0; i < PANDA_DATAKEYS_MAX; i++)
	main_thread.glocal[i] = NULL;
    main_thread.detach = 0;
    main_thread.priority = DEFAULT_PRIORITY;
    main_thread.wake_up_next = NULL;
    main_thread.wake_up_link[0] = NULL;
    main_thread.wake_up_link[1] = NULL;
    InitSem(&main_thread.wake_up_sema, 0);
}


/* pan_thread_end: Should be called to end thread usage */

void
pan_sys_thread_end(void)
{
    int         result;

    run_detach_thread = 0;
    Signal(&last_thread_block);
    WaitThread(detacher_thread, &result);
}


static int
generic_wrapper_function(void *arg)
{
    int          i;
    pan_thread_p myself = arg;

    SetLocal(myself);

    for (i = 0; i < PANDA_DATAKEYS_MAX; i++)
	myself->glocal[i] = NULL;

    myself->wake_up_next = NULL;
    myself->wake_up_link[0] = NULL;
    myself->wake_up_link[1] = NULL;
    InitSem(&myself->wake_up_sema, 0);

    ChangePriority(myself->priority);

    i = setjmp(myself->my_jmp_buf);
    if (i == 0) {
	myself->panda_func(myself->panda_arg);
				/* Result delivered via thread_exit() RFHH */
    }

    ChangePriority(LOW_PRIORITY);

    if (myself->detach) {
	pan_free(myself);
	return SYS_THREAD_DETACH_ME;
    } else
	return SYS_THREAD_JOIN_ME;
}

void
pan_thread_exit(void)
{
    pan_thread_p    myself;

    myself = (pan_thread_p)GetLocal();

    if (myself == &main_thread) {
	ChangePriority(LOW_PRIORITY);
	pan_end();
	exit(1);
    }
    longjmp(myself->my_jmp_buf, 1);
}


pan_thread_p
pan_thread_create(pan_thread_func_p func, void *actual_arg, long stacksize,
		  int priority, int detach)
{
    pan_thread_p  thread;
    int           actual_stack_size;
    int           error;
    Thread_t     *actual_thread;

    if (stacksize == 0L)
	actual_stack_size = DEFAULT_STACKSIZE;
    else
	actual_stack_size = (int)stacksize;

    if (priority == 0)
	priority = DEFAULT_PRIORITY;	/* LOW_PRIORITY == 1 */
    else
	priority = 0;		/* HIGH_PRIORITY == 0 */

    thread = pan_malloc(sizeof(struct pan_thread));
    thread->detach = detach;
    thread->priority = priority;
    thread->panda_func = func;
    thread->panda_arg = actual_arg;

    actual_thread = CreateThread(NULL, actual_stack_size,
				 generic_wrapper_function, &error,
				 thread);
    if (actual_thread == NULL) {
	pan_panic("cannot start thread; free heap space = %d\n", mallinfo().freearena);
    }
    if (!detach)
	thread->parix_identification = actual_thread;
    else
	threads_to_detach++;
    return thread;
}

void
pan_thread_join(pan_thread_p thread)
{
    int         result;

    if (thread == NULL || thread->detach == 1) {
	pan_panic("cannot join detached thread");
    }
    WaitThread(thread->parix_identification, &result);
    if (result != SYS_THREAD_JOIN_ME) {
	pan_panic("cannot find thread struct %x, %x in list\n",
		  thread, thread->parix_identification);
    }
}

void
pan_thread_clear(pan_thread_p thread)
{
    pan_free(thread);
}

void
pan_thread_yield(void)
{
    /* yield is still in the interface? */
}

pan_thread_p
pan_thread_self(void)
{
    return (pan_thread_p)GetLocal();
}

int
pan_thread_getprio(void)
{
    int         r, s;

    r = GetPriority();		/* undocumented! */

    s = (((pan_thread_p)(GetLocal()))->priority ? 0 : 1);

    return s;
}


int
pan_thread_setprio(int priority)
{
    int           r, s;
    pan_thread_p  me;

    me = (pan_thread_p)GetLocal();
    s = (me->priority ? 0 : 1);

    if (priority == 0)
	priority = DEFAULT_PRIORITY;	/* LOW_PRIORITY == 1 */
    else
	priority = 0;		/* HIGH_PRIORITY == 0 */
    r = ChangePriority(priority);
    me->priority = priority;

    return s;
}


int
pan_thread_maxprio(void)
{
    return 0;
}


int
pan_thread_minprio(void)
{
    return 0;
}
