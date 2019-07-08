#ifdef STATISTICS
#include <stdio.h>
#endif

#include "pan_group.h"

#include "grp.h"
#include "grp_stats.h"


#ifdef STATISTICS

static char *stat_name[N_MSG_TICKS] = {
		    "s/data",
		    "s/join",
		    "s/leav",
		    "r/data",
		    "r/join",
		    "r/leav",
		    "upcall"
		};


static char *dcsrd_name[N_DSCRDS] = {
		    "unreg",
		    "no mem"
		};

#endif


void
pan_grp_stats_init(pan_group_p g)
{
#ifdef STATISTICS
    int i;

    for (i = 0; i < N_MSG_TICKS; i++) {
	g->ticks[i] = 0;
    }
    for (i = 0; i < N_DSCRDS; i++) {
        g->discards[i] = 0;
    }
#endif
}

void
pan_grp_stats_print(pan_group_p g)
{
#ifdef STATISTICS
    int i;

    printf("%2d: grp", pan_my_pid());
    for (i = 0; i < N_MSG_TICKS; i++) {
	printf(" %6s", stat_name[i]);
    }
    printf(" dscrd");
    for (i = 0; i < N_DSCRDS; i++) {
	printf(" %6s", dcsrd_name[i]);
    }
    printf("\n");

    printf("%2d: grp", pan_my_pid());
    for (i = 0; i < N_MSG_TICKS; i++) {
	printf(" %6d", g->ticks[i]);
    }
    printf("      ");
    for (i = 0; i < N_DSCRDS; i++) {
	printf(" %6d", g->discards[i]);
    }
    printf("\n");
#endif
}

void
pan_grp_stats_clear(pan_group_p g)
{
}

void
pan_grp_stats_start(void)
{
}

void
pan_grp_stats_end(void)
{
}
