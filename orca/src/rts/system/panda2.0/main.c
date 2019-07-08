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
 * NOMACROS:	???
 * LATESTGREATEST:
 *		affects start-up comment.
 * TR_HACK:	???
 * USE_BG:	use the bg group implementation instead of the usual one.
 * DATA_PARALLEL:
 *		enables the additions (mostly initializing Saniya's RTS) for
 *		Data parallel Orca.
 * CERIELPORT	Only when DATA_PARALLEL is defined, combines Saniya's original
 *		RTS with Panda2. Only exists to get things working fast. Will
 *		be phased out later on.
 */

#include <interface.h>
#include "pan_sys.h"
#ifdef USE_BG
#include "pan_bg.h"
#else
#include "pan_group.h"
#endif
#include "pan_util.h"
#include "fork_exit.h"
#include "rts_comm.h"
#include "rts_init.h"
#include "rts_measure.h"

#define MAX(a, b)   ( (a) > (b) ? (a) : (b) )


/* Global options with default settings */
int replicate_all      = 0;     /* Object management by hand? */
int replicate_none     = 0;
int use_compiler_info  = 1;     /* What info is used for object managment */
int use_runtime_info   = 1;
int statistics         = 0;
static int trace_level = 0;
int ignore_strategy    = 0;
int log_accounting     = 0;
int verbose	       = 0;
int use_threads	       = 0;


int rts_my_pid;
int rts_nr_platforms;
int rts_base_pid = 0;		/* default situation; NO dedicated sequencer */

int rts_poll_count = 0;         /* poll when rts_poll_count == rts_max_poll */
int rts_max_poll = 1000;         /* default; can be overridden */

#ifdef AMOEBA
int warm_grp = 1;		/* should communications be warmed up? */
int warm_rpc = 0;
#else
int warm_grp = 0;
int warm_rpc = 0;
#endif

static int	ac;
static char	**av;

static void sync_platforms(void);

static void
usage(char *prog_name)
{
    (void)fprintf(stderr, "Usage: %s <my_pid> <nr_platforms> [options]\n"
		  "-v             verbose (print decisions)\n"
		  "-dedicated     use a dedicated sequencer\n"
		  "-statistics    print (group) statistics\n"
		  "-poll <num>    set polling interval\n"
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
#endif
		  "-warm_start <rpc> <grp>\n"
		  "               initialize comms (both RPC and GRP)\n",
		  prog_name);
    exit(1);
}

#ifdef CERIELPORT
#ifndef DATA_PARALLEL
#error DATA_PARALLEL should be defined when CERIELPORT is defined
#endif
#endif

void
main(int argc, char **argv)
{
    extern prc_dscr OrcaMain;
    extern void     ini_OrcaMain(void);
    int		    i;
#ifdef CERIELPORT
    int		    me, sz, pd;
#endif

    ac = 1;
    av = argv;

#ifdef CERIELPORT
    init_rts(&argc, argv);
    get_group_configuration(&me, &sz, &pd);
#else
    pan_init(&argc, argv);
#endif
    rts_my_pid = pan_my_pid();
    rts_nr_platforms = pan_nr_platforms();

    /* Not necessary anymore (according to Rutger). Also results in
       error message on platforms where the parameters don't make
       any sence (where the communication is reliable).

#ifndef USE_BG
    pan_group_va_set_params(0,
			    "PAN_GRP_ord_buf_size",
			    MAX(2 * rts_nr_platforms, 32),
			    "PAN_GRP_hist_size",
			    MAX(8 * rts_nr_platforms, 32),
			    0);
#endif
    */

    for (i = 1; i < argc; i++) {
    	if (strcmp(argv[i],"-p") == 0) {
	    if ( ++i >= argc) {
		usage(argv[0]);
	    }
	    switch ( atoi( argv[i])) {
	      case 1: replicate_none = 1;
		break;
	      case 2: replicate_all = 1;
		break;
	      case 3: /* default */
		break;
	      default:
		usage( argv[0]);
		break;
	    }
	} else if (strcmp(argv[i], "-poll") == 0) {
	    if (++i >= argc) {
		usage(argv[0]);
	    }
	    rts_max_poll = atoi(argv[i]);
	} else if (strcmp(argv[i],"-g") == 0) { 
	    /* PB/BB switch */
#ifndef USE_BG
	    pan_group_va_set_params( NULL, "bb_large", atoi(argv[++i]), NULL);
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
                usage(argv[0]);
            }
            trace_level = atoi(argv[++i]);
            if (trace_level < 0) {
                fprintf(stderr, "%d) trace level should be >= 0\n",
                        rts_my_pid);
                exit(1);
            }
#ifndef TRACING
            fprintf(stderr, "%d) (warning) tracing not enabled: "
                    "option -tlevel ignored\n", rts_my_pid);
#endif
	} else if (strcmp(argv[i], "-no_strategy") == 0) {
		ignore_strategy = 1;
	} else if (strcmp(argv[i], "-account") == 0) {
		log_accounting = 1;
	} else if (strcmp(argv[i], "-v") == 0) {
		verbose = 1;
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
		usage(argv[0]); 
#endif
	}
    }

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ); /* setlinebuf( stdout); */

    pan_mp_init();
#ifdef USE_BG
    pan_bg_init();
#else
    pan_group_init();
#endif
    pan_rpc_init();
    pan_util_init();

    rts_init(trace_level);

#if defined(DATA_PARALLEL) && !defined(CERIELPORT)
    /* Delay initialization of Saniya's RTS till here, because it calls
     * pan_start.
     */
    init_rts(&argc, argv);

#else
    pan_start();  /* Here?? */
#endif
#ifdef DATA_PARALLEL
    marshall_func = marshall_f;
    unmarshall_func = unmarshall_f;
    free_in_params_func = free_in_params_f;
    p_gatherinit = p_gatherinit_f;
#endif
#ifdef CERIELPORT
    init_rts_object(me, sz, pd, sizeof(int));
#endif
    ini_OrcaMain();

    sync_platforms();

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
	DoFork(0, &OrcaMain, (void **) 0);
    }    

    /* dedicated sequencer shuts down immediately */
    if (rts_my_pid >= rts_base_pid) {
    	fe_await_termination();
    }
    rts_measure_dump();
    rts_end();

#ifdef DATA_PARALLEL
    finish_rts();
#endif
    pan_util_end();
    pan_rpc_end();
#ifdef USE_BG
    pan_bg_end();
#else
    pan_group_end();
#endif
    pan_mp_end();
    pan_end();

    exit(0);
}


/* Some code to avoid cold start effects in communication. Send messages on
 * potential communication paths of an Orca application.
 */

#define BIG	5555		/* message size to initialize communications */

static int mcast_handle;
static int rpc_handle;

static pan_mutex_p comm_lock;
static pan_cond_p comm_cond;
static int grp_msg = 0, rpc_msg = 0;


static void
comm_handler(int handle, pan_upcall_p upcall, pan_msg_p request)
{
    int mc_done = 0;

    pan_mutex_lock(comm_lock);
    if (handle == rpc_handle) {
	pan_msg_p reply = pan_msg_create();

	rpc_msg++;
	pan_msg_push(reply, BIG, 1);

	pan_mutex_unlock(comm_lock);
	rc_untagged_reply(upcall, reply);
	pan_mutex_lock(comm_lock);
	pan_msg_clear(request);
    } else {
	grp_msg++;
	mc_done = 1;
    }
    pan_cond_broadcast(comm_cond);
    pan_mutex_unlock(comm_lock);
    if (mc_done) {
	rc_mcast_done();
    }
}


static void
sync_platforms(void)
{
    int cpu;
    pan_msg_p request, reply;
    pan_time_p start = pan_time_create();
    pan_time_p stop  = pan_time_create();
    
    if ( !warm_grp && !warm_rpc ) {
	rc_sync_platforms();
	return;
    }
    
    /*
     * init synch mechanism before exporting handler
     */
    comm_lock = pan_mutex_create();
    comm_cond = pan_cond_create(comm_lock);
    
    mcast_handle = rc_export(comm_handler);
    rpc_handle   = rc_export(comm_handler);
    
    /* Make sure everybody has exported "com_server" before
     * communicating. Calling rc_sync_platforms() is not enough
     * since only the "sequencer" really waits for all members to join.
     */
    rc_sync_platforms();
    if (rts_my_pid == 0) {
	if (verbose) printf( "warming up ...\n");
	pan_time_get(start);
    } else {  
	/* 
	 * I'm not the sequencer, so wait for his message to arrive
	 */
	pan_mutex_lock(comm_lock);
	while ( grp_msg == 0 && rpc_msg == 0) {
	    pan_cond_wait(comm_cond);
	}
	pan_mutex_unlock(comm_lock);
    }
    
    request = pan_msg_create();
    (void)pan_msg_push(request, BIG, 1);
    
    if (warm_rpc) {
	/* Avoid flooding the system by waiting for my turn */
	pan_mutex_lock(comm_lock);
	while (rpc_msg != rts_my_pid) {
	    pan_cond_wait(comm_cond);
	}
	pan_mutex_unlock(comm_lock);
	    
	/* use all RPC connections in round robin fashion */
	cpu = rts_my_pid + 1;
	for (;;) {
	    if (cpu == rts_nr_platforms) {
		cpu = 0;
	    }
	    if (cpu == rts_my_pid) {
		break;
	    }
	    rc_rpc(cpu, rpc_handle, request, &reply);
	    pan_msg_clear(reply);
	    cpu++;
	}
    }
    
    if (warm_grp) {
	/* issue one multicast to all members */
	rc_mcast(mcast_handle, request);
    } else {
	pan_msg_clear(request);
    }
    
    /* Wait for everybody to finish warming up */
    pan_mutex_lock(comm_lock);
    if (warm_grp) {
	while ( grp_msg < rts_nr_platforms) {
	    pan_cond_wait(comm_cond);
	}
    }
    if (warm_rpc) {
	while ( rpc_msg < rts_nr_platforms-1) {
	    pan_cond_wait(comm_cond);
	}
    }
    pan_mutex_unlock(comm_lock);
	    
    if (rts_my_pid == 0) {
	pan_time_get(stop);
	pan_time_sub(stop, start);
	if (verbose) {
	    printf("communications ready after %ld second(s)\n",
	       (long int)pan_time_t2d(stop));
	}
    } 
    
    pan_cond_clear(comm_cond);
    pan_mutex_clear(comm_lock);
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


