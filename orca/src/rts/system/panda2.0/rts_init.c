/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include "pan_sys.h"
#include "continuation.h"
#include "obj_tab.h"
#include "process.h"
#include "proxy.h"
#include "rts_comm.h"
#include "rts_init.h"
#include "rts_trace.h"
#include "fork_exit.h"
#include "manager.h"
#include "account.h"
#include "rts_measure.h"
#include "rts_globals.h"

void
rts_init(int trace_level)
{
    rts_measure_start();
    rts_trc_start(trace_level);

    /* Initialise all modules in the right order */
    cont_start(use_threads);
    otab_start();
    rc_start();
    pr_start();
    fe_start();
    ac_start();         /* for man_init() please (quick IO hack) */
    man_start();
    p_start();
}


void
rts_end(void)
{
    cont_end();		/* stop cont_immediate_worker thread now */
    rc_end();		/* remove comm handler before clearing other modules */
    ac_end();
    p_end();
    man_end();
    fe_end();
    otab_end();
    rts_measure_end();
}
