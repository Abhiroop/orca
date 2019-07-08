#include "pan_util.h"

#include "pan_sleep.h"
#include "pan_time_fix.h"
#include "pan_clock_sync.h"
#include "pan_endian.h"
#include "pan_strdup.h"

void
pan_util_init(void)
{
    pan_sleep_start();
    pan_clock_sync_start();
    pan_strdup_start();
    pan_endian_start();
    pan_time_fix_start();
}

void
pan_util_end(void)
{
    pan_time_fix_end();
    pan_endian_end();
    pan_strdup_end();
    pan_clock_sync_end();
    pan_sleep_end();
}
