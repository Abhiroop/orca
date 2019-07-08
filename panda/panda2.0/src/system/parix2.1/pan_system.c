#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/root.h>
#include <sys/thread.h>
#include <sys/link.h>
#include <sys/memory.h>

#include "pan_global.h"
#include "pan_glocal.h"
#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_message.h"
#include "pan_threads.h"
#include "pan_comm_inf.h"
#include "pan_comm.h"
#include "pan_nsap.h"
#include "pan_pset.h"
#include "pan_sync.h"
#include "pan_time.h"
#include "pan_system.h"

#include "pan_trace.h"
#ifdef TRACING
#include <time.h>
#include "pan_util.h"
#endif

#define N_CLOCK_SYNCS 10        /* # msg exchanges to determine clock sync */

#include "pan_parix.h"

#undef pan_my_pid
#undef pan_nr_platforms

#define MAX_MESSAGE_NR 16

#define DEDICATED_SEQUENCER_TRIGGER  16	/* use dedicated Sequencer if more CPUs */

#define M_SNAKE  1
#define M_SNAIL  2
#define M_DOUBLE 3


int         pan_sys_nr;		/* N */
int         pan_sys_total_platforms;
int         pan_sys_sequencer;
int         pan_sys_pid;	/* Cache pan_my_pid() value */

int         pan_sys_mem_startup;

int         pan_sys_Parix_id;	/* The Parix id */
int         pan_sys_DimX;
int         pan_sys_DimY;
int         pan_sys_protocol;
int         pan_sys_dedicated;	/* set ( to 1 ), if sequencer is dedicated */
int         pan_sys_link_connected;
int         pan_sys_verbose_level;
int         pan_sys_save_check;

static int  pan_sys_computed_pid;	/* range: 0..N-1 */
static int  seqx, seqy;

static int  actual_server_threads;

static int  pan_sys_mapping_type;	/* M_SNAIL, M_SNAKE or M_DOUBLE */
static int  pan_sys_mapping_swap;	/* set (to 1), if dimensions swapped */
static int  pan_sys_mapping_offset;	/* offset ( != 0, if dim/2 odd ) */
static int  pan_sys_mapping_lb[4];	/* lower bound (>=) for pids in quadrant */
static int  pan_sys_mapping_ub[4];	/* upper bound (<) for pids in quadrant */
static int  pan_sys_mapping_xx[4];	/* x-Dim in standard-quadrant */
static int  pan_sys_mapping_yy[4];	/* y-Dim in standard-quadrant */

static int  x_wise[] = {0, 1, 3, 2};
static int  y_wise[] = {0, 3, 1, 2};

int         pan_sys_started = 0;

#define odd(x)	(((x) & 1) != 0)

/* msgpool_t msg_pool; */

static void
pan_sys_compute_mapping(void)
{
    int         i, j, tmp;

    pan_sys_mapping_swap = pan_sys_mapping_offset = 0;
    if (pan_sys_sequencer == -1) {
	seqx = (pan_sys_DimX - 1) / 2;
	seqy = (pan_sys_DimY - 1) / 2;
	pan_sys_sequencer = pan_sys_DimX * seqy + seqx;
    } else {
	seqx = pan_sys_sequencer % pan_sys_DimX;
	seqy = pan_sys_sequencer / pan_sys_DimX;
    }
    if (odd(pan_sys_DimX) && odd(pan_sys_DimY)) {
	pan_sys_mapping_type = M_SNAKE;
	return;
    }
    pan_sys_mapping_type = M_DOUBLE;
    if ((pan_sys_DimX % 4) == 0 && (pan_sys_DimY % 4) != 0)
	pan_sys_mapping_swap = 1;
    if (! odd(pan_sys_DimX) && odd(pan_sys_DimY))
	pan_sys_mapping_swap = 1;
    if ((pan_sys_DimX % 4) != 0 && (pan_sys_DimY % 4) != 0)
	if (pan_sys_mapping_swap)
	    pan_sys_mapping_offset = seqy;
	else
	    pan_sys_mapping_offset = seqx;

    pan_sys_mapping_xx[0] = pan_sys_mapping_xx[2] = seqx + 1;
    pan_sys_mapping_yy[0] = pan_sys_mapping_yy[1] = seqy + 1;
    pan_sys_mapping_xx[1] = pan_sys_mapping_xx[3] = pan_sys_DimX - seqx - 1;
    pan_sys_mapping_yy[3] = pan_sys_mapping_yy[2] = pan_sys_DimY - seqy - 1;

    tmp = 0;
    for (i = 0; i < 4; i++) {
	if (pan_sys_mapping_swap)
	    j = y_wise[y_wise[i]];
	else
	    j = x_wise[i];
	pan_sys_mapping_lb[i] = tmp;
	tmp += pan_sys_mapping_xx[j] * pan_sys_mapping_yy[j];
	pan_sys_mapping_ub[i] = tmp;
    }
}

int
pan_sys_proc_mapping(int pid)
{
    int         x, y, i, quad;

    pid += pan_sys_dedicated;	/* !!! complement to pan_sys_my_pid() !!! */

    switch (pan_sys_mapping_type) {
    case M_SNAKE:		/* fall through, M_SNAIL not implemented */
    case M_SNAIL:
	pid += pan_sys_sequencer;	/* HACK */
	if (pid >= pan_sys_total_platforms)
	    pid -= pan_sys_total_platforms;	/* HACK */
	x = pid % pan_sys_DimX;
	y = pid / pan_sys_DimX;
	if (odd(y))
	    x = pan_sys_DimX - x - 1;
	return y * pan_sys_DimX + x;

    case M_DOUBLE: /* fall out: */ ;
    }
    pid += pan_sys_mapping_offset;	/* shift back to quadrant borders */
    if (pid >= pan_sys_total_platforms)
	pid -= pan_sys_total_platforms;

    for (i = 0; i < 4; i++)		/* determine quadrant */
	if (pid >= pan_sys_mapping_lb[i] && pid < pan_sys_mapping_ub[i])
	    break;
    pid -= pan_sys_mapping_lb[i];	/* compute quadrant-relative pid */

    if (pan_sys_mapping_swap) {	/* determine standard-ordered quadrant */
	quad = y_wise[y_wise[i]];
	y = pid % pan_sys_mapping_yy[quad];	/* compute quadrant-relative
						 * coords */
	x = pid / pan_sys_mapping_yy[quad];
	if (pan_sys_mapping_offset != 0 && ! odd(i))	/* i = 0,2 */
	    y = pan_sys_mapping_yy[quad] - y - 1;	/* from snake to
							 * standard row-wise */
	if (odd(x))				/* i = all */
	    y = pan_sys_mapping_yy[quad] - y - 1;	/* from snake to
							 * standard row-wise */
    } else {
	quad = x_wise[i];
	x = pid % pan_sys_mapping_xx[quad];	/* compute quadrant-relative
						 * coords */
	y = pid / pan_sys_mapping_xx[quad];
	if (pan_sys_mapping_offset != 0 && ! odd(i))	/* i = 0,2 */
	    x = pan_sys_mapping_xx[quad] - x - 1;	/* from snake to
							 * standard row-wise */
	if (odd(y))				/* i = all */
	    x = pan_sys_mapping_xx[quad] - x - 1;	/* from snake to
							 * standard row-wise */
    }

    switch (i) {
    case 0:
	x = pan_sys_mapping_xx[quad] - x - 1; /* correct orientation */
	y = pan_sys_mapping_yy[quad] - y - 1; /* correct orientation */
	break;
    case 1:
	if (pan_sys_mapping_swap)
	    y += seqy + 1;
	else
	    x += seqx + 1;
	break;
    case 2:
	x += seqx + 1;	/* correct to standard count */
	y += seqy + 1;	/* correct to standard count */
	break;
    case 3:
	x = pan_sys_mapping_xx[quad] - x - 1;
	y = pan_sys_mapping_yy[quad] - y - 1;
	if (pan_sys_mapping_swap)
	    x += seqx + 1;
	else
	    y += seqy + 1;
	break;
    }
    return y * pan_sys_DimX + x;	/* range 0..N-1 */
}


static int
pan_sys_compute_pid(int x, int y)
{
    int         tmp, tmpx, tmpy, quadrant, i, pid;

    switch (pan_sys_mapping_type) {
    case M_SNAKE:		/* fall through, M_SNAIL not implemented */
    case M_SNAIL:
	if (odd(y))
	    x = pan_sys_DimX - x - 1;
	tmp = y * pan_sys_DimX + x;
	tmp += pan_sys_total_platforms - pan_sys_sequencer;	/* HACK */
	if (tmp >= pan_sys_total_platforms) {
	    tmp -= pan_sys_total_platforms;			/* HACK */
	}
	return tmp;

    case M_DOUBLE: /* fall out: */ ;
    }
    i = (x > seqx) + 2 * (y > seqy);	/* determine standard quadrant */

    tmpx = x;
    tmpy = y;
    if (pan_sys_mapping_swap)	/* determine quadrant */
	quadrant = y_wise[i];
    else
	quadrant = x_wise[i];

    switch (quadrant) {
    case 0:
	tmpx = pan_sys_mapping_xx[i] - tmpx - 1;	/* swap orientation */
	tmpy = pan_sys_mapping_yy[i] - tmpy - 1;	/* swap orientation */
	break;
    case 1:
	if (pan_sys_mapping_swap)
	    tmpy -= seqy + 1;
	else
	    tmpx -= seqx + 1;
	break;
    case 2:
	tmpx -= seqx + 1;	/* make coord quadrant-relative */
	tmpy -= seqy + 1;	/* make coord quadrant-relative */
	break;
    case 3:
	if (pan_sys_mapping_swap)
	    tmpx -= seqx + 1;
	else
	    tmpy -= seqy + 1;
	tmpx = pan_sys_mapping_xx[i] - tmpx - 1;
	tmpy = pan_sys_mapping_yy[i] - tmpy - 1;
	break;
    }

    if (pan_sys_mapping_swap) {
	if (pan_sys_mapping_offset != 0 && ! odd(quadrant))	/* 0,2 */
	    tmpy = pan_sys_mapping_yy[i] - tmpy - 1;	/* from snake to
							 * standard row-wise */
	if (odd(tmpx))
	    tmpy = pan_sys_mapping_yy[i] - tmpy - 1;	/* from snake to
							 * standard row-wise */
	pid = tmpx * pan_sys_mapping_yy[i] + tmpy;
    } else {
	if (pan_sys_mapping_offset != 0 && ! odd(quadrant))	/* 0,2 */
	    tmpx = pan_sys_mapping_xx[i] - tmpx - 1;	/* from snake to
							 * standard row-wise */
	if (odd(tmpy))
	    tmpx = pan_sys_mapping_xx[i] - tmpx - 1;	/* from snake to
							 * standard row-wise */
	pid = tmpy * pan_sys_mapping_xx[i] + tmpx;
    }

    pid += pan_sys_mapping_lb[quadrant] + pan_sys_total_platforms -
	   pan_sys_mapping_offset;
    pid %= pan_sys_total_platforms;
    return pid;			/* range 0..N-1 */
}

static char *rts_name;

static char protocolname[4][15] =
	{
	 "normal",
	 "link",
	 "dedicated",
	 "link+dedicated"
	};
static char policyname[8][14] =
	{
	 "MIX/1 src", "GSB/1 src", "MIX/1 src+UNI", "GSB/1 src+UNI",
	 "MIX/2 src", "GSB/2 src", "MIX/2 src+UNI", "GSB/2 src+UNI"
	};


static void delete_arg(int *argc, char *argv[], int *usurp)
{
    int i;

    --*argc;
    for (i = *usurp; i < *argc; i++) {
	argv[i] = argv[i + 1];
    }
    --*usurp;
}


void
pan_init(int *argc, char *argv[])
{
    int         i;
    mallinfo_t  usage;
    boolean     no_indirect_pb = FALSE;

    pan_sys_total_platforms = GET_ROOT()->ProcRoot->nProcs;
    pan_sys_Parix_id = GET_ROOT()->ProcRoot->MyProcID;
    pan_sys_DimX = GET_ROOT()->ProcRoot->DimX;
    pan_sys_DimY = GET_ROOT()->ProcRoot->DimY;

#if defined PARIX_PowerPC && defined NEVER
    pan_sys_protocol = (PROTO_pure_GSB | PROTO_pipe_ucast | PROTO_link_conn);
#else
    pan_sys_protocol = (PROTO_pure_GSB | PROTO_pipe_ucast);
#endif
    pan_sys_verbose_level = 0;
    actual_server_threads = 2;
    pan_sys_save_check = 0;
    pan_sys_sequencer = -1;

    usage = mallinfo();			/* remember size of free memory */
    pan_sys_mem_startup = usage.freearena;

    for (i = 1; i < *argc; i++) {

	assert(argv[i] != NULL);

	if (strcmp(argv[i], "-SYSdedicated") == 0) {
	    pan_sys_protocol |= PROTO_dedicated;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSpure_gsb") == 0) {
	    pan_sys_protocol |= PROTO_pure_GSB;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSlink") == 0) {
	    pan_sys_protocol |= PROTO_link_conn;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSmailbox") == 0) {
	    pan_sys_protocol &= ~PROTO_link_conn;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSmix") == 0) {
	    pan_sys_protocol &= ~PROTO_pure_GSB;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSno_indirect_pb") == 0) {
	    no_indirect_pb = TRUE;
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSsequencer") == 0) {
	    delete_arg(argc, argv, &i);
	    ++i;
	    if (sscanf(argv[i], "%d", &pan_sys_sequencer) != 1 ||
		pan_sys_sequencer < 0 ||
		pan_sys_sequencer > pan_sys_total_platforms) {
		printe("Illegal sequencer id value: %s\n", argv[i]);
	    }
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSserver") == 0) {
	    delete_arg(argc, argv, &i);
	    ++i;
	    if (sscanf(argv[i], "%d", &actual_server_threads) != 1 ||
		actual_server_threads < 0 ||
		actual_server_threads > 32) {
		printe("Illegal #server threads: %s\n", argv[i]);
	    }
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-SYSverbose") == 0) {
	    delete_arg(argc, argv, &i);
	    ++i;
	    if (sscanf(argv[i], "%d", &pan_sys_verbose_level) != 1 ||
		pan_sys_verbose_level < 0) {
		printe("Illegal verbose level: %s\n", argv[i]);
	    }
	    delete_arg(argc, argv, &i);
	} else if (strcmp(argv[i], "-?") == 0 ||
		   strncmp(argv[i], "-SYS", 4) == 0) {
	    printe("System level options:\n"
		   "\t-SYSdedicated\t\t: dedicated sequencer\n"
		   "\t-SYSpure_gsb\t\t: broadcast with GSB only (no PB)\n"
		   "\t-SYSmix\t\t\t: broadcast with GSB and PB\n"
		   "\t-SYSlink\t\t: connect to sequencer with links\n"
		   "\t-SYSmailbox\t\t: connect to sequencer with mailboxes\n"
		   "\t-SYSno_indirect_pb\t: do not broadcast with indirect PB\n"
		   "\t-SYSsequencer <node>\t: sequencer node (ParixID)\n"
		   "\t-SYSserver <num>\t: # sequencer server threads\n"
		   "\t-SYSverbose <num>\t: verbose level\n");
	    if (strncmp(argv[i], "-SYS", 4) == 0) {
		delete_arg(argc, argv, &i);
	    }
	}
    }

    pan_sys_glocal_start();
    pan_sys_error_start();

    pan_sys_dedicated = (pan_sys_protocol & PROTO_dedicated) == PROTO_dedicated;
    pan_sys_link_connected = (pan_sys_protocol & PROTO_link_conn) ==
				PROTO_link_conn;
    if (pan_sys_link_connected && !no_indirect_pb) {
	pan_sys_protocol |= PROTO_indirect_pb;
    }

    if (pan_sys_sequencer != -1)
	pan_sys_save_check = -1;

    pan_sys_compute_mapping();
    pan_sys_computed_pid = pan_sys_compute_pid(pan_sys_Parix_id % pan_sys_DimX,
					       pan_sys_Parix_id / pan_sys_DimX);

    if (pan_sys_dedicated) {
	pan_sys_pid = pan_sys_computed_pid - 1;
	if (pan_sys_pid == -1) {
	    pan_sys_pid += pan_sys_total_platforms;
	}
	pan_sys_nr = pan_sys_total_platforms - 1;
    } else {
	pan_sys_pid = pan_sys_computed_pid;
	pan_sys_nr = pan_sys_total_platforms;
    }

							/* test mapping */
    if (pan_sys_Parix_id != pan_sys_proc_mapping(pan_my_pid())) {
	pan_sys_printf("Parix_id = %d, pan_sys_proc_mapping(pan_my_pid()) = %d\n",
		pan_sys_Parix_id, pan_sys_proc_mapping(pan_my_pid()));
    }
    assert(pan_sys_Parix_id == pan_sys_proc_mapping(pan_my_pid()));
    assert(pan_sys_proc_mapping(pan_sys_compute_pid(seqx, seqy)) == pan_sys_sequencer);

    rts_name = argv[0];
    if (pan_sys_Parix_id == 0) {
#ifndef NOVERBOSE
	printf("%s # %s Type %s/P Policy %s/P Threads %d/S Verbose %d/O Check %d/Q\n",
	       rts_name, parix_environ->ArgV[0],
	       protocolname[pan_sys_protocol % 4],
	       policyname[pan_sys_protocol / 4],
	       actual_server_threads, pan_sys_verbose_level,
	       pan_sys_save_check == -1);
#endif
    }

    pan_sys_time_start();
    pan_sys_sync_start();
    pan_sys_pool_start();
    pan_sys_msg_start();
    pan_sys_fragment_start();
    pan_sys_pset_start();
    pan_sys_nsap_start();
    pan_sys_thread_start();

    pan_comm_info_start();

#ifdef TRACING

    /*
     * Switch on tracing before any call to trc_event can have occurred: i.e.
     * before group or ucast _init
     */
    {
	char        panda_id[256];	/* Derive name from argv list */
	int         max_trace_buf;	/* Derive value from argv list */
	time_t      tp;
	struct tm  *t;

	time(&tp);
	t = localtime(&tp);
	max_trace_buf = 1048576;/* For now.... */
	sprintf(panda_id, "panda-%d.%d.%d-%d.%02d",
		t->tm_mday, t->tm_mon + 1, t->tm_year, t->tm_hour, t->tm_min);
	trc_start(panda_id, max_trace_buf);
	trc_set_level(0);	/* For now.... */
	trc_new_thread(0, "main");
    }

    pan_util_init();
#endif

    if (pan_sys_dedicated && pan_sys_Parix_id == pan_sys_sequencer) {
	pan_sys_comm_start(actual_server_threads);
	pan_sys_started = 1;
    }
}

void
pan_start(void)
{
#ifdef TRACING
    struct pan_time t;
    struct pan_time dt;
#endif

    if (! pan_sys_dedicated || pan_sys_Parix_id != pan_sys_sequencer) {
	pan_sys_comm_start(actual_server_threads);
	pan_sys_started = 1;
    }
    /* pan_sys_msgpool_init(&msg_pool, MAX_MESSAGE_NR, "Message pool"); */
#ifdef TRACING
    pan_clock_sync(N_CLOCK_SYNCS, &t, &dt);
#endif
}

void
pan_end(void)
{
    pan_sys_comm_end();
    pan_sys_glocal_end();
    pan_sys_thread_end();
    pan_sys_nsap_end();
    pan_sys_pset_end();
    pan_sys_fragment_end();
    pan_sys_msg_end();
    pan_sys_pool_end();
    pan_sys_sync_end();
    pan_sys_time_end();

    pan_sys_error_end();

#ifdef TRACING
    trc_end();
#endif

    /* collect and print diagnostic info */
    pan_comm_info(rts_name, parix_environ->ArgV[0],
		  protocolname[pan_sys_protocol % 4],
		  policyname[pan_sys_protocol / 4]);

    pan_comm_info_end();
}


int
pan_my_pid(void)
{
    return pan_sys_pid;
}


int
pan_nr_platforms(void)
{
    return pan_sys_nr;
}


void
pan_sys_va_set_params(void *dummy, ...)
{
    va_list     args;
    char       *cmd;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (FALSE) {
	} else {
	    printf("No such system value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}

int
pan_sys_panda_id(int parix_id)
{
    return pan_sys_compute_pid(parix_id % pan_sys_DimX,
			       parix_id / pan_sys_DimY);
}


void
pan_sys_va_get_params(void *dummy, ...)
{
    va_list     args;
    char       *cmd;
    int        *i;

    va_start(args, dummy);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if (strcmp("PAN_SYS_mcast_sequencer", cmd) == 0) {
	    i = (int *)va_arg(args, int*);
	    *i = pan_sys_compute_pid(seqx, seqy);
	} else {
	    printf("No such system value tag: \"%s\" -- ignored\n", cmd);
	    va_arg(args, void *);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}
