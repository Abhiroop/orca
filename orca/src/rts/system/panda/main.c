#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <orca_types.h>
#include <interface.h>
#include <rts_internals.h>
#include "panda/panda.h"
#include "fork_exit.h"
#include "rts_init.h"
#include "rts_comm.h"


/* Global options with default settings */
int replicate_all      = 0;     /* Object management by hand? */
int replicate_none     = 0;
int use_compiler_info  = 1;     /* What info is used for object managment */
int use_runtime_info   = 0;
int statistics         = 0;
static int trace_level = 0;

int rts_base_pid = 1;		/* default situation; NO dedicated sequencer */

/* Reply pool visible to all modules to avoid inefficient
 * use of message buffers.
 */
mpool_t message_pool;

#ifdef AMOEBA
int warm_grp = 1;		/* should communications be warmed up? */
int warm_rpc = 0;
#else
int warm_grp = 0;
int warm_rpc = 0;
#endif
static int	ac;
static char	**av;


static void sync_platforms(int me, int nr_platforms);

static void
usage(char *prog_name)
{
    (void)fprintf(stderr, "Usage: %s <my_pid> <nr_platforms>"
		  "[-dedicated] [-statistics] [-p <num>] [-g <num>]"
		  "[-dynamic] [-combined]"
#ifdef TRACING
		  "[-tlevel <level>]"
#endif
		  "[-warm_start <rpc> <grp>]\n", prog_name);
    exit(1);
}


void
main(int argc, char **argv)
{
    extern prc_dscr OrcaMain;
    extern void     ini_OrcaMain(void);
    extern group_t  rts_group;
    int me, nr_platforms, i, sequencer;

#ifdef AMOEBA
    me           = atoi(argv[1]) + 1;	/* gax counts from 0, Panda from 1 */
#else
    me           = atoi(argv[1]);
#endif
    nr_platforms = atoi(argv[2]);

    ac = 1;
    av = argv;
    for (i = 3; i < argc; i++) {
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
	} else if (strcmp(argv[i],"-g") == 0) { 
	    /* PB/BB switch */
	    pan_group_va_set_params( NULL, "bb_large", atoi(argv[++i]), NULL);
	} else if (strcmp(argv[i],"-dedicated") == 0) {  
	    /* only sequencer at 1 */
	    rts_base_pid = 2;
	} else if (strcmp(argv[i],"-dynamic") == 0)  {
		use_runtime_info = 1;
		use_compiler_info = 0;
	} else if (strcmp(argv[i],"-combined") == 0)  {
		use_runtime_info = 1;
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
                        sys_my_pid);
                exit(1);
            }
#ifndef TRACING
            fprintf(stderr, "%d) (warning) tracing not enabled: "
                    "option -tlevel ignored\n", sys_my_pid);
#endif
	} else if (strcmp(argv[i], "-OC") == 0) {
		ac = argc - i;
		argv[i] = argv[0];
		av = &argv[i];
		argc = i;
	} else {
	    usage( argv[0]);
	}
    }

    setvbuf( stdout, NULL, _IOLBF, 0); /* setlinebuf( stdout); */

    rts_init(me, nr_platforms, trace_level);
    init_mpool_special(&message_pool, "RTS", nr_platforms*2);
    ini_OrcaMain();

    sync_platforms(me, nr_platforms);

    if (me == 1) {
	printf("Forking OrcaMain...\n");
	DoFork(0, &OrcaMain, (void **) 0);
    }    

    /* quick hack, to get a dedicated sequencer without a member */
    if (me < rts_base_pid) {
	pan_group_va_get_g_params(&rts_group, "sequencer", &sequencer, NULL);
	if (me != sequencer) {
	    printf( "WARNING: NO dedicated sequencer at CPU0, "
		   "but shared at CPU%d\n", sequencer);
	}
	/* don't wait, but shut down immediately */
    } else {
    	fe_await_termination();
    }
    clear_mpool(&message_pool);
    rts_end();

    if (statistics) {
	pan_group_print_stats(&rts_group);
    }
    exit(0);
}


/* Some code to avoid cold start effects in communication. Send messages on
 * potential communication paths of an Orca application.
 */

static void comm_handler(int op, pan_upcall_t upcall, message_p request);

static operation_p comm_ops[] = { comm_handler, comm_handler};
#define MCAST	0
#define RPC	1


#define BIG	5555		/* message size to initialize communications */

static mutex_t comm_lock;
static cond_t comm_cond;
static int grp_msg = 0, rpc_msg = 0;


static void
sync_platforms(int me, int nr_platforms)
{
    int comm_server, cpu;
    message_p request, reply;
    timest_t start, stop;
    
    if ( !warm_grp && !warm_rpc ) {
	rc_sync_platforms();
	return;
    }
    
    /*
     * init synch mechanism before exporting handler
     */
    sys_cond_init( &comm_cond);
    sys_mutex_init( &comm_lock);
    
    comm_server = rc_export( "comm_server", 2, comm_ops);
    
    /* Make sure everybody has exported "com_server" before
     * communicating. Calling rc_sync_platforms() is not enough
     * since only the "sequencer" really waits for all members to join.
     */
    rc_sync_platforms();
    if (me == 1) {
	printf( "warming up ...\n");
	sys_gettime(&start);
    } else {  
	/* 
	 * I'm not the sequencer, so wait for his message to arrive
	 */
	sys_mutex_lock( &comm_lock);
	while ( grp_msg == 0 && rpc_msg == 0) {
	    sys_cond_wait( &comm_cond, &comm_lock);
	}
	sys_mutex_unlock( &comm_lock);
    }
    
    request = grp_message_init();		/* not on the stack! */
    (void)sys_message_push( request, BIG, 1);
    
    if (warm_rpc) {
	/* Avoid flooding the system by waiting for my turn */
	sys_mutex_lock( &comm_lock);
	while ( rpc_msg != sys_my_pid-1) {
	    sys_cond_wait( &comm_cond, &comm_lock);
	}
	sys_mutex_unlock( &comm_lock);
	    
	/* use all RPC connections in round robin fashion */
	cpu = me+1;
	for (;;) {
	    if ( cpu > nr_platforms) {
		cpu = 1;
	    }
	    if ( cpu == me) {
		break;
	    }
	    rc_rpc(cpu, comm_server, RPC, request, &reply);
	    sys_message_clear( reply);
	    cpu++;
	}
    }
    
    if (warm_grp) {
	/* issue one multicast to all members */
	rc_mcast(comm_server, MCAST, request);
    }
    
    /* wait for everybody to finish warming up */
    sys_mutex_lock( &comm_lock);
    if (warm_grp) {
	    while ( grp_msg < nr_platforms) {
		sys_cond_wait( &comm_cond, &comm_lock);
	    }
    }
    if (warm_rpc) {
	    while ( rpc_msg < nr_platforms-1) {
		sys_cond_wait( &comm_cond, &comm_lock);
	    }
    }
    sys_mutex_unlock( &comm_lock);
	    
    if (me == 1) {
	sys_gettime(&stop);
	printf("communications ready after %ld second(s)\n",
	       stop.t_sec - start.t_sec);
    }
    
    sys_cond_clear(&comm_cond);
    sys_mutex_clear(&comm_lock);
}


static void
comm_handler(int op, pan_upcall_t upcall, message_p request)
{
    int mc_done = 0;

    sys_mutex_lock(&comm_lock);
    if ( op == RPC) {
	message_p reply = get_mpool(&message_pool);

	rpc_msg++;
	sys_message_push(reply, BIG, 1);

	sys_mutex_unlock(&comm_lock);
	rc_untagged_reply(upcall, reply);
	sys_mutex_lock(&comm_lock);
	sys_message_clear(request);
    } else {
	grp_msg++;
	mc_done = 1;
    }
    sys_cond_broadcast(&comm_cond);
    sys_mutex_unlock(&comm_lock);
    if (mc_done) {
	rc_mcast_done();
    }
}

t_integer f_args__Argc(void)
{
  return	ac;
}

void f_args__Argv(t_integer n, t_string *s)
{
  a_free(s, &td_string);
  if (n >= 0 && n < ac) {
	a_allocate(s, &td_string, 1, strlen(av[n]));
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
  a_free(s, &td_string);
  if (ev) {
	a_allocate(s, &td_string, 1, strlen(ev));
	strncpy(&((char *)(s->a_data))[s->a_offset], ev, s->a_sz);
  }
}
