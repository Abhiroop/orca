/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __UTIL_CLOCK_SYNC_H__
#define __UTIL_CLOCK_SYNC_H__


#include "pan_sys.h"


/* This implementation does not require that upwards and downwards unicast
 * times be the same, and a first step in averaging is done automatically at
 * the cost of one more unicast and some delay; moreover, there is less
 * contention for the time server as _it_ takes the initiative:
 */
/*
 * Time server algorithm is nearly as simpleminded as is possible:
 * In turn, the time server exchanges time with each of the other hosts.
 * Half a time exchange consists of 2 unicasts:
 *
 *  Time server    Host		Exchange:	Server -Remember- Other:
 *      t0
 *          ----->		t0
 *                  t1						  t0,t1
 *          <-----		t1
 *      t2					t2
 *
 * Second half of time exchange is mirrored:
 *
 *                  t3						  t0,t1,t3
 *          <-----		t3
 *      t4					
 *          ----->		t2,t4
 *                  t5
 *
 * And now we have:
 *
 *   Real time between t0 and t1 = real time between t3 and t4 = d1
 *   ----------------- t1 --- t2 = ----------------- t4 --- t5 = d2
 *
 * Clock difference = d.
 *
 * Equations:
 * t0 + d1 = t1 - d                                     (1)
 * t2      = t1 - d + d2                                (2)
 *                      ((2) - (1): t2 - t0 = d1 + d2)          (3)
 * t3 + d1 = t4 + d                                     (4)
 * t5      = t4 + d + d2                                (5)
 *                      (4) - (1): t3 - t0 = t4 - t1 + 2d       (6a)
 *                      (5) - (2): t5 - t2 = t4 - t1 + 2d       (6b)
 *
 * The time server derives the time shift from Eq. 6a, the other host averages
 * between this value and Eq. 6b.
 */



/*-------------- Init, clear -------------------------------------------------*/

void        pan_clock_sync_start(void);

void        pan_clock_sync_end(void);


#endif
