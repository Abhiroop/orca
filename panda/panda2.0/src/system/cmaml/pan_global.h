#ifndef _SYS_CMAML_GLOBAL_
#define _SYS_CMAML_GLOBAL_

#include <assert.h>

#ifdef CMOST
#include <cm/cmmd.h>
#ifdef NODE_TIMER
#define PANDA_TIMER 0
#endif
#else
#ifdef MAMA
#include "mama.h"
#else
#ifdef LAM
#include "gam_cmaml.h"
#endif /* LAM   */
#endif /* MAMA  */
#endif /* CMOST */

#ifdef MIN
#undef MIN
#endif
#define MIN(x, y)   ((x) < (y) ? (x) : (y))

#ifdef MAX
#undef MAX
#endif
#define MAX(x, y)   ((x) > (y) ? (x) : (y))

extern int pan_comm_winterval;               /* warning interval */
extern int pan_stats;
extern int pan_mcast_slack;

#endif /* _SYS_CMAML_GLOBAL_ */
