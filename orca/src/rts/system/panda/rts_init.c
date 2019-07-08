#include "panda/panda.h"
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

void
rts_init(int me, int nr_platforms, int trace_level)
{
    /* start Panda */
    pan_init(me, nr_platforms);
    rts_trc_start(trace_level);

    /* Initialise all modules in the right order */
#ifdef BLOCKING_UPCALLS
    cont_start(1);
#else
    cont_start(0);
#endif
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
    ac_end();
    p_end();
    man_end();
    fe_end();
    rc_end();
    otab_end();
    cont_end();
    pan_end();
}
