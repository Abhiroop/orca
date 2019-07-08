#ifndef __RTS_PO_TIMER_H__
#define __RTS_PO_TIMER_H__

#include "synchronization.h"
#include "pan_sys.h"

struct po_timer_s {
    char         *name;
    pan_time_p    time;
    pan_time_p    total;
    unsigned long clicks;

    po_timer_p next;
};

/*
 * rts_po_timer_time2panda:
 *                 Convert from (po_time, unit_t) to Panda time.
 */
void rts_po_timer_time2panda(po_time_t time, unit_t unit, pan_time_p p);

/*
 * rts_po_timer_panda2time:
 *                 Convert from Panda time to po_time with unit unit.
 */
po_time_t rts_po_timer_panda2time(pan_time_p p, unit_t unit);

#endif /* __RTS_PO_TIMER_H__ */
