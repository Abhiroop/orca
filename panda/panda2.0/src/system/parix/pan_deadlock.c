#include <assert.h>
#include <limits.h>

#include <sys/root.h>

#include "pan_sys.h"

#include "pan_error.h"

#include "pan_comm.h"
#include "pan_bcst_hst.h"
#include "pan_bcst_fwd.h"

#include "pan_deadlock.h"


/*
 * Deadlock detection
 */


static pan_thread_p deadlock_thread;


/* For Orca RTS investigation */
static int         magic_fetch_shared_objects = INT_MAX;
static int         magic_r_fork = INT_MAX;
static int         magic_fetching_shared_objects = INT_MAX;
static int         magic_awaiting_fetch = INT_MAX;
static int         magic_r_replicate = INT_MAX;
static int         magic_r_migrate = INT_MAX;
static int         magic_r_delete = INT_MAX;
static int         magic_r_move = INT_MAX;
static int         magic_r_mcast_invocation = INT_MAX;
static int         magic_delaying_ucast = INT_MAX;
static int         magic_r_exit = INT_MAX;

/* For broadcast investigation */
int                magic_cookie;
int                magic_watchdog[4] = {-1, -1, -1, -1};
int                magic_wachhond[4] = {-1, -1, -1, -1};

int                magic_bcast_upcall_ = INT_MAX;

/* For unicast investigation */
int                magic_ucast_upcall_ = INT_MAX;


static int         detect_counter = 0;
static int         detect_counter_1 = 0;
static int         detect_counter_2 = 0;
static int         detect_counter_3 = 0;
static int         detect_counter_4 = 0;


#define dead_check(x_name) \
	do { \
	    if (x_name < x_time) \
		pan_sys_printf("%s blocked?\n", #x_name); \
	} while (0)

#define dead_check_ext(x_name) \
	do { \
	    if (x_name < x_time) \
		pan_sys_printf("%s blocked (%d, %d, %d, %d, %d)?\n", \
			#x_name, detect_counter, \
			detect_counter_1, detect_counter_2, detect_counter_3, \
			detect_counter_4); \
	} while (0)


/*ARGSUSED*/
static void
deadlock_detect(void *arg)
{
    int         watchdog[4];
    int         wachhond[4];
    int         alle_tien;
    int         i;
    int         brechen;
    int         x_time;

    ChangePriority(LOW_PRIORITY);
    brechen = 0;
    alle_tien = 0;
    do {
	magic_cookie = 0;
	for (i = 0; i < 4; i++) {
	    watchdog[i] = magic_watchdog[i];
	    wachhond[i] = magic_wachhond[i];
	}
	TimeWaitHigh(TimeNowHigh() + CLK_TCK_HIGH * 10);
	/* next check for deadlock in 10 secs */
	if (magic_cookie < 0)
	    break;
	x_time = TimeNowHigh() - CLK_TCK_HIGH * 4;

	dead_check(magic_fetch_shared_objects);
	dead_check(magic_r_fork);
	dead_check(magic_fetching_shared_objects);
	dead_check(magic_awaiting_fetch);
	dead_check(magic_r_replicate);
	dead_check(magic_r_migrate);
	dead_check(magic_r_delete);
	dead_check(magic_r_move);
	dead_check(magic_r_mcast_invocation);
	dead_check(magic_delaying_ucast);
	dead_check(magic_r_exit);

	dead_check(magic_bcast_upcall_);
	dead_check_ext(magic_ucast_upcall_);

	for (i = 0; i < 4; i++) {
	    if (magic_watchdog[i] != -1 && magic_watchdog[i] == watchdog[i]) {
		pan_sys_printf("=============== LINK %d BLOCKED ============\n",
		       i);
		brechen = 1;
	    }
	    if (magic_wachhond[i] != -1 && magic_wachhond[i] == wachhond[i]) {
		pan_sys_printf("------------- LISTENER %d BLOCKED ----------\n",
		       i);
		brechen = 1;
	    }
	}
	if (magic_cookie == 0) {
	    alle_tien++;
	    if (alle_tien == 6)
		brechen = 1;
	} else
	    alle_tien = 0;
    } while (!brechen);

    if (magic_cookie != -1) {
	pan_sys_bcast_print_state();
	pan_bcast_hist_print(&pan_bcast_state.hist);
    }

    ChangePriority(LOW_PRIORITY);
}


void
pan_comm_deadlock_start(void)
{
    deadlock_thread = pan_thread_create(deadlock_detect, NULL, STACK,
					SYSTEM_PRIORITY_H, 0);
}


void
pan_comm_deadlock_end(void)
{
    pan_thread_join(deadlock_thread);
}
