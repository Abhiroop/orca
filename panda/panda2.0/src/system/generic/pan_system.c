/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_comm.h"
#include "pan_glocal.h"
#include "pan_threads.h"
#include "pan_message.h"
#include "pan_sync.h"
#include "pan_pset.h"
#include "pan_time.h"

/* ARCHITECTURE DEPENDENT */

#include "pan_trace.h"


#ifdef TRACING
#include "pan_util.h"
#include <time.h>
#endif

#include <stdio.h>


#define N_CLOCK_SYNCS 10	/* # msg exchanges to determine clock sync */


int      pan_sys_nr;
int      pan_sys_pid;
int      pan_sys_started = 0;

void
pan_init(int *argc, char *argv[])
{
    int i;

    if (*argc < 3){
	fprintf(stderr, "Usage: %s <pid> <nr platforms> [<appl args>]*\n",
		argv[0]);
	exit(1);
    }

    /* Extract pid and nr_platforms, and shift all other arguments */
    pan_sys_pid  = atoi(argv[1]);
    pan_sys_nr   = atoi(argv[2]);
    for(i = 1; i < *argc; i++){
	argv[i] = argv[i + 2];
    }
    *argc -= 2;
    argv[i] = 0;

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
    }
#endif

    pan_sys_time_start();
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
}


void
pan_end(void)
{
    pan_sys_comm_end();
    pan_sys_glocal_end();
    pan_sys_thread_end();
    pan_sys_nsap_end();
    pan_sys_pset_start();
    pan_sys_fragment_end();
    pan_sys_msg_end();
    pan_sys_buffer_end();
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



