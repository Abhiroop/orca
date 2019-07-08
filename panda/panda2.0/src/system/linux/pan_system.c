#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_time.h"
#include "pan_comm.h"
#include "pan_glocal.h"
#include "pan_threads.h"
#include "pan_message.h"
#include "pan_sync.h"
#include "pan_pset.h"
#include "pan_sys_pool.h"

#include "pan_trace.h"

#include <pthread.h>
#include <signal.h>
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


static void
usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s <pid> <nr platforms> [<appl args>]*\n",
	    prog_name);
    exit(1);
}


void
pan_init(int *argc, char *argv[])
{
    int i, j;

    pthread_init();

    if (*argc < 3){
	usage(argv[0]);
    }

    /* Extract pid and nr_platforms, and shift all other arguments */
    if (sscanf(argv[1], "%d", &pan_sys_pid) != 1) {
	fprintf(stderr, "Platform id is not an int: \"%s\"\n", argv[1]);
	usage(argv[0]);
    }
    if (sscanf(argv[2], "%d", &pan_sys_nr) != 1) {
	fprintf(stderr, "Platform total is not an int: \"%s\"\n", argv[2]);
	usage(argv[0]);
    }

    if (pan_sys_nr <= 0) {
	fprintf(stderr, "Platform total is not positive: %d\n", pan_sys_nr);
	usage(argv[0]);
    }
    if (pan_sys_pid < 0 || pan_sys_pid >= pan_sys_nr) {
	fprintf(stderr, "Platform id is out of range 0 .. %d: %d\n",
		pan_sys_nr - 1, pan_sys_pid);
	usage(argv[0]);
    }

    j = 1;
    for(i = 3; i < *argc; i++){
	if (strcmp(argv[i], "-pan_pool_statistics") == 0){
	    pan_sys_pool_statistics();
	}else{
	    argv[j++] = argv[i];
	}
    }
    *argc = j;
    argv[j] = 0;

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
	sprintf(panda_id, "panda:%d.%d.%d:%d.%02d",
		t->tm_mday, t->tm_mon + 1, t->tm_year, t->tm_hour, t->tm_min);
	trc_start(panda_id, max_trace_buf);
	trc_set_level(0);			/* For now.... */
	trc_new_thread(0, "main");
    }
#endif

    pan_sys_time_start();
    pan_sys_sync_start();
    pan_sys_pool_start();
    pan_sys_buffer_start();
    pan_sys_msg_start();
    pan_sys_fragment_start();
    pan_sys_pset_start();
    pan_sys_nsap_start();
    pan_sys_glocal_start();
    pan_sys_thread_start();
    pan_sys_comm_start();
#ifdef TRACING
    pan_util_init();
#endif
}


void
pan_end(void)
{
    pan_sys_comm_end();
    pan_sys_thread_end();
    pan_sys_glocal_end();
    pan_sys_nsap_end();
    pan_sys_pset_start();
    pan_sys_fragment_end();
    pan_sys_msg_end();
    pan_sys_buffer_end();
    pan_sys_pool_end();
    pan_sys_sync_end();
    pan_sys_time_end();

#ifdef TRACING
    trc_end();
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


#ifdef NO
/* Added parameter set/get RFHH */
/* Thu Feb 16 12:14:24 MET 1995 */
void pan_sys_va_set_params(void *dummy, ...)
{
    va_list args;
    char   *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {
	/* Templates:
	if        (strcmp("PAN_SYS_n_buffers", cmd) == 0) {
	    n_buffers = va_arg(args, int);
	} else if (strcmp("PAN_SYS_timeout", cmd) == 0) {
	    pan_time_d2t(timeout, va_arg(args, double));
	} else {
	* end of templates */

	printf("%2d: No such system parameter: \"%s\" -- ignored\n",
		pan_my_pid(), cmd);
	va_arg(args, void*);

	cmd = va_arg(args, char*);
    }
}



void pan_sys_va_get_params(void *dummy, ...)
{
    va_list args;
    char   *cmd;
    char  **comm_stats;
    int     b_size;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {
	if (strcmp("PAN_SYS_statistics", cmd) == 0) {
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
#endif
