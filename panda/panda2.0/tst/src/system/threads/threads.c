#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"

#ifndef FALSE
#  define FALSE 0
#endif
#ifndef TRUE
#  define TRUE 1
#endif


/*
 * Test 0: test thread interface.
 *
 * BUG: test signals
 */

#define TIMEOUT		10.0	/* number of secs to wait */

#define MILLI		1000
#define MICRO		1000000

static char *progname;
static int  start_0;
static int  start_1;
static int  start_2;

static pan_mutex_p global_lock;
static pan_cond_p global_cv;
static int  turn;

static int  verbose = 0;

const unsigned int BIT_LIFT_LOCK = 0x1 << 0;
const unsigned int BIT_DO_BCAST  = 0x1 << 1;
const unsigned int BIT_YIELD     = 0x1 << 2;
const unsigned int BIT_QUICK     = 0x1 << 3;


typedef enum THR_SYNC_TYPE {
    THR_SIGNAL,
    THR_BCAST,
    THR_YIELD
} thr_sync_type_t, *thr_sync_type_p;


typedef struct THREAD_ARG_T thread_arg_t, *thread_arg_p;

struct THREAD_ARG_T {
    int          verbose;
    int          key;
    int          n_switches;
    int          ticks;
    thread_arg_p partner;
    unsigned int flags;
    pan_time_p   t;
};


int global_go = 0;

void
barrier(void)
{
    pan_mutex_lock( global_lock);
    while (!global_go) {
	pan_cond_wait( global_cv);
    }
    pan_mutex_unlock( global_lock);
}

static void
quick_loop_signal(thread_arg_p arg)
{
    int          i;
    pan_time_p   start = pan_time_create();
    pan_time_p   stop = pan_time_create();
    int          key        = arg->key;
    int          n_switches = arg->n_switches;
#ifdef VERY_VERBOSE
    int          my_verbose = arg->verbose;
#endif		/* VERY_VERBOSE */

    pan_time_get(start);

    pan_mutex_lock(global_lock);
    if (key == 0) {
	pan_cond_signal(global_cv);
    }

    for (i = 0; i < n_switches; i++) {
	pan_cond_wait(global_cv);

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: my turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: their turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	pan_cond_signal(global_cv);
    }

    pan_mutex_unlock(global_lock);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_time_copy(arg->t, stop);
    arg->n_switches = n_switches;

    pan_time_clear(start);
    pan_time_clear(stop);
}




static void
quick_loop_broadcast(thread_arg_p arg)
{
    int          i;
    pan_time_p   start = pan_time_create();
    pan_time_p   stop = pan_time_create();
    int          key        = arg->key;
    int          n_switches = arg->n_switches;
#ifdef VERY_VERBOSE
    int          my_verbose = arg->verbose;
#endif		/* VERY_VERBOSE */


    pan_time_get(start);

    pan_mutex_lock(global_lock);
    if (key == 0) {
	pan_cond_broadcast(global_cv);
    }

    for (i = 0; i < n_switches; i++) {
	pan_cond_wait(global_cv);

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: my turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: their turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	pan_cond_broadcast(global_cv);
    }

    pan_mutex_unlock(global_lock);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_time_copy(arg->t, stop);
    arg->n_switches = n_switches;

    pan_time_clear(start);
    pan_time_clear(stop);
}


static void
test0(void *arg_p)
{
    thread_arg_p arg = (thread_arg_p)arg_p;

    start_0 = 1;
    if (verbose >= arg->verbose) {
	printf("%p: run and return\n", pan_thread_self());
    }
    pan_thread_exit();
}


static void
test1(void *arg_p)
{
    thread_arg_p arg = (thread_arg_p)arg_p;

    start_1 = 1;
    if (verbose >= arg->verbose) {
	printf("%p: run and call pan_thread_exit\n", pan_thread_self());
    }
    pan_thread_exit();
}


static void
test2(void *arg_p)
{
    thread_arg_p arg = (thread_arg_p)arg_p;

    start_2 = 1;

    if (pan_thread_getprio() != pan_thread_maxprio()) {
	printf("%p: prio differs from the one that was set: %d %d\n",
	       pan_thread_self(), pan_thread_getprio(), pan_thread_maxprio());
    }
    if (verbose >= arg->verbose) {
	printf("%p: run and call pan_thread_exit\n", pan_thread_self());
    }
    pan_thread_exit();
}


static void
loop_yield(thread_arg_p arg)
{
    int          i;
    pan_time_p   start = pan_time_create();
    pan_time_p   stop = pan_time_create();
    int          switches;
    int          their_ticks;
    int          new_their_ticks;
    int          n_switches = arg->n_switches;
#ifdef VERY_VERBOSE
    int          my_verbose = arg->verbose;
#endif		/* VERY_VERBOSE */

    switches = 0;

    pan_time_get(start);

    their_ticks   = arg->partner->ticks;

    for (i = 0; i < n_switches; i++) {
	pan_thread_yield();

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: my turn\n", pan_thread_self());
	}
#endif		/* VERY_VERBOSE */

	arg->ticks++;
	new_their_ticks = arg->partner->ticks;
	if (their_ticks != new_their_ticks) {
	    ++switches;
	    their_ticks = new_their_ticks;
	}
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_time_copy(arg->t, stop);
    arg->n_switches = switches;

    pan_time_clear(start);
    pan_time_clear(stop);
}




/* In real ctxt switches, operation is
 *     Thread A		Thread B
 * 	lock;
 * 	wait;
 *			lock;
 *			signal;
 *			unlock;
 *	unlock;
 * In our loop, we therefore we do an extra, seemingly useless, unlock/lock
 * operation.
 * Watch out: the unlock should _not_ be done _after_ the turn increment/
 * signal: this can cause the other thread to run, to set the turn back
 * to us, and sleep. Then we run and find the turn already set to us,
 * so we can skip the cond_wait. This would yield a too favourable
 * image of thread switching!
 */

static void
loop_signal(thread_arg_p arg)
{
    int          i;
    pan_time_p     start = pan_time_create();
    pan_time_p     stop = pan_time_create();
    int          key        = arg->key;
    int          n_switches = arg->n_switches;
#ifdef VERY_VERBOSE
    int          my_verbose = arg->verbose;
#endif		/* VERY_VERBOSE */

    pan_time_get(start);

    pan_mutex_lock(global_lock);

    for (i = 0; i < n_switches; i++) {
	while (turn != key) {
	    pan_cond_wait(global_cv);
	}

	pan_mutex_unlock(global_lock);
	pan_mutex_lock(global_lock);

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: my turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	++turn;
	key += 2;

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: their turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	pan_cond_signal(global_cv);
    }

    pan_mutex_unlock(global_lock);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_time_copy(arg->t, stop);
    arg->n_switches = n_switches;

    pan_time_clear(start);
    pan_time_clear(stop);
}



static void
loop_broadcast(thread_arg_p arg)
{
    int          i;
    pan_time_p   start = pan_time_create();
    pan_time_p   stop = pan_time_create();
    int          key        = arg->key;
    int          n_switches = arg->n_switches;
#ifdef VERY_VERBOSE
    int          my_verbose = arg->verbose;
#endif		/* VERY_VERBOSE */


    pan_time_get(start);

    pan_mutex_lock(global_lock);

    for (i = 0; i < n_switches; i++) {
	while (turn != key) {
	    pan_cond_wait(global_cv);
	}

	pan_mutex_unlock(global_lock);
	pan_mutex_lock(global_lock);

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: my turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	++turn;
	key += 2;

#ifdef VERY_VERBOSE
	if (verbose >= my_verbose) {
	    printf("%p: their turn (turn = %d)\n", pan_thread_self(), turn);
	}
#endif		/* VERY_VERBOSE */

	pan_cond_broadcast(global_cv);
    }

    pan_mutex_unlock(global_lock);

    pan_time_get(stop);
    pan_time_sub(stop, start);

    pan_time_copy(arg->t, stop);
    arg->n_switches = n_switches;

    pan_time_clear(start);
    pan_time_clear(stop);
}





static void
test3(void *arg_p)
{
    thread_arg_p arg = (thread_arg_p)arg_p;


    if (verbose >= 10) {
	printf("%p: running\n", pan_thread_self());
    }
    if (verbose >= 25) {
	printf("%p: args: verbose = %d; key = %d; partner = %p; flags = %x\n",
		pan_thread_self(),
		arg->verbose, arg->key, arg->partner, arg->flags);
    }

    barrier();

    if (arg->flags & BIT_YIELD) {
	loop_yield(arg);
    } else {
	if (arg->flags & BIT_QUICK) {
	    if (arg->flags & BIT_DO_BCAST) {
		quick_loop_broadcast(arg);
	    } else {
		quick_loop_signal(arg);
	    }
	} else {
	    if (arg->flags & BIT_DO_BCAST) {
		loop_broadcast(arg);
	    } else {
		loop_signal(arg);
	    }
	}
    }

    if (verbose >= 10) {
	printf("%p: call pan_thread_exit\n", pan_thread_self());
    }

    pan_thread_exit();
}


static void
do_gettime(int n_gettimes, pan_time_p stop)
{
    pan_time_p start = pan_time_create();
    int      i;
    pan_time_p t = pan_time_create();

    if (n_gettimes == 0) {
	pan_time_copy(stop, pan_time_zero);
	return;
    }

    pan_time_get(start);
    for (i = 0; i < n_gettimes; i++) {
	pan_time_get(t);
    }
    pan_time_get(stop);
    pan_time_sub(stop, start);
    pan_time_div(stop, n_gettimes);
    pan_time_mul(stop, MICRO);

    pan_time_clear(start);
    pan_time_clear(t);
}


static void
do_create(int n_creates, pan_time_p stop)
{
    pan_time_p   start = pan_time_create();
    int          i;
    thread_arg_t arg;
    pan_thread_p thread;

    if (n_creates == 0) {
	pan_time_copy(stop, pan_time_zero);
	return;
    }

    pan_time_get(start);

    arg.verbose = 30;
    for (i = 0; i < n_creates; i++) {

	thread = pan_thread_create(test0, &arg, 0, 0, FALSE);

	if (verbose >= 15) {
	    printf("%p: thread %d created\n", pan_thread_self(), i);
	}
	pan_thread_join(thread);

	if (verbose >= 15) {
	    printf("%p: thread %d joined\n", pan_thread_self(), i);
	}
	pan_thread_clear(thread);
    }

    pan_time_get(stop);
    pan_time_sub(stop, start);
    pan_time_div(stop, n_creates);
    pan_time_mul(stop, MICRO);

    pan_time_clear(start);
}


static void
do_ctxt_switch(int             prio_0,
	       int             prio_1,
	       thr_sync_type_t sync_tp_0,
	       thr_sync_type_t sync_tp_1,
	       int             do_quick,
	       int            *n_switches,
	       pan_time_p      res)
{
    pan_thread_p thread0;
    pan_thread_p thread1;
    thread_arg_t arg0;
    thread_arg_t arg1;

    turn = 0;
    global_go = 0;

    arg0.n_switches = *n_switches;
    arg0.verbose    = 20;
    arg0.key        = 1;
    arg0.ticks      = 0;
    arg0.partner    = &arg1;
    arg0.flags      = 0;
    arg0.t          = pan_time_create();
    switch (sync_tp_0) {
    case THR_SIGNAL:	break;
    case THR_BCAST:	arg0.flags |= BIT_DO_BCAST;
			break;
    case THR_YIELD:	arg0.flags |= BIT_YIELD;
			break;
    }
    if (do_quick) {
	arg0.flags |= BIT_QUICK;
    }
    thread0 = pan_thread_create(test3, &arg0, 0, prio_0, FALSE);

    arg1.n_switches = *n_switches;
    arg1.verbose    = 20;
    arg1.key        = 0;
    arg1.ticks      = 0;
    arg1.partner    = &arg0;
    arg1.flags      = 0;
    arg1.t          = pan_time_create();
    switch (sync_tp_1) {
    case THR_SIGNAL:	break;
    case THR_BCAST:	arg1.flags |= BIT_DO_BCAST;
			break;
    case THR_YIELD:	arg1.flags |= BIT_YIELD;
			break;
    }
    if (do_quick) {
	arg1.flags |= BIT_QUICK;
    }
    thread1 = pan_thread_create(test3, &arg1, 0, prio_1, FALSE);

    pan_mutex_lock( global_lock);
    global_go = 1;
    pan_cond_broadcast( global_cv);
    pan_mutex_unlock( global_lock);

    pan_thread_join(thread0);
    pan_thread_join(thread1);

    *n_switches = arg0.n_switches + arg1.n_switches;

    if (*n_switches != 0) {
	pan_time_div(arg0.t, *n_switches);
    }
    pan_time_mul(arg0.t, MICRO);

    pan_time_copy(res, arg0.t);

    pan_thread_clear(thread0);
    pan_thread_clear(thread1);

    pan_time_clear(arg0.t);
    pan_time_clear(arg1.t);
}


static void
usage(void)
{
    printf("Usage: %s [flags] <n_switches>\n", progname);
    printf("flags:\n");
    printf("\t -c <n_create>\t # thread creations (default <n_switches>)\n");
    printf("\t -q \t\t quick sync (default with shared var)\n");
    printf("\t -t <n_time>\t # gettime calls (default <n_switches>)\n");
    printf("\t -v <verbosity>\t verbose value (default 0)\n");
    printf("\t -w <t_wait>\t cond wait interval (default %f)\n", TIMEOUT);
    pan_end();
    exit(5);
}



int
main(
     int argc,
     char **argv
)
{
    int          i;
    pan_time_p   t = pan_time_create();
    pan_time_p   start = pan_time_create();
    pan_time_p   stop = pan_time_create();
    int          n_switches   = -1;
    int          n_creates    = -1;
    int          n_gettimes   = -1;
    pan_thread_p thread1;
    pan_thread_p thread2;
    pan_thread_p thread3;
    thread_arg_t arg1;
    thread_arg_t arg2;
    thread_arg_t arg3;
    int          max_p;
    int          min_p;
    int          sw;
    int          do_quick = FALSE;
    double       t_wait = -1.0;

    /* Call initialization routines. */

    pan_init(&argc, argv);

    progname = argv[0];

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    if (strcmp(argv[i], "-c") == 0) {
		++i;
		if (sscanf(argv[i], "%d", &n_creates) != 1 || n_creates < 0) {
		    printf("illegal n_create value: %s\n", argv[i]);
		    usage();
		}
	    } else if (strcmp(argv[i], "-q") == 0) {
		do_quick = TRUE;
	    } else if (strcmp(argv[i], "-t") == 0) {
		++i;
		if (sscanf(argv[i], "%d", &n_gettimes) != 1 || n_gettimes < 0) {
		    printf("illegal n_time value: %s\n", argv[i]);
		    usage();
		}
	    } else if (strcmp(argv[i], "-v") == 0) {
		++i;
		if (sscanf(argv[i], "%d", &verbose) != 1) {
		    printf("illegal verbosity value: %s\n", argv[i]);
		    usage();
		}
	    } else if (strcmp(argv[i], "-w") == 0) {
		++i;
		if (sscanf(argv[i], "%lf", &t_wait) != 1) {
		    printf("illegal t_wait value: %s\n", argv[i]);
		    usage();
		}
	    } else {
		printf("unknown flag: %s\n", argv[i]);
		usage();
	    }
	} else {
	    if (n_switches != -1) {
		printf("unknown option: %s\n", argv[i]);
		usage();
	    }
	    if (sscanf(argv[i], "%d", &n_switches) != 1 || n_switches < 0) {
		printf("illegal n_switch value: %s\n", argv[i]);
		usage();
	    }
	}
    }

    arg1.t = pan_time_create();
    arg2.t = pan_time_create();
    arg3.t = pan_time_create();

    if (verbose >= 1) {
	printf("past pan_init\n");
    }
    global_lock = pan_mutex_create();
    global_cv = pan_cond_create(global_lock);

    if (n_creates == -1) {
	n_creates = n_switches;
    }
    if (n_gettimes == -1) {
	n_gettimes = n_switches;
    }
    n_switches = n_switches / 2;

    pan_start();

				/* the time it takes to read the clock */
    if (n_gettimes > 0) {
	do_gettime(n_gettimes, t);
	printf("pan_gettime       times  time (usec)\n");
	printf("                  %5d %12.3f\n",
		n_gettimes, pan_time_t2d(t));
    }


    max_p = pan_thread_maxprio();
    min_p = pan_thread_minprio();

    if (min_p < max_p) {
	pan_thread_setprio(min_p + 1);
    }

				/* Test thread create without join. */

    start_0 = start_1 = start_2 = 0;

    arg1.verbose = 10;
    thread1 = pan_thread_create(test0, &arg1, 0, min_p, TRUE);

				/* Test thread create with join. */

    arg2.verbose = 10;
    thread2 = pan_thread_create(test1, &arg2, 0, min_p, FALSE);

				/* Check scheduling;
				 * test0 and test1 should not have run yet. */

    if (start_0)
	printf("error: thread1 has run\n");
    if (start_1)
	printf("error: thread2 has run\n");
    if (start_0 || start_1) {
	printf("error: threads with lower priority scheduled 1\n");
    }

				/* Check thread_join. */

    if (verbose >= 10) {
	printf("before join of thread 2\n");
    }
    pan_thread_join(thread2);
    pan_thread_clear(thread2);
    if (verbose >= 10) {
	printf("thread 2 joined\n");
    }
    if (!start_1 || !start_0) {
	printf("error: main should have been blocked\n");
    }


			/* Check thread priorities. */

    				/* Create thread with high priority. */

    arg3.verbose = 10;
    thread3 = pan_thread_create(test2, &arg3, 0, max_p, TRUE);

    /* Test2 should have run by now. */

    if (!start_2) {
	printf("error: threads with lower priority scheduled 2\n");
    }
    if (verbose >= 4) {
	printf("so far so good; the following tests will take some"
	       " time to complete\n");
    }

    /* Check if creation of many threads works and measure cost. */
    if (n_creates > 0) {
	printf("thread create     times  time (usec)\n");
	do_create(n_creates, t);
	printf("                  %5d %12.3f\n",
		n_creates, pan_time_t2d(t));
    }

    if (n_switches > 0) {

	printf("context switch times\n");

	printf("    prio    signal/signal  times  time (usec)\n");
	printf("    ----    ------/------  -----  -----------\n");

	/* Do test with high priority threads and yield calls */

	sw = n_switches;
	do_ctxt_switch(max_p, max_p, THR_YIELD, THR_YIELD, do_quick, &sw, t);
	printf("    equal   yield /yield  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	/* Do test with mixed priority threads and yield calls */

	sw = n_switches;
	do_ctxt_switch(max_p, min_p, THR_YIELD, THR_YIELD, do_quick, &sw, t);
	printf("    unequal yield /yield  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	/* Do test with high priority threads and condition variables. */

	sw = n_switches;
	do_ctxt_switch(max_p, max_p, THR_BCAST, THR_BCAST, do_quick, &sw, t);
	printf("    equal   bcast /bcast  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	sw = n_switches;
	do_ctxt_switch(max_p, max_p, THR_SIGNAL, THR_BCAST, do_quick, &sw, t);
	printf("    equal   signal/bcast  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	sw = n_switches;
	do_ctxt_switch(max_p, max_p, THR_SIGNAL, THR_SIGNAL, do_quick, &sw, t);
	printf("    equal   signal/signal %6d %12.3f\n",
		sw, pan_time_t2d(t));


	/* Do test with a mix of high and low priority threads and condition */
	/* variables. */

	sw = n_switches;
	do_ctxt_switch(max_p, min_p, THR_BCAST, THR_BCAST, do_quick, &sw, t);
	printf("    unequal bcast /bcast  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	sw = n_switches;
	do_ctxt_switch(max_p, min_p, THR_SIGNAL, THR_BCAST, do_quick, &sw, t);
	printf("    unequal signal/bcast  %6d %12.3f\n",
		sw, pan_time_t2d(t));

	sw = n_switches;
	do_ctxt_switch(max_p, min_p, THR_SIGNAL, THR_SIGNAL, do_quick, &sw, t);
	printf("    unequal signal/signal %6d %12.3f\n",
		sw, pan_time_t2d(t));

    }


    /* Now check pan_cond_wait_time. */

    if (t_wait == -1.0) {
	t_wait = TIMEOUT;
    }

    pan_time_d2t(t, t_wait);

    printf("start of cond_timedwait during %f sec\n", pan_time_t2d(t));

    pan_mutex_lock(global_lock);

    pan_time_get(start);
    pan_time_add(t, start);

    pan_cond_timedwait(global_cv, t);

    pan_time_get(stop);

    pan_mutex_unlock(global_lock);

    pan_time_sub(stop, start);
    printf("    cond_timedwait of %f sec takes %f sec\n",
	    t_wait, pan_time_t2d(stop));

    pan_mutex_clear(global_lock);
    pan_cond_clear(global_cv);

    pan_end();

    pan_time_clear(t);
    pan_time_clear(start);
    pan_time_clear(stop);

    pan_time_clear(arg1.t);
    pan_time_clear(arg2.t);
    pan_time_clear(arg3.t);

    return 0;
}
