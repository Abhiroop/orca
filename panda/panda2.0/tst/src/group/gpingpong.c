/* Test pingpong latency for the group protocol.
 * Parameters:
 * mpingpong <my_platform n_platforms> <switches> msgs msg_size
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"

extern void sys_print_comm_stats(void);

#include "pan_mp.h"

#ifdef TRACING
#include "pan_trace.h"
#endif

#include "pan_group.h"


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


typedef struct HDR_T {
    short int sender;
    int       size;
} hdr_t, *hdr_p;


#ifdef TRACING
typedef struct TRACE_INFO_T {
    int size;
    int num;
    char msg[80];
} trace_info_t, *trace_info_p;

static trc_event_t RCVE_EV;
static trc_event_t START_SEND_EV;
static trc_event_t END_SEND_EV;
#endif

typedef enum MSG_FILL_STYLE_T {
    NO_FILL,
    FIXED_FILL,
    VAR_FILL
} msg_fill_style_t, *msg_fill_style_p;


#ifndef REPORT_RCVE
#  define REPORT_RCVE 1
#endif


static int              me;
static int              n_platforms;

pan_group_p		global_group;	/* For easy access in the debugger */


static pan_mutex_p	rcve_lock;
static pan_cond_p	my_turn;
static pan_cond_p	done;
static int		whose_turn = 0;
static int		msg_count = 0;
static int		msgs;

static int		max_msg_size;
static int		verbose   = 1;
static int    		human_msg = FALSE;
static int    		poll_on_sync = FALSE;
static msg_fill_style_t	fill_msg  = NO_FILL;
static int              check_msg = FALSE;


extern unsigned int sleep(unsigned int);



static int
str2int(const char int_str[])
{
    int res;

    if (sscanf(int_str, "%d", &res) != 1) {
	fprintf(stderr, "cannot convert to int: \"%s\"\n", int_str);
	abort();
    }
    return(res);
}


static char *
hdr_push(pan_msg_p msg, int size, int sender)
{
    hdr_p hdr;
    char *data;

    size = ((size + alignof(hdr_t) - 1) / alignof(hdr_t)) * alignof(hdr_t);
    data = pan_msg_push(msg, sizeof(hdr_t) + size, alignof(hdr_t));
    hdr = (hdr_p)(data + size);
    hdr->size   = size;
    hdr->sender = sender;

    return data;
}


static hdr_p
hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(hdr_t), alignof(hdr_t));
}


static hdr_p
hdr_look(pan_msg_p msg)
{
    return pan_msg_look(msg, sizeof(hdr_t), alignof(hdr_t));
}



static int
msg_contents_ok(char *str, int msg_size, int *byte)
{
    int i;

    switch (fill_msg) {
    case NO_FILL:
	break;
    case VAR_FILL:
	for (i = 0; i < msg_size - 1; i++) {
	    if (str[i] != ' ' + (i & 63)) {
		*byte = i;
		return(FALSE);
	    }
	}
	break;
    case FIXED_FILL:
	for (i = 0; i < msg_size - 1; i++) {
	    if (str[i] != 'a') {
		*byte = i;
		return(FALSE);
	    }
	}
	break;
    }
    return(TRUE);
}


static void
receive(pan_msg_p msg)
{
    hdr_p   hdr;
    char   *str;

#ifdef TRACING
    trace_info_t trace_info;
#endif

    hdr = hdr_look(msg);
    str = (char *)(hdr + 1);

#ifdef TRACING
    trace_info.size = hdr->size;
    trace_info.num  = msg_count;
    if (hdr->size != 0) {
	strncpy((char *) &trace_info.msg, str, 80);
    }
    trc_event(RCVE_EV, &trace_info);
#endif

    if (verbose >= 6) {
	if (hdr->size == 0) {
	    printf("%2d: message %3d received:  <empty>\n", me, msg_count);
	} else {
	    printf("%2d: message %3d received:  \"%s\"\n", me, msg_count,
		    str);
	}
    } else if (verbose >= 4) {
	if ((msg_count % REPORT_RCVE) == 0)
	    printf("%2d: message %3d (sender = %d, size = %d) received\n",
		    me, msg_count, hdr->sender, hdr->size);
    }

    if (human_msg) {
	assert(hdr->size == max_msg_size);
    } else if (check_msg) {
	int byte;

	if (! msg_contents_ok(str, hdr->size, &byte)) {
	    printf("%2d: rcve msg %p %d from %d: byte %d corrupted\n",
		   me, msg, msg_count, hdr->sender, byte);
	}
    }

    pan_mutex_lock(rcve_lock);
    if (verbose >= 30) {
	printf("%2d: rcve msg %d; whose_turn = %d\n",
		me, msg_count, whose_turn);
    }
    if (whose_turn == -1) {
	whose_turn = me + 1; 
    } else {
	++whose_turn;
    }
    if (whose_turn == n_platforms) {
	whose_turn = 0;
    }
    if (verbose >= 30) {
	printf("%2d: whose_turn := %d\n", me, whose_turn);
    }
    if (whose_turn == me) {
	pan_cond_signal(my_turn);
    }
    ++msg_count;
    if (msg_count == n_platforms * msgs) {
	pan_cond_signal(done);
    }

    if (hdr->sender != me) {
	pan_msg_clear(msg);
    }
    pan_mutex_unlock(rcve_lock);
}


static void
do_pingpong(pan_group_p g, int n)
{
    pan_msg_p  msg;
    char      *str;
    int        msg_size;
    int        i;
#ifdef TRACING
    trace_info_t trace_info;
#endif

    msg = pan_msg_create();

    if (human_msg) {
	msg_size = max_msg_size;
	str = (char*)hdr_push(msg, msg_size, me);
	sprintf(str, "msg %4d from host %d", n, me);
    } else {
	if (max_msg_size < 0) {
	    msg_size = 1 + (rand() % (-max_msg_size));
	} else {
	    msg_size = max_msg_size;
	}

	str = (char*)hdr_push(msg, msg_size, me);

	if (msg_size > 0) {

	    switch (fill_msg) {
	    case NO_FILL:
		break;
	    case VAR_FILL:
		for (i = 0; i < msg_size - 1; i++) {
		    str[i] = ' ' + (i & 63);
		}
		str[msg_size - 1] = '\0';
		break;
	    case FIXED_FILL:
		memset(str, 'a', msg_size - 1);
		str[msg_size - 1] = '\0';
		break;
	    }

	}

    }

    for (i = 0; i < msgs; i++) {
	pan_mutex_lock(rcve_lock);
	while (whose_turn != me) {
#ifdef POLL_ON_WAIT
	    if (poll_on_sync) {
		pan_mutex_unlock(rcve_lock);
		pan_poll();
		pan_mutex_lock(rcve_lock);
	    } else {
		pan_cond_wait(my_turn);
	    }
#else
	    pan_cond_wait(my_turn);
#endif
	}
	whose_turn = -1;
	pan_mutex_unlock(rcve_lock);

	if (verbose >= 25) {
	    printf("%2d: send grp msg %d of size %d\n", me, i, msg_size);
	}

#ifdef TRACING
	trace_info.size = msg_size;
	trace_info.num  = n;
	if (msg_size != 0) {
	    strncpy(trace_info.msg, str, 80);
	}
	trc_event(START_SEND_EV, &trace_info);
#endif

	pan_group_send(g, msg);

#ifdef TRACING
	trace_info.size = msg_size;
	trace_info.num  = n;
	trc_event(END_SEND_EV, &trace_info);
#endif

    }

    pan_mutex_lock(rcve_lock);
    while (msg_count < n_platforms * msgs) {
#ifdef POLL_ON_WAIT
	if (poll_on_sync) {
	    pan_mutex_unlock(rcve_lock);
	    pan_poll();
	    pan_mutex_lock(rcve_lock);
	} else {
	    pan_cond_wait(my_turn);
	}
#else
	pan_cond_wait(done);
#endif
    }
    pan_mutex_unlock(rcve_lock);

    pan_msg_clear(msg);
}




static void
usage(int argc, char *argv[])
{
     printf("usage: %s <cpu> <ncpu> <nr_msg> <msg_size> [options..]\n",
	     argv[0]);
     printf("options:\n");
     printf("   -check	: check message contents (meaningless without -fill)\n");
     printf("   -fill none 	: do not fill msg with characters\n");
     printf("   -fill fixed 	: fill msg with 1 character\n");
     printf("   -fill var 	: fill msg with different characters\n");
     printf("   -human	: human readable message\n");
     printf("   -nobuffer	: no buffering of stdout\n");
     printf("   -random	: msg size random up till <msg_size>\n");
     printf("   -statistics	: print msg statistics\n");
     printf("   -v <n>	: verbose level              (default 1)\n");
}


int
main(int argc, char** argv)
{
    pan_group_p g;
    int        i;
    pan_time_p start;
    pan_time_p now;
    int        statistics    = FALSE;
    int        option;
    char      *buf;

    if (argc < 5) {
	usage(argc, argv);
	exit(1);
    }

    pan_init(&argc, argv);

    start = pan_time_create();
    now   = pan_time_create();

    me          = pan_my_pid();
    n_platforms = pan_nr_platforms();

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-nobuffer") == 0) {
	    setbuf(stdout, NULL);
	} else if (strcmp(argv[i], "-fill") == 0) {
	    ++i;
	    if (strcmp(argv[i], "no") == 0) {
		fill_msg = NO_FILL;
	    } else if (strcmp(argv[i], "fixed") == 0) {
		fill_msg = FIXED_FILL;
	    } else if (strcmp(argv[i], "var") == 0) {
		fill_msg = VAR_FILL;
	    } else {
		printf("No such option: %s\n", argv[i]);
		usage(argc, argv);
	    }
	} else if (strcmp(argv[i], "-check") == 0) {
	    check_msg = TRUE;
	} else if (strcmp(argv[i], "-human") == 0) {
	    human_msg = TRUE;
	    max_msg_size = 30;
	} else if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_sync = TRUE;
	} else if (strcmp(argv[i], "-random") == 0) {
	    max_msg_size = -max_msg_size;
	} else if (strcmp(argv[i], "-statistics") == 0) {
	    statistics = TRUE;
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = str2int(argv[i]);
	} else {
	    if (option == 0) {
		msgs         = str2int(argv[i]);
	    } else if (option == 1) {
		max_msg_size = str2int(argv[i]);
	    } else {
		printf("No such option: %s\n", argv[i]);
		usage(argc, argv);
		exit(5);
	    }
	    ++option;
	}
    }

    if (verbose >= 1) {
	printf("%2d: past pan_init\n", me);
    }

    if (max_msg_size < 0) {
	srand(me);
    }

#ifdef TRACING
    trc_set_level(5000);
    if (me == 1) {
	RCVE_EV = trc_new_event(6000, sizeof(trace_info_t), "rcve_ev",
				"size = %d, num = %d, msg = \"%80s\"");
	START_SEND_EV = trc_new_event(6000, sizeof(trace_info_t),
				      "start_send_ev",
				      "size = %d, num = %d, msg = \"%80s\"");
	END_SEND_EV = trc_new_event(6000, sizeof(trace_info_t), 
				    "end_send_ev",
				    "size = %d, num = %d, msg = \"%80s\"");
    } else {
	START_SEND_EV = trc_new_event(6000, sizeof(trace_info_t),
				      "start_send_ev",
				      "size = %d, num = %d, msg = \"%80s\"");
	END_SEND_EV = trc_new_event(6000, sizeof(trace_info_t),
				    "end_send_ev", "size = %d, num = %d, msg = \"%80s\"");
	RCVE_EV = trc_new_event(6000, sizeof(trace_info_t), "rcve_ev",
				"size = %d, num = %d, msg = \"%80s\"");
    }
#endif


    rcve_lock  = pan_mutex_create();
    done       = pan_cond_create(rcve_lock);
    my_turn    = pan_cond_create(rcve_lock);
    msg_count  = 0;

    /* pan_mp_init(); */
    pan_group_init();

    pan_start();

    g = pan_group_join("group_1", receive);
    global_group = g;

    if (verbose >= 1) {
	printf("%2d: join the club...\n", me);
    }

    pan_group_await_size(g, n_platforms);

    pan_time_get(start);

    do_pingpong(g, msgs);

    pan_time_get(now);
    pan_time_sub(now, start);

    printf("%2d:      total time  : %f in %d ticks\n",
	    me, pan_time_t2d(now), msg_count);

    if (msg_count > 0) {
	pan_time_div(now, msg_count);
	printf("%2d:      per tick    : %f\n", me, pan_time_t2d(now));
    }

    pan_group_leave(g);

    pan_mutex_clear(rcve_lock);
    pan_cond_clear(done);
    pan_cond_clear(my_turn);

    pan_time_clear(start);
    pan_time_clear(now);

    if (statistics) {
	printf("%2d: Sent: %d messages; received: %d messages\n",
		me, msgs, msg_count);
	pan_group_va_get_g_params(g, "PAN_GRP_statistics", &buf, NULL);
	printf(buf);
	free(buf);
    }

    pan_group_clear(g);

    pan_group_end();
    /* pan_mp_end(); */

    if (statistics) {
	pan_sys_va_get_params(NULL, "PAN_SYS_statistics", &buf, NULL);
	printf(buf);
	free(buf);
	pan_sys_va_get_params(NULL, "PAN_SYS_mcast_statistics", &buf, NULL);
	printf(buf);
	free(buf);
    }

    pan_end();

    return(0);
}
