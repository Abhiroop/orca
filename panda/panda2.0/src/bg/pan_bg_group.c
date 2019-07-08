/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_group.h"
#include "pan_bg_ack.h"
#include "pan_bg_bb_list.h"
#include "pan_bg_error.h"
#include "pan_bg_global.h"
#include "pan_bg_hist_list.h"
#include "pan_bg_history.h"
#include "pan_bg_index.h"
#include "pan_bg_order_list.h"
#include "pan_bg_rec.h"
#include "pan_bg_send.h"
#include "pan_bg_ticks.h"

#include <stdio.h>

/*
 * Driver for the group communication code.
 */

void
pan_bg_init(void)
{
    pan_bg_lock = pan_mutex_create();

    pan_mutex_lock(pan_bg_lock);

    pan_bg_all = pan_pset_create();
    pan_pset_fill(pan_bg_all);

    pan_bg_seqno  = SEQNO_START;
    pan_bg_rseqno = SEQNO_START;

    hist_start();
    ack_start();
    bb_start();
    index_start();
    order_start();
    history_start();
    rec_start();
    send_start();
    pan_bg_ticks_start();
    pan_bg_ticks_run();

    pan_mutex_unlock(pan_bg_lock);
}


void
pan_bg_end(void)
{
    pan_mutex_lock(pan_bg_lock);

    printf("Going to terminate\n");
    pan_bg_ticks_release();
    pan_bg_ticks_end();
    send_end();
    rec_end();
    history_end();
    order_end();
    index_end();
    bb_end();
    ack_end();
    hist_end();

    pan_pset_clear(pan_bg_all);

    pan_mutex_unlock(pan_bg_lock);
    pan_mutex_clear(pan_bg_lock);
}


