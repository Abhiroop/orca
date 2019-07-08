#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_glocal.h"
#include "pan_malloc.h"
#include "pan_threads.h"
#include "pan_message.h"
#include "pan_sync.h"
#include "pan_pset.h"
#include "pan_time.h"
#include "pan_sys_pool.h"
#include "pan_error.h"

#include "pan_trace.h"
#include <time.h>

#ifdef MAMA
#include "mama.h"
#endif
#ifdef LAM
#include "gam_cmaml.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef TRACING
#include "pan_util.h"
#endif

#define N_CLOCK_SYNCS 10	/* # msg exchanges to determine clock sync */

int      pan_sys_nr;
int      pan_sys_pid;
int      pan_sys_started = 0;

int pan_comm_winterval = 1;               /* warning interval */
int pan_stats = 0;                        /* print statistics */
int pan_mcast_slack = 0;

static char *progname;

static void
usage(void)
{
#ifdef MAMA
    fprintf(stderr,
	    "Usage: gax [<gax args>] %s "
	    "<partition size> "     /* allocate this many processors */
	    "<#procs> "             /* use this many for Panda */
	    "[-pan_wi <number>]"    /* warning interval */
	    "[-pan_mem <Mbytes>]"   /* heap size */
	    "[-pan_slack <number>]" /* max. backlog at sequencer */
            "[-pan_stats]"          /* print statistics */
	    "[<MAMA args>] "        /* MAMA-specific arguments */
	    "[<appl args>]\n",      /* other arguments */
	    progname);
#else
#ifdef LAM
    fprintf(stderr,
	    "Usage: gax [<gax args>] %s "
	    "[-pan_wi <number>]"    /* warning interval */
	    "[-pan_mem <Mbytes>]"   /* heap size */
	    "[-pan_slack <number>]" /* max. backlog at sequencer */
            "[-pan_stats]"          /* print statistics */
	    "[<appl args>]\n",      /* other arguments */
	    progname);
#else
    fprintf(stderr, "Usage: %s "
	    "<nr platforms> "       /* #processors */
	    "[-pan_wi <number>]"    /* warning interval */
	    "[-pan_mem <Mbytes>]"   /* heap size */
	    "[-pan_slack <number>]" /* max. backlog at sequencer */
            "[-pan_stats]"          /* print statistics */
	    "[<appl args>]\n",      /* other arguments */
	    progname);
#endif /* LAM */
#endif /* MAMA */
    exit(1);
}

void
pan_init(int *argc, char *argv[])
{
    int i, my_argc, heapsize;
    char **my_argv;
    
#ifdef CMOST
    CMMD_fset_io_mode(stdin, CMMD_independent);
    CMMD_fset_io_mode(stdout, CMMD_independent);
    CMMD_fset_io_mode(stderr, CMMD_independent);
#endif

    heapsize = 0;
    progname = argv[0];

    my_argc = *argc;
    my_argv = pan_malloc((my_argc + 1) * sizeof(char *));
    for (i = 0; i <= my_argc; i++) {
	my_argv[i] = argv[i];
    }
    *argc = 1;

#ifdef MAMA
    if (my_argc < 4){
	usage();
    }
    pan_sys_nr = atoi(argv[3]);
    *argc += 2;
    i = 4;
#else
#ifdef LAM
    if (my_argc < 3){
	usage();
    }
    pan_sys_pid = atoi(argv[1]);
    pan_sys_nr = atoi(argv[2]);
    i = 3;
#else
    if (my_argc < 2){
	usage();
    }
    pan_sys_nr = atoi(argv[1]);
    i = 2;
#endif /* LAM */
#endif /* MAMA */

    /* 
     * Parse all Panda arguments.
     */
    for (; i < my_argc; i++) {
	if (strcmp(my_argv[i], "-pan_pool_statistics") == 0){
	    pan_sys_pool_statistics();
	} else if (strcmp(my_argv[i], "-pan_panic_core") == 0){
	    pan_sys_dump_core();
	} else if (strcmp(my_argv[i], "-pan_wi") == 0) {
	    if (++i >= my_argc) {
		usage();
	    }
	    pan_comm_winterval = atoi(my_argv[i]);
        } else if (strcmp(my_argv[i], "-pan_stats") == 0) {
            pan_stats = 1;
	} else if (strcmp(my_argv[i], "-pan_mem") == 0) {
	    if (++i >= my_argc) {
		usage();
	    }
	    heapsize = atoi(my_argv[i]);
	} else if (strcmp(my_argv[i], "-pan_slack") == 0) {
	    if (++i >= my_argc) {
		usage();
	    }
	    pan_mcast_slack = atoi(my_argv[i]);
	} else {
	    break;
	}
    }

    /*
     * Copy unrecognised tail arguments back into argv.
     */
    for (; i < my_argc; i++) {
	argv[(*argc)++] = my_argv[i];
    }
    pan_free(my_argv);
    argv[*argc] = (char *)0;

#ifdef MAMA
    mama_init(argc, argv, 0);         /* 0 => no preemptive scheduling */
    mama_register();                  /* register this thread */
#else
#ifdef LAM
    gam_cmaml_init(pan_sys_nr, pan_sys_pid);
#endif /* LAM */
#endif /* MAMA */

    pan_sys_pid = CMMD_self_address();

    /*
     * Disable interrupts when they are enabled.
     */
    if (CMAML_interrupt_status()) {
	if (!CMAML_disable_interrupts()) {
	    CMMD_error("pan_init: CMAML_disable_interrupts failed\n");
	}
    }

    if (pan_sys_pid >= pan_sys_nr) {
	CMMD_sync_with_nodes();    /* startup sync with this routine */
	CMMD_sync_with_nodes();    /* startup sync with pan_start() */
#ifdef MAMA
	mama_end();
#endif
	exit(0);
    }

    pan_malloc_start(heapsize);
    pan_sys_time_start();

#ifdef TRACING
    /*
     * Switch on tracing before any call to trc_event can have occurred:
     * i.e.  before group or rpc _init
     */
    {
	char panda_id[256];                 /* Derive name from argv list */
	int  max_trace_buf;                 /* Derive value from argv list */
	time_t     tp;
	struct tm *t;
	
	time(&tp);
	t = localtime(&tp);
	max_trace_buf = 1048576;		/* For now.... */
	sprintf(panda_id, "panda-%d.%d.%d-%d.%02d",
		t->tm_mday, t->tm_mon + 1, t->tm_year, t->tm_hour, t->tm_min);
	trc_start(panda_id, max_trace_buf);
	trc_set_level(0);			/* For now.... */
	trc_new_thread(0, "main");
        if (pan_sys_nr > MAXMACHINE) {
	CMMD_error("pan_sys_comm_start: too many (%d) machines requested\n",
		   pan_sys_nr);
    }

}
#endif

    pan_sys_pool_start();
    pan_sys_buffer_start();
    pan_sys_msg_start();
    pan_sys_fragment_start();
    pan_sys_pset_start();
    pan_sys_nsap_start();
    pan_sys_thread_start();
    pan_sys_glocal_start();
    pan_sys_comm_start();
#ifdef TRACING
    pan_util_init();
#endif

    CMMD_sync_with_nodes();
}


void
pan_end(void)
{
    pan_malloc_stats();
    pan_sys_comm_end();
    pan_sys_glocal_end();
    pan_sys_thread_end();
    pan_sys_nsap_end();
    pan_sys_pset_start();
    pan_sys_fragment_end();
    pan_sys_msg_end();
    pan_sys_buffer_end();
    pan_sys_pool_end();
    pan_sys_time_end();

#ifdef TRACING
    trc_end();
#endif

#ifdef MAMA
    mama_end();
#endif
#ifdef LAM
    gam_cmaml_end();
#endif
}

void
pan_start(void)
{
#ifdef TRACING
    struct pan_time t;
    struct pan_time dt;
#endif

    pan_sys_comm_wakeup();
    pan_sys_started = 1;
#ifdef TRACING
    pan_clock_sync(N_CLOCK_SYNCS, &t, &dt);
#endif
    CMMD_sync_with_nodes();
}

int
pan_nr_platforms(void)
{
    return pan_sys_nr;
}

int
pan_my_pid(void)
{
    return pan_sys_pid;
}

/* Added parameter set/get RFHH */
/* Thu Feb 16 12:14:24 MET 1995 */
void
pan_sys_va_set_params(void *dummy, ...)
{
    va_list args;
    char   *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd) {
	/* Templates:
	if (strcmp("PAN_SYS_n_buffers", cmd) == 0) {
	    n_buffers = va_arg(args, int);
	} else if (strcmp("PAN_SYS_timeout", cmd) == 0) {
	    pan_time_d2t(timeout, va_arg(args, double));
	} else {
	* end of templates */

	if (pan_my_pid() == 0) {
	    printf("%2d: No such system parameter: \"%s\" -- ignored\n",
		   pan_my_pid(), cmd);
	}

	va_arg(args, void*);           /* skip value */
	cmd = va_arg(args, char*);     /* next command */
    }
}

void
pan_sys_va_get_params(void *dummy, ...)
{
    va_list args;
    char   *cmd;
    char  **comm_stats;
    int     b_size;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {
	if (strcmp("PAN_SYS_statistics", cmd) == 0) {
	    /* do nothing */
	    comm_stats = va_arg(args, char **);
	    b_size = sys_sprint_comm_stat_size();
	    *comm_stats = pan_malloc(b_size);
	    sys_sprint_comm_stats(*comm_stats);
	} else {
	    printf("%2d: No such system parameter: \"%s\" -- ignored\n",
		    pan_my_pid(), cmd);
	    va_arg(args, void*);
	}

	cmd = va_arg(args, char*);
    }
}


