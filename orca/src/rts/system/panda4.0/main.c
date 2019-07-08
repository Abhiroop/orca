/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* Compilation defines:
 *
 * AMOEBA:	must be set when compiled for Amoeba; different defaults for
 *		communication warm-up.
 * TRACING:	enables tracing facilities.
 * PURE_WRITE:	enables pure writes handling (not finished).
 * ACCOUNTING:	turn on accounting facilities.
 * RTS_VERBOSE:	debugging. Makes RTS print decisions it makes.
 * RTS_MEASURE:	???
 * NOALIGN:	???
 * LATESTGREATEST:
 *		affects start-up comment.
 * USE_BG:	use the bg group implementation instead of the usual one.
 * DATA_PARALLEL:
 *		enables the additions (mostly initializing Saniya's RTS) for
 *		Data parallel Orca.
 */

#include <interface.h>
#include "pan_sys.h"
#include "pan_util.h"
#include "pan_comm_util.h"
#ifdef USE_BG
#include "pan_bg.h"
#else
#include "pan_group.h"
#endif
#include "fork_exit.h"
#include "rts_comm.h"
#include "rts_init.h"
#include "rts_measure.h"
#include "msg_marshall.h"

#define MAX(a, b)   ( (a) > (b) ? (a) : (b) )


/* Global options with default settings */
int replicate_all      = 0;     /* Object management by hand? */
int replicate_none     = 0;
int use_compiler_info  = 1;     /* What info is used for object managment */
int use_runtime_info   = 1;
int statistics         = 0;
static int trace_level = 0;
int trc_local_reads    = 1;
int ignore_strategy    = 0;
int log_accounting     = 0;
int verbose	       = 0;
int use_threads	       = 0;

pan_mutex_p rts_global_lock;
pan_mutex_p rts_write_lock;
pan_mutex_p rts_read_lock;

int rts_my_pid;
int rts_nr_platforms;
int rts_base_pid = 0;		/* default situation; NO dedicated sequencer */

int rts_max_poll    = 1000;     /* default; can be overridden */
int rts_poll_count  = 0;
int rts_max_slice   = 10000;    /* default; can be overridden */
int rts_slice_count = 0;
int rts_step_count  = 1000;     /* poll/switch when rts_step_count <= 0 */

#ifdef AMOEBA
int warm_grp = 1;		/* should communications be warmed up? */
int warm_rpc = 0;
#else
int warm_grp = 0;
int warm_rpc = 0;
#endif

static int	ac;
static char	**av;

static void sync_platforms(int *pargc, char **argv);

static void
usage(char *prog_name)
{
    (void)fprintf(stderr, "Usage: %s <my_pid> <nr_platforms> [options]\n"
		  "-v             verbose (print decisions)\n"
		  "-dedicated     use a dedicated sequencer\n"
		  "-statistics    print (group) statistics\n"
		  "-poll <num>    set polling interval\n"
		  "-slice <num>   set time-slicing interval\n"
		  "-p <num>       policy (compatible with bcast-rpc)\n"
		  "-g <num>       switch between PB and BB\n"
		  "-no_strategy   ignore Strategy directives in Orca source\n"
		  "-dynamic       base obj impl strategies on dynamic counts\n"
		  "-static        base obj impl strategies on compiler estimates\n"
		  "-combined      combine compiler estimates and -dynamic\n"
		  "-account       generate account files (if compiled properly)\n"
		  "-threads       RTS-threads version (RTS-cont by default)\n"
#ifdef TRACING
		  "-tlevel <level>\n"
		  "               discard tracing events below <level>\n"
		  "-trc_no_local_reads\n"
		  "               discard tracing local read operations\n"
#endif
		  "-warm_start <rpc> <grp>\n"
		  "               initialize comms (both RPC and GRP)\n",
		  prog_name);
    exit(1);
}


void     ini_OrcaMain(void);

void
main(int argc, char **argv)
{
    extern prc_dscr OrcaMain;
    int		    i;

    ac = 1;
    av = argv;

    pan_init(&argc, argv);
    rts_my_pid = pan_my_pid();
    rts_nr_platforms = pan_nr_processes();

    rts_global_lock = pan_mutex_create();
    rts_write_lock = pan_mutex_create();
    rts_read_lock = pan_mutex_create();

#ifdef USE_BG
    pan_bg_init(&argc, argv);
#else
    pan_group_init(&argc, argv);
#endif
    pan_rpc_init(&argc, argv);
    pan_util_init(&argc, argv);
    pan_comm_util_init(&argc, argv);

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i],"-p") == 0) {
	    if ( ++i >= argc) {
		if (rts_my_pid == 0) {
		    usage(argv[0]);
		}
	    }
	    switch ( atoi( argv[i])) {
	      case 1: replicate_none = 1;
		break;
	      case 2: replicate_all = 1;
		break;
	      case 3: /* default */
		break;
	      default:
		if (rts_my_pid == 0) {
		    usage( argv[0]);
		}
		break;
	    }
	} else if (strcmp(argv[i], "-poll") == 0) {
	    if (++i >= argc) {
		if (rts_my_pid == 0) {
		    usage(argv[0]);
		}
	    }
	    rts_max_poll = atoi(argv[i]);
	} else if (strcmp(argv[i], "-slice") == 0) {
	    if (++i >= argc) {
		if (rts_my_pid == 0) {
		    usage(argv[0]);
		}
	    }
	    rts_max_slice = atoi(argv[i]);
#ifdef NEVER
	} else if (strcmp(argv[i],"-g") == 0) { 
	    /* PB/BB switch */
#ifndef USE_BG
	    pan_group_va_set_params( NULL, "bb_large", atoi(argv[++i]), NULL);
#endif
#endif
	} else if (strcmp(argv[i],"-dedicated") == 0) {  
	    /* only sequencer at 0 */
	    rts_base_pid++;
	} else if (strcmp(argv[i],"-dynamic") == 0)  {
		use_runtime_info = 1;
		use_compiler_info = 0;
	} else if (strcmp(argv[i],"-combined") == 0)  {
		use_runtime_info = 1;
		use_compiler_info = 1;
	} else if (strcmp(argv[i],"-static") == 0)  {
		use_runtime_info = 0;
		use_compiler_info = 1;
	} else if (strcmp(argv[i],"-warm_start") == 0)  {
	    warm_rpc = atoi( argv[++i]);
	    warm_grp = atoi( argv[++i]);
	} else if (strcmp(argv[i],"-statistics") == 0)  {
	    statistics = !statistics;
	} else if (strcmp(argv[i], "-tlevel") == 0) {
	    if (i + 1 >= argc) {
		if (rts_my_pid == 0) {
		    usage(argv[0]);
		}
	    }
	    trace_level = atoi(argv[++i]);
	    if (trace_level < 0) {
		fprintf(stderr, "%d) trace level should be >= 0\n",
			rts_my_pid);
		exit(1);
	    }
#ifndef TRACING
	    if (rts_my_pid == 0) {
		fprintf(stderr, "%d) (warning) tracing not enabled: "
			"option -tlevel ignored\n", rts_my_pid);
	    }
#endif
	} else if (strcmp(argv[i], "-trc_no_local_reads") == 0) {
	    trc_local_reads = 0;
#ifndef TRACING
	    if (rts_my_pid == 0) {
		fprintf(stderr, "%d) (warning) tracing not enabled: "
			"option -trc_no_local_reads ignored\n", rts_my_pid);
	    }
#endif
	} else if (strcmp(argv[i], "-no_strategy") == 0) {
		ignore_strategy = 1;
	} else if (strcmp(argv[i], "-account") == 0) {
		log_accounting = 1;
	} else if (strcmp(argv[i], "-v") == 0) {
		verbose++;
	} else if (strcmp(argv[i], "-threads") == 0) {
		use_threads = 1;
	} else if (strcmp(argv[i], "-OC") == 0) {
		/* All arguments after -OC are for the Orca program. */
		ac = argc - i;
		argv[i] = argv[0];
		av = &argv[i];
		argc = i;
	} else {
#ifndef DATA_PARALLEL
	    if (rts_my_pid == 0) {
		usage(argv[0]);
	    }
#endif
	}
    }

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ); /* setlinebuf( stdout); */

    rts_init(trace_level);

#ifdef DATA_PARALLEL
    opmarshall_func = opmarshall_f;
    opunmarshall_func = opunmarshall_f;
    opfree_in_params_func = opfree_in_params_f;
    p_gatherinit = p_gatherinit_f;
#endif

#ifndef DATA_PARALLEL
    ini_OrcaMain();
#endif

    rts_lock();
    sync_platforms(&argc, argv);

    if (rts_my_pid == 0) {
#ifdef LATESTGREATEST
	printf("### Orca RTS; latest greatest ###\n");
	printf("- uses specialised marshalling hooks\n");
#ifdef PURE_WRITE
	printf("- uses PURE WRITES\n");
#endif
#ifdef TRACING
	printf("- tracing switched on\n");
#endif
#ifdef ACCOUNTING
	printf("- accounting switched on\n");
#endif
	printf("######################################\n");
#endif
	if (verbose) {
	    printf("Forking OrcaMain...\n");
	}
	rts_unlock();
	DoFork(0, &OrcaMain, (void **) 0);
	rts_lock();
    }    

    /* 
     * Wait until all Orca processes have died. Only the 
     * dedicated sequencer shuts down immediately
     */
    if (rts_my_pid >= rts_base_pid && rts_my_pid < rts_nr_platforms) {
	fe_await_termination();
    }

    /*
     * RTS finalisation.
     */
    if (statistics)
	mm_print_stats();
    rts_measure_dump();
    rts_end();
    rts_unlock();
    pan_mutex_clear(rts_global_lock);
    pan_mutex_clear(rts_write_lock);
    pan_mutex_clear(rts_read_lock);

#ifdef DATA_PARALLEL
    finish_rts();
#endif

    /*
     * Panda finalisation.
     */
    pan_comm_util_end();
    pan_util_end();
    pan_rpc_end();
#ifdef USE_BG
    pan_bg_end();
#else
    pan_group_end();
#endif
    pan_end();

    exit(0);
}


/* Some code to avoid cold start effects in communication. Send messages on
 * potential communication paths of an Orca application.
 */

#ifdef NOALIGN
#define BIG	5555		/* message size to initialize communications */
#else
#define BIG 6400		/* aligned on sizeof(double) */
#endif

static int mcast_handle;
static int rpc_handle;

static pan_cond_p comm_cond;
static int grp_msg = 0, rpc_msg = 0;

static void comm_rpc_clean(rc_msg_p msg)
{
    m_free(msg->iov->data);
    rts_lock();
    mm_free_iovec(msg->iov);
    rc_proto_clear(msg->proto);
    rts_unlock();
    rc_msg_clear(msg);
}


static int
comm_handler(int handle, int upcall, pan_msg_p request, void *proto)
{
    int mc_done = 0;

    if (handle == rpc_handle) {
	rc_msg_p reply;

	reply = rc_msg_create();
	reply->iov = mm_get_iovec(1);
	reply->iov->data = m_malloc(BIG);
	reply->iov->len = BIG;
	reply->iov_size = 1;
	reply->proto = rc_proto_create();
	reply->proto_size = pan_proto_max_size();
	reply->clearfunc = comm_rpc_clean;

	rpc_msg++;

	rc_untagged_reply(upcall, reply);
    } else {
	grp_msg++;
	mc_done = 1;
    }
    pan_cond_broadcast(comm_cond);
    if (mc_done) {
	rc_mcast_done();
    }
    return 1;
}


static void
sync_platforms(int *pargc, char **argv)
{
    int cpu;
    pan_iovec_t	iov;
    void       *proto;
    void *request;
    pan_msg_p	reply;
    void       *reply_proto;
    int size, len;
    pan_time_p start;
    pan_time_p stop;

    assert(!rts_trylock());

    if ( !warm_grp && !warm_rpc ) {
	/* release lock because pan_start() may block for incoming message */
	rts_unlock();
#ifdef DATA_PARALLEL
	init_rts(pargc, argv);
	ini_OrcaMain();	/* it gets called after pan_start. Not good, but calling
			   it before init_rts is not good either.
			*/
#else
	pan_start();
#endif
	rts_lock();
	return;
    }

    start = pan_time_create();
    stop  = pan_time_create();

    /*
     * init synch mechanism before exporting handler
     */
    comm_cond = rts_cond_create();

    mcast_handle = rc_export(comm_handler);
    rpc_handle   = rc_export(comm_handler);

    /* Make sure everybody has exported "com_server" before
     * communicating: call pan_start() afterwards.
     */
    rts_unlock();
#ifdef DATA_PARALLEL
    init_rts(pargc, argv);
    ini_OrcaMain();	/* it gets called after pan_start. Not good, but calling
			   it before init_rts is not good either.
			*/
    /* init_rts calls pan_start. */
#else
    pan_start();
#endif
    rts_lock();
    if (rts_my_pid == 0) {
	if (verbose) printf( "warming up ...\n");
	pan_time_get(start);
    } else {  
	/* 
	 * I'm not the sequencer, so wait for his message to arrive
	 */
	while ( grp_msg == 0 && rpc_msg == 0) {
	    pan_cond_wait(comm_cond);
	}
    }

    proto = rc_proto_create();
    request = m_malloc(BIG);
    iov.data = request;
    iov.len  = BIG;

    if (warm_rpc) {
	/*
	 * Avoid flooding the system by waiting for my turn.
	 */
	while (rpc_msg != rts_my_pid) {
	    pan_cond_wait(comm_cond);
	}

	/*
	 * Use all RPC connections in round robin fashion.
	 */
	cpu = rts_my_pid + 1;
	for (;;) {
	    if (cpu == rts_nr_platforms) {
		cpu = 0;
	    }
	    if (cpu == rts_my_pid) {
		break;
	    }
	    rc_rpc(cpu, rpc_handle, &iov, 1, proto, pan_proto_max_size(),
	    	   &reply, &reply_proto);
	    rts_unlock();
	    pan_msg_clear(reply);
	    rts_lock();
	    cpu++;
	}
    }

    if (warm_grp) {
	/*
	 * issue one multicast to all members
	 */
	rc_mcast(mcast_handle, &iov, 1, proto, pan_proto_max_size());
    } else {
	m_free(request);
	rc_proto_clear(proto);
    }

    /*
     * Wait for everybody to finish warming up
     */
    if (warm_grp) {
	while ( grp_msg < rts_nr_platforms) {
	    pan_cond_wait(comm_cond);
	}
	m_free(request);
	rc_proto_clear(proto);
    }
    if (warm_rpc) {
	while ( rpc_msg < rts_nr_platforms-1) {
	    pan_cond_wait(comm_cond);
	}
    }

    if (rts_my_pid == 0) {
	pan_time_get(stop);
	pan_time_sub(stop, start);
	if (verbose) {
	    printf("communications ready after %ld second(s)\n",
	       (long int)pan_time_t2d(stop));
	}
    } 

    rts_cond_clear(comm_cond);
    pan_time_clear(start);
    pan_time_clear(stop);
}

t_integer f_args__Argc(void)
{
  return	ac;
}

void f_args__Argv(t_integer n, t_string *s)
{
  free_string(s);
  if (n >= 0 && n < ac) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(av[n]));
	strncpy(&((char *)(s->a_data))[s->a_offset], av[n], s->a_sz);
  }
}

void f_args__Getenv(t_string *e, t_string *s)
{
  char	buf[1024];
  char	*p = buf;
  char	*ev;

  if (e->a_sz >= 1024) {
	p = m_malloc(e->a_sz + 1);
  }
  strncpy(p, &((char *)(e->a_data))[e->a_offset], e->a_sz);
  p[e->a_sz] = 0;
  ev = getenv(p);
  if (e->a_sz >= 1024) m_free(p);
  free_string(s);
  if (ev) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(ev));
	strncpy(&((char *)(s->a_data))[s->a_offset], ev, s->a_sz);
  }
}


