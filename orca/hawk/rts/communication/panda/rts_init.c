/*
 * Author:         Tim Ruhl
 *
 * Date:           Thu Dec 14 10:02:33 MET 1995
 *
 * RTS initialization and cleanup
 */

#include <assert.h>
#include <string.h>

#include "communication.h"      /* Part of the communication module */
#include "util.h"
#include "pan_sys.h"
#include "pan_module.h"
#include "pan_group.h"
#include "pan_mp.h"
#include "pan_util.h"

#include "tpool.h"
#include "sync.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized;

extern void rts_sleep(unsigned int seconds); /* in synchronization/panda */

int
init_rts(int *argc, char **argv)
{
    int i, j;

    if (initialized++) return 0;

    pan_init(argc, argv);

    /* Panda specific initialization */
    pan_mp_init(argc, argv);
#ifndef USE_BG
    pan_group_init(argc, argv);
#else
    pan_bg_init(argc, argv);
#endif
    pan_util_init(argc, argv);		/* Needed for rts_sleep */

    /* Parse all Hawk arguments */
    j = 1;
    for(i = 1; i < *argc; i++){
	if (0) {
	} else if (strcmp(argv[i], "-hawk_warnings") == 0) {
	    rts_warnings_set(1);
	} else {
	    argv[j++] = argv[i];
	}
    }
    *argc = j;

    /* Initialize communication module */
    rts_tpool_init();
    rts_sync_init();    
    init_message(pan_my_pid(), pan_nr_platforms(), 0);
    init_mp_channel(pan_my_pid(), pan_nr_platforms(), 0);
    init_grp_channel(pan_my_pid(), pan_nr_platforms(), 0);

    /* Initialize synchronization module */
    init_atomic_int(pan_my_pid(), pan_nr_platforms(), 0);
    init_lock(pan_my_pid(), pan_nr_platforms(), 0);
    init_condition(pan_my_pid(), pan_nr_platforms(), 0);
    init_po_timer(pan_my_pid(), pan_nr_platforms(), 0);

    /* RTS initialization */
    me = pan_my_pid();
    group_size = pan_nr_platforms();
    proc_debug = 0;

    /* Start communication, because init_rts_object performs communication */
    pan_start();

    /* Initialize rts modules */
    init_rts_object(me, group_size, proc_debug, sizeof(int), argc, argv);

    return 0;
}

#ifdef FOUR_PHASE_STARTUP
int
start_rts(void)
{
    assert(initialized);

    /*
     * XXX: Should de something useful here to prevent messages to arrive
     * before everything is initialized.
     */

    return 0;
}


int
sync_rts(void)
{
    assert(initialized);

    finish_rts_object();

    return 0;
}
#endif

int
finish_rts(void)
{
    if (--initialized) return 0;

#ifndef FOUR_PHASE_STARTUP
    rts_sleep(3);
    finish_rts_object();
#endif

    /* Finish synchronization module */
    finish_po_timer();
    finish_condition();
    finish_lock();
    finish_atomic_int();

    /* Finish communication module */
    finish_grp_channel();
    finish_mp_channel();
    finish_message();
    rts_sync_end();
    rts_tpool_end();

    /* Finish Panda */
    pan_util_end();
    pan_group_end();
    pan_mp_end();
    pan_end();

    return 0;
}

int
get_group_configuration(int *m, int *gs, int *pd)
{
    *m = me;
    *gs = group_size;
    *pd = proc_debug;

    return 0;
}
