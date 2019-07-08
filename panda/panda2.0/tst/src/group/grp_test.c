/* Test program for the group protocol.
 * Parameters:
 * test_1 my_platform n_platforms msgs msg_size <switches>
 * watch out: 1 <= my_platform <= n_platforms
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
    int		size;
    short int	sender;
    short int	group_id;
    unsigned char key;
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

pan_group_p	       *global_group;	/* For easy access in the debugger */
int                     n_groups = 1;


static pan_mutex_p	rcve_lock;
static pan_cond_p      *rcve_one;
static pan_cond_p	rcve_done;
static int    	       *go_msg_arrived;
static int	       *msg_count;
static int		total_msg_count;
static int		grand_total_msg_count = 0;
static int	       *my_msg_count;
static int		msgs;
static int		n_senders;

static int		max_msg_size;
static int		verbose   = 1;
static int    		human_msg = FALSE;
static int    		sync_send = FALSE;
static msg_fill_style_t	fill_msg  = NO_FILL;
static int              check_msg = FALSE;

#ifdef POLL_ON_WAIT
static int              poll_on_wait = FALSE;
#endif



#ifdef KOEN
static pan_time_p Start, Stop, Total;
static int Nr_timings = 0;
static int Valid = 0;
#endif


static void
hdr_push(pan_msg_p msg, int size, short int sender, short int group_id,
	 unsigned char key)
{
    hdr_p hdr;

    hdr = pan_msg_push(msg, sizeof(hdr_t), alignof(int));

    hdr->size     = size;
    hdr->sender   = me;
    hdr->group_id = group_id;
    hdr->key      = key;
}


static hdr_p
hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(hdr_t), alignof(int));
}


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


static double
str2double(const char double_str[])
{
    double res;

    if (sscanf(double_str, "%lf", &res) != 1) {
	fprintf(stderr, "cannot convert to double: \"%s\"\n", double_str);
	abort();
    }
    return(res);
}


static int
msg_contents_ok(char *str, int msg_size, int *byte, unsigned char key)
{
    int i;

    switch (fill_msg) {
    case NO_FILL:
	break;
    case VAR_FILL:
	key = ~key;
	for (i = 0; i < msg_size - 1; i++) {
	    if ((unsigned char)str[i] != key) {
		*byte = i;
		return(FALSE);
	    }
	    ++key;
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


static void receive(pan_msg_p msg)
{
    char   *str = NULL;
    hdr_p   hdr;

#ifdef TRACING
    trace_info_t trace_info;
#endif

#ifdef KOEN
    pan_time_get(Start);
    Valid = 1;
#endif

    hdr = hdr_pop(msg);

    if (hdr->size != 0) {
	str = pan_msg_look(msg, hdr->size, alignof(char));
    }

    if (! go_msg_arrived[hdr->group_id] && (! human_msg ||
		strncmp(str, "Hello world", strlen("Hello world")) != 0)) {
	assert(strcmp(str, "GO!") == 0);

	pan_mutex_lock(rcve_lock);
	go_msg_arrived[hdr->group_id] = TRUE;
	pan_cond_signal(rcve_one[hdr->group_id]);
	pan_mutex_unlock(rcve_lock);

	pan_msg_clear(msg);
	return;
    }

#ifdef TRACING
    trace_info.size = hdr->size;
    trace_info.num  = msg_count[hdr->group_id];
    if (hdr->size != 0) {
	strncpy((char *) &trace_info.msg, str, 80);
    }
    trc_event(RCVE_EV, &trace_info);
#endif

    if (verbose >= 6) {
	if (hdr->size == 0) {
	    printf("%2d: message %3d received:  <empty>\n",
		    me, msg_count[hdr->group_id]);
	} else {
	    printf("%2d: message %3d received:  \"%s\"\n",
		    me, msg_count[hdr->group_id], str);
	}
    } else if (verbose >= 4) {
	if ((msg_count[hdr->group_id] % REPORT_RCVE) == 0)
	    printf("%2d: message %3d (sender = %d, size = %d) received\n",
		    me, msg_count[hdr->group_id], hdr->sender, hdr->size);
    }

    if (human_msg) {
	assert(hdr->size == max_msg_size);
	if (strncmp(str, "Hello world", strlen("Hello world")) != 0 &&
	    strncmp(str, "Goodbye world", strlen("Goodbye world")) != 0) {
	    msg_count[hdr->group_id]++;
	}
    } else {
	/*
	assert(max_msg_size >= 0 ? hdr->size == max_msg_size :
				   hdr->size <= -max_msg_size);
	 */
	if (check_msg) {
	    int byte;

	    if (! msg_contents_ok(str, hdr->size, &byte, hdr->key)) {
		printf("%2d: received message %p %d from %d: byte %d corrupted\n",
		       me, msg, msg_count[hdr->group_id], hdr->sender, byte);
	    }
	}
	msg_count[hdr->group_id]++;
    }

    if (! sync_send) {
	if (hdr->sender == me) {
	    pan_mutex_lock(rcve_lock);
	    my_msg_count[hdr->group_id]++;
	    pan_cond_signal(rcve_one[hdr->group_id]);
	    pan_mutex_unlock(rcve_lock);
	}
	if (msg_count[hdr->group_id] == total_msg_count) {
	    pan_mutex_lock(rcve_lock);
	    grand_total_msg_count += total_msg_count;
	    pan_cond_signal(rcve_done);
	    pan_mutex_unlock(rcve_lock);
	}
    } else {
	if (msg_count[hdr->group_id] == total_msg_count) {
	    pan_mutex_lock(rcve_lock);
	    grand_total_msg_count += total_msg_count;
	    pan_cond_signal(rcve_done);
	    pan_mutex_unlock(rcve_lock);
	}
    }

    pan_msg_clear(msg);
}


static void send_m(pan_group_p g, int n, int group_id)
{
    pan_msg_p  msg;
    char      *str;
    int        msg_size;
    int        i;
    unsigned char key;
#ifdef TRACING
    trace_info_t trace_info;
#endif

    msg = pan_msg_create();
    if (human_msg) {
	msg_size = max_msg_size;
	str = pan_msg_push(msg, msg_size, alignof(char));
	sprintf(str, "msg %4d from host %d", n, me);
    } else {
	if (max_msg_size < 0) {
	    msg_size = 1 + (rand() % (-max_msg_size));
	} else {
	    msg_size = max_msg_size;
	}

	if (msg_size > 0) {
	    key = ~((unsigned char)n);
	    str = pan_msg_push(msg, msg_size, alignof(char));

	    switch (fill_msg) {
	    case NO_FILL:
		break;
	    case VAR_FILL:
		for (i = 0; i < msg_size - 1; i++) {
		    str[i] = key;
		    ++key;
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

    if (verbose >= 5) {
	if (msg_size == 0) {
	    printf("%2d: send message %3d:          <empty>\n", me, n);
	} else {
	    printf("%2d: send message %3d:          \"%s\"\n", me, n,
		    (char *)pan_msg_look(msg, msg_size, alignof(char)));
	}
    } else if (verbose >= 3) {
	    printf("%2d: send message %3d: (size = %d)\n", me, n,
		    msg_size);
    }

    hdr_push(msg, msg_size, me, group_id, (unsigned char)n);

#ifdef TRACING
    trace_info.size = msg_size;
    trace_info.num  = n;
    if (msg_size != 0) {
	strncpy(trace_info.msg, str, 80);
    }
    trc_event(START_SEND_EV, &trace_info);
#endif

#ifdef KOEN
    if (Valid) {
    	pan_time_get(Stop);
    	pan_time_sub(Stop,Start);
    	pan_time_add(Total,Stop);
	Nr_timings++;
	Valid = 0;
    }
#endif
    pan_group_send(g, msg);

#ifdef TRACING
    trace_info.size = msg_size;
    trace_info.num  = n;
    trc_event(END_SEND_EV, &trace_info);
#endif

}



static void sender(void *arg)
{
    int group_id = (int)arg;
    int i;
    pan_group_p g = global_group[group_id];

    for (i = 0; i < msgs; i++) {
	send_m(g, i, group_id);

	if (! sync_send) {
	    pan_mutex_lock(rcve_lock);
	    while (my_msg_count[group_id] != i) {
#ifdef POLL_ON_WAIT
		if (poll_on_wait) {
		    while (my_msg_count[group_id] != i) {
			pan_mutex_unlock(rcve_lock);
			pan_poll();
			pan_mutex_lock(rcve_lock);
		    }
		} else {
		    pan_cond_wait(rcve_one[group_id]);
		}
#else
		pan_cond_wait(rcve_one[group_id]);
#endif
	    }
	    pan_mutex_unlock(rcve_lock);
	}
    }

    pan_thread_exit();
}


static void
handle_go_msg(pan_group_p g, int sequencer, int group_id)
{
    pan_msg_p  msg;
    char      *str;

					/* Await join of all members */
    if (me == sequencer) {
	pan_group_await_size(g, pan_nr_platforms());
	msg = pan_msg_create();
	str = pan_msg_push(msg, strlen("GO!") + 1, alignof(char));
	sprintf(str, "GO!");
	hdr_push(msg, strlen("GO!") + 1, me, group_id, 0);

	pan_group_send(g, msg);
    }

    pan_mutex_lock(rcve_lock);
    while (! go_msg_arrived[group_id]) {
#ifdef POLL_ON_WAIT
	if (poll_on_wait) {
	    while (! go_msg_arrived[group_id]) {
		pan_mutex_unlock(rcve_lock);
		pan_poll();
		pan_mutex_lock(rcve_lock);
	    }
	} else {
	    pan_cond_wait(rcve_one[group_id]);
	}
#else
	pan_cond_wait(rcve_one[group_id]);
#endif
    }
    pan_mutex_unlock(rcve_lock);

}


static void usage(int argc, char *argv[])
{
     printf("usage: %s <cpu> <ncpu> <nr_msg> <msg_size> [options..]\n",
	     argv[0]);
     printf("options:\n");
     printf("   -bb <n>	: bb threshold\n");
     printf("   -check	: check message contents (meaningless without -fill)\n");
#ifdef AMOEBA
     printf("   -core	: dump core on a failing assert\n");
#endif
     printf("   -dedicated	: dedicated sequencer: implies -nohome\n");
     printf("   -g <n>		: number of groups (default 1)\n");
     printf("   -fill none 	: do not fill msg with characters\n");
     printf("   -fill fixed 	: fill msg with 1 character\n");
     printf("   -fill var 	: fill msg with different characters\n");
     printf("   -human	: human readable message\n");
     printf("   -nobuffer	: no buffering of stdout\n");
#ifdef POLL_ON_WAIT
     printf("   -poll	: poll on cond wait\n");
#endif
     printf("   -random	: msg size random up till <msg_size>\n");
     printf("   -s <n>	: n sending hosts            (default 1)\n");
     printf("   -s 'a'	: all are sender\n");
     printf("   -s 'm'	: all except sequencer member are sender\n");
     printf("   -s 's'	: sequencer member is sender (default not)\n");
     printf("   -statistics	: print msg statistics\n");
     printf("   -status <frac>	: fraction of hist buffer to send status\n");
     printf("   -sync_send	: send sync, no sync at high level\n");
     printf("   -v <n>	: verbose level              (default 1)\n");
     printf("   -watch <frac>	: fraction of hist buffer to send sync\n");
}


int          main(int argc, char** argv)
{
    int        n_platforms;
    int        i;
    pan_time_p start;
    pan_time_p now;
    int       *i_am_sender;
    int        seq_sends     = FALSE;
    int        dedicated_seq = FALSE;
    int        statistics    = FALSE;
    int        bb_large;
    int       *sequencer;
    pan_thread_p *sender_thread;
    int        option;
    char      *buf;
    char       name[32];
    double     f;

    if (argc < 5) {
	usage(argc, argv);
	exit(1);
    }

    pan_init(&argc, argv);

    start = pan_time_create();
    now   = pan_time_create();

    me = pan_my_pid();

    n_platforms  = pan_nr_platforms();

    n_senders = -1;

    option = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-bb") == 0) {
	    ++i;
	    bb_large = str2int(argv[i]);
	    pan_group_va_set_params(NULL, "PAN_GRP_bb_boundary", bb_large,
					NULL);
	} else if (strcmp(argv[i], "-nobuffer") == 0) {
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
#ifdef AMOEBA
	} else if (strcmp(argv[i], "-core") == 0) {
	    pan_sys_dump_core();
#endif
	} else if (strcmp(argv[i], "-dedicated") == 0) {
	    dedicated_seq = TRUE;
	} else if (strcmp(argv[i], "-g") == 0) {
	    ++i;
	    n_groups = atoi(argv[i]);
	} else if (strcmp(argv[i], "-human") == 0) {
	    human_msg = TRUE;
	    max_msg_size = 30;
#ifdef POLL_ON_WAIT
	} else if (strcmp(argv[i], "-poll") == 0) {
	    poll_on_wait = TRUE;
#endif
	} else if (strcmp(argv[i], "-random") == 0) {
	    max_msg_size = -max_msg_size;
	} else if (strcmp(argv[i], "-s") == 0) {
	    ++i;
	    switch (argv[i][0]) {
	    case 'a' : /* all are sender */
			n_senders = n_platforms;
			seq_sends = TRUE;
			break;
	    case 'm' : /* all except sequencer member are sender */
			n_senders = n_platforms - 1;
			break;
	    case 's' : /* sequencer member is sender */
			seq_sends = TRUE;
			break;
	    default  : /* n sending hosts; default 1 */
			n_senders = str2int(argv[i]);
	    }
	} else if (strcmp(argv[i], "-statistics") == 0) {
	    statistics = TRUE;
	} else if (strcmp(argv[i], "-status") == 0) {
	    ++i;
	    f = str2double(argv[i]);
	    pan_group_va_set_params(NULL, "PAN_GRP_hist_status", f, NULL);
	} else if (strcmp(argv[i], "-sync_send") == 0) {
	    sync_send = TRUE;
	} else if (strcmp(argv[i], "-v") == 0) {
	    ++i;
	    verbose = str2int(argv[i]);
	} else if (strcmp(argv[i], "-watch") == 0) {
	    ++i;
	    f = str2double(argv[i]);
	    pan_group_va_set_params(NULL, "PAN_GRP_hist_watch", f, NULL);
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

    if (n_senders == -1) {
	n_senders = 1;
	if (n_platforms == 1) {
	    seq_sends = TRUE;
	}
    }

    if (max_msg_size < 0) {
	srand(me);
    }

    global_group  = pan_malloc(n_groups * sizeof(pan_group_p));
    sequencer     = pan_malloc(n_groups * sizeof(int));
    i_am_sender   = pan_malloc(n_groups * sizeof(int));
    sender_thread = pan_malloc(n_groups * sizeof(pan_thread_p));
    rcve_one      = pan_malloc(n_groups * sizeof(pan_cond_p));
    my_msg_count  = pan_malloc(n_groups * sizeof(int));
    msg_count     = pan_malloc(n_groups * sizeof(int));
    go_msg_arrived = pan_malloc(n_groups * sizeof(int));
    for (i = 0; i < n_groups; i++) {
	go_msg_arrived[i] = FALSE;
	sequencer[i]      = 0;
	i_am_sender[i]    = FALSE;
	my_msg_count[i]   = -1;
	msg_count[i]      = 0;
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

#ifdef KOEN
    Start = pan_time_create();
    Stop = pan_time_create();
    Total = pan_time_create();
#endif

    rcve_lock = pan_mutex_create();
    for (i = 0; i < n_groups; i++) {
	rcve_one[i]  = pan_cond_create(rcve_lock);
    }
    rcve_done = pan_cond_create(rcve_lock);

    pan_mp_init();
    pan_group_init();

    /*
    pan_group_va_set_params(NULL, "PAN_GRP_hist_size", 32 * n_platforms, NULL);

    pan_group_va_set_params(NULL, "PAN_GRP_watch_timeout", 120.0, NULL);
    pan_group_va_set_params(NULL, "PAN_GRP_sync_timeout", 120.0, NULL);
    pan_group_va_set_params(NULL, "PAN_GRP_send_timeout", 120.0, NULL);
    pan_group_va_set_params(NULL, "PAN_GRP_retrans_timeout", 120.0, NULL);
    */

    pan_start();

    for (i = 0; i < n_groups; i++) {
	sprintf(name, "group_%d", i);
	global_group[i] = pan_group_join(name, receive);

	pan_group_va_get_g_params(global_group[i],
				    "PAN_GRP_sequencer", &sequencer[i],
				    NULL);

	if (me == sequencer[i]) {
	    i_am_sender[i] = seq_sends || n_senders == n_platforms;
	} else if (me > sequencer[i]) {
	    if (seq_sends) {
		i_am_sender[i] = (me < n_senders);
	    } else {
		i_am_sender[i] = (me < n_senders + 1);
	    }
	} else {
	    if (seq_sends) {
		i_am_sender[i] = (me < n_senders - 1);
	    } else {
		i_am_sender[i] = (me < n_senders);
	    }
	}

	if (sync_send) {
	    pan_group_va_set_g_params(global_group[i],
					"PAN_GRP_send_sync", TRUE, NULL);
	}
    }

    total_msg_count = n_senders * msgs;

    for (i = 0; i < n_groups; i++) {

	handle_go_msg(global_group[i], sequencer[i], i);

	if (verbose >= 1) {
	    if (i_am_sender[i]) {
		printf("%2d: am sender host for group %d\n", me, i);
	    }
	}

	if (dedicated_seq && me == sequencer[i]) {
	    pan_group_leave(global_group[i]);
	    if (verbose >= 1) {
		printf("%2d: sequencer member left\n", me);
	    }
	}
    }

    pan_time_get(start);

    for (i = 0; i < n_groups; i++) {
	if (i_am_sender[i]) {
	    sender_thread[i] = pan_thread_create(sender, (void *)i, 0,
						 pan_thread_minprio(), 0);
	}
    }

    for (i = 0; i < n_groups; i++) {
	if (i_am_sender[i]) {
	    pan_thread_join(sender_thread[i]);
	    pan_thread_clear(sender_thread[i]);
	}
    }

    pan_mutex_lock(rcve_lock);
    while (grand_total_msg_count < n_groups * total_msg_count) {
#ifdef POLL_ON_WAIT
	if (poll_on_wait) {
	    while (grand_total_msg_count < n_groups * total_msg_count) {
		pan_mutex_unlock(rcve_lock);
		pan_poll();
		pan_mutex_lock(rcve_lock);
	    }
	} else {
	    pan_cond_wait(rcve_done);
	}
#else
	pan_cond_wait(rcve_done);
#endif
    }
    pan_mutex_unlock(rcve_lock);

    pan_time_get(now);
    pan_time_sub(now, start);

    for (i = 0; i < n_groups; i++) {
	if (!dedicated_seq || me != sequencer[i]) {
	    if (verbose >= 2) {
		printf("%2d: going to leave\n", me);
	    }
	    pan_group_leave(global_group[i]);
	}
    }

    printf("%2d: time %.3f s; %d msgs; %d senders; thrp %.2f MB/s; ",
	    me, pan_time_t2d(now), grand_total_msg_count, n_senders,
	    (grand_total_msg_count * max_msg_size) /
		(1048576 * pan_time_t2d(now)));

    if (grand_total_msg_count > 0) {
	pan_time_div(now, grand_total_msg_count);
	printf("%.6f s/msg\n", pan_time_t2d(now));
    } else {
	printf("\n");
    }

#ifdef KOEN
    if (Nr_timings>0)
    	printf("%d: Total time = %f s in %d ticks (%f s per tick)\n", me,
	       pan_time_t2d(Total), Nr_timings, pan_time_t2d(Total)/Nr_timings);
#endif

    pan_mutex_clear(rcve_lock);
    for (i = 0; i < n_groups; i++) {
	pan_cond_clear(rcve_one[i]);
    }
    pan_cond_clear(rcve_done);

    pan_time_clear(start);
    pan_time_clear(now);

    if (statistics) {
	for (i = 0; i < n_groups; i++) {
	    if (! i_am_sender[i]) {
		msgs = 0;
	    }
	    printf("%2d: Sent: %d messages; received: %d messages\n",
		    me, msgs, total_msg_count);
	    pan_group_va_get_g_params(global_group[i],
					"PAN_GRP_statistics", &buf, NULL);
	    printf(buf);
	    free(buf);
	}
    }

    for (i = 0; i < n_groups; i++) {
	pan_group_clear(global_group[i]);
    }

    pan_group_end();
    pan_mp_end();

    if (statistics) {
	pan_sys_va_get_params(NULL, "PAN_SYS_statistics", &buf, NULL);
	printf(buf);
	free(buf);
    }

    pan_end();

    return(0);
}
