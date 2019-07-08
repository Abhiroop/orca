/* Author:         Tim Ruhl
 *
 * Date:           Fri Mar 15 13:08:45 MET 1996
 *
 * sleep:          Provide own sleep routine, because it is not
 *                 standard. XXX: This is a hack.
 */

#include "sleep.h"
#include "pan_sys.h"
#include "pan_util.h"

void
rts_sleep(unsigned int seconds)
{
    pan_time_p time;

    time = pan_time_create();
    pan_time_set(time, (long)seconds, 0UL);
    pan_sleep(time);
    pan_time_clear(time);
}

#ifdef PANDA_SLEEP

unsigned
sleep(unsigned int seconds)
{
    rts_sleep(seconds);
    return 0;
}
#endif /* PANDA_SLEEP */
    
