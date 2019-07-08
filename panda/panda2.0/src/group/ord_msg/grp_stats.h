#ifndef __PAN_GRP_STATS_H__
#define __PAN_GRP_STATS_H__


#include "pan_group.h"


#ifdef STATISTICS


#define N_MSG_TICKS	7

typedef enum STAT_TICKS {
    GRP_ST_SEND_DATA,
    GRP_ST_SEND_JOIN,
    GRP_ST_SEND_LEAVE,
    GRP_ST_RCVE_DATA,
    GRP_ST_RCVE_JOIN,
    GRP_ST_RCVE_LEAVE,
    GRP_ST_UPCALL
} stat_ticks_t, *stat_ticks_p;


#define N_DSCRDS	2

typedef enum DSCRDS {
    GRP_DX_UNREG,
    GRP_DX_NO_MEM
} dscrd_ticks_t, *dscrd_ticks_p;


#define STATINC(g, t)		(++(g)->ticks[t])
#define DISCARDINC(g, t)	(++(g)->discards[t])

#else

#define STATINC(g, t)
#define DISCARDINC(g, t)

#endif


void pan_grp_stats_init(pan_group_p g);

void pan_grp_stats_print(pan_group_p g);

void pan_grp_stats_clear(pan_group_p g);


void pan_grp_stats_start(void);

void pan_grp_stats_end(void);


#endif
