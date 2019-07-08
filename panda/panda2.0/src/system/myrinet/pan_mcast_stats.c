/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group statistics */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pan_sys_msg.h"

#include "pan_system.h"
#include "pan_sync.h"			/* Use macros */

	/*--- #include "pan_mcast_memlist.h" ---*/

#include "pan_mcast_global.h"
#include "pan_mcast_stats.h"


#define LINE_SIZE	(8 * (STAT_COUNT + 2))





/* Statistics for buffer that is shared among mcast groups */

static pan_mutex_p  stat_lock;


static char         stat_name[STAT_COUNT][20] = {
			"snd gsb",
			"purg/s",
			"sync/s",
			"purg/a",
			"sync/a",
			"watch",
			"rx req",
			"rx gsb",
			"rx hm",
			"stat/s",
			"stat/a",

			"up/dir",
			"up/sto",
			"up/lat",

			"up/low",
			"up/c/p",
			"up/d/m",
			"up/c/m",
		    };


static char         discard_name[DISCARD_COUNT][20] = {
			"ord",
			"hist",
			"rexmit",
			"netwrk"
		    };


#ifdef STATISTICS


void
STATINC(int idx)
{
    assert(idx < STAT_COUNT);
    pan_mutex_lock(stat_lock);
    ++pan_mcast_state.stat[idx];
    pan_mutex_unlock(stat_lock);
}

void
STATDISCARD(int idx, int n)
{
    pan_mcast_state.discard[idx] += n;
}

void
STATDISCARD_CLEANUP(int idx, int n)
{
    pan_mcast_state.discard_cleanup[idx] += n;
}



pan_mcast_stat_p
pan_mcast_stats_create(void)
{
    pan_mcast_stat_p stat;
    int         i;

    stat = pan_malloc(STAT_COUNT * sizeof(pan_mcast_stat_t));
    for (i = 0; i < STAT_COUNT; i++) {
	stat[i] = 0;
    }
    return stat;
}


#endif		/* STATISTICS */


int
*pan_mcast_discards_create(void)
{
    int        *discard;
    int         i;

    discard = pan_malloc(DISCARD_COUNT * sizeof(int));
    for (i = 0; i < DISCARD_COUNT; i++) {
	discard[i] = 0;
    }
    return discard;
}


void
pan_mcast_stats_clear(pan_mcast_stat_p stat)
{
    pan_free(stat);
}


void
pan_mcast_discards_clear(int *discard)
{
    pan_free(discard);
}


void
pan_mcast_stats_init(void)
{
    stat_lock = pan_mutex_create();
}


void
pan_mcast_stats_end(void)
{
    pan_mutex_clear(stat_lock);
}


static int
is_ordr_stat(stat_idx_t stat)
{
    switch (stat) {
        case ST_SEND_GSB:		return TRUE;
        case ST_PURGE_SYNC:		return FALSE;
        case ST_PURGE_ASYNC:		return FALSE;
        case ST_SYNC_SYNC:		return FALSE;
        case ST_SYNC_ASYNC:		return FALSE;
        case ST_SYNC_WATCH:		return FALSE;
#ifdef RELIABLE_NETWORK
        case ST_REXMIT_REQ:		return FALSE;
        case ST_REXMIT_GSB:		return FALSE;
        case ST_REXMIT_HOME:		return FALSE;
        case ST_STATUS_SYNC:		return FALSE;
        case ST_STATUS_ASYNC:		return FALSE;
#else
        case ST_REXMIT_REQ:		return TRUE;
        case ST_REXMIT_GSB:		return TRUE;
        case ST_REXMIT_HOME:		return TRUE;
        case ST_STATUS_SYNC:		return TRUE;
        case ST_STATUS_ASYNC:		return TRUE;
#endif

	case ST_DIRECT_UPCALL:		return TRUE;
	case ST_UPCALL_ASIDE:		return TRUE;
	case ST_DELAYED_UPCALL:		return TRUE;

        case ST_LOW_LEVEL_UPCALL:	return TRUE;

        case ST_PURGE_PE_CNTRL_UPCALL:	return FALSE;
        case ST_ORDR_DATA_UPCALL:	return TRUE;
#ifdef RELIABLE_NETWORK
        case ST_ORDR_CNTRL_UPCALL:	return FALSE;
#else
        case ST_ORDR_CNTRL_UPCALL:	return TRUE;
#endif
    }
    pan_panic("illegal stat switch\n");
    return FALSE;					/*NOTREACHED*/
}


static int
is_purge_stat(stat_idx_t stat)
{
#ifdef RELIABLE_NETWORK
    return FALSE;
#else
    switch (stat) {
        case ST_SEND_GSB:		return FALSE;
        case ST_PURGE_SYNC:		return TRUE;
        case ST_PURGE_ASYNC:		return TRUE;
        case ST_SYNC_SYNC:		return TRUE;
        case ST_SYNC_ASYNC:		return TRUE;
        case ST_SYNC_WATCH:		return TRUE;
        case ST_REXMIT_REQ:		return FALSE;
        case ST_REXMIT_GSB:		return FALSE;
        case ST_REXMIT_HOME:		return FALSE;
        case ST_STATUS_SYNC:		return FALSE;
        case ST_STATUS_ASYNC:		return FALSE;

	case ST_DIRECT_UPCALL:		return FALSE;
	case ST_UPCALL_ASIDE:		return FALSE;
	case ST_DELAYED_UPCALL:		return FALSE;

        case ST_LOW_LEVEL_UPCALL:	return FALSE;

        case ST_PURGE_PE_CNTRL_UPCALL:	return TRUE;
        case ST_ORDR_DATA_UPCALL:	return FALSE;
        case ST_ORDR_CNTRL_UPCALL:	return FALSE;
    }
    pan_panic("illegal stat switch\n");
    return FALSE;					/*NOTREACHED*/
#endif
}



static int
is_ordr_discard(buffer_idx_t discard)
{
    return TRUE;
}


static int
is_purge_discard(buffer_idx_t discard)
{
    return FALSE;
}



char *
pan_mcast_sprint_stats(pan_mcast_stat_p stat)
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 5);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 5;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:%-5s", pan_my_pid(), "msg");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_ordr_stat(i)) {
	    sprintf(buf, "%7s ", stat_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:%-5s", pan_my_pid(), "#");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_ordr_stat(i)) {
	    sprintf(buf, "%7d ", stat[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    return buf_start;
}


char *
pan_mcast_sprint_purge_stats(pan_mcast_stat_p stat)
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 5);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 5;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:%-5s", pan_my_pid(), "msg");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_purge_stat(i)) {
	    sprintf(buf, "%7s ", stat_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:%-5s", pan_my_pid(), "#");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_purge_stat(i)) {
	    sprintf(buf, "%7d ", stat[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    return buf_start;
}


char *
pan_mcast_sprint_discards(int discard[], int discard_cleanup[])
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 3);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 3;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:dscrd ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++) {
	if (is_ordr_discard(i)) {
	    sprintf(buf, "%6s ", discard_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:o/fly ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++)
	if (is_ordr_discard(i)) {
	    sprintf(buf, "%6d ", discard[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

#ifdef PRINT_DISCARD_CLEANUP
    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:clean ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++)
	if (is_ordr_discard(i)) {
	    sprintf(buf, "%6d ", discard_cleanup[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');
#endif
    return buf_start;
}


char *
pan_mcast_sprint_purge_discards(int discard[], int discard_cleanup[])
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 3);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 3;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:dscrd ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++) {
	if (is_purge_discard(i)) {
	    sprintf(buf, "%6s ", discard_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:o/fly ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++)
	if (is_purge_discard(i)) {
	    sprintf(buf, "%6d ", discard[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

#ifdef PRINT_DISCARD_CLEANUP
    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:clean ", pan_my_pid());
    buf = strchr(buf, '\0');
    for (i = 0; i < DISCARD_COUNT; i++)
	if (is_purge_discard(i)) {
	    sprintf(buf, "%6d ", discard_cleanup[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');
#endif
    return buf_start;
}



void
pan_mcast_print_stats(pan_mcast_stat_p stat)
{
    char      *buf;

    buf = pan_mcast_sprint_stats(stat);
    printf(buf);
    pan_free(buf);
}


void
pan_mcast_print_purge_stats(pan_mcast_stat_p stat)
{
    char       *buf;

    buf = pan_mcast_sprint_purge_stats(stat);
    printf(buf);
    pan_free(buf);
}


void
pan_mcast_print_discards(int discard[], int discard_cleanup[])
{
    char       *buf;

    buf = pan_mcast_sprint_discards(discard, discard_cleanup);
    printf(buf);
    pan_free(buf);
}


void
pan_mcast_print_purge_discards(int discard[], int discard_cleanup[])
{
    char       *buf;

    buf = pan_mcast_sprint_purge_discards(discard, discard_cleanup);
    printf(buf);
    pan_free(buf);
}
