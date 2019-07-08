/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group statistics */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pan_sys.h"

	/*--- #include "grp_msg_buf.h" ---*/
	/*--- #include "grp_bb.h" ---*/
	/*--- #include "grp_frag.h" ---*/
	/*--- #include "grp_memlist.h" ---*/

#include "grp_types.h"
#include "grp_global.h"
#include "grp_stats.h"


#define LINE_SIZE	(8 * (STAT_COUNT + 2))





/* Statistics for buffer that is shared among groups */

pan_mutex_p         stat_lock;

int                 discard_intpt = 0;
int		    discard_cleanup_intpt = 0;
int                 discard_random = 0;
int		    discard_cleanup_random = 0;

static char        stat_name[STAT_COUNT][20] = {
    "snd pb",
    "ret pb",
    "snd bb",
    "ret bb",
    "acp pb",
    "acp bb",
    "suic",
    "sync/u",
    "sync/m",
    "watch",
    "rx pb",
    "rx bb",
    "status",
    "rx req",
    "rx lv",
    "up/d/s",
    "up/c/s",
    "up/d/m",
    "up/c/m",
    "acp/d",
    "acp/in",
    "late/h"
};


static char        buffer_name[BUFFERS_COUNT][20] = {
    "intpt",
    "ord",
    "seq",
    "hist",
    "bb",
    "accpt",
    "bb rt",
    "bb rx",
    "retrl",
    "rexmt",
    "self",
    "prem.",
    "prelv",
    "ooord",
    "hm fr",
    "rand"
};


#ifdef STATISTICS

#ifndef NO_STATISTICS_LOCK
#define LOCK(s)		pan_mutex_lock(s)
#define UNLOCK(s)	pan_mutex_unlock(s)
#else
#define LOCK(s)
#define UNLOCK(s)
#endif

void
STATINC(pan_group_p g, stat_idx_t idx)
{
    assert(idx < STAT_COUNT);
    LOCK(stat_lock);
    ++g->stat[idx];
    UNLOCK(stat_lock);
}

void
STATDISCARD(pan_group_p g, buffer_idx_t idx, int n)
{
    assert(idx < BUFFERS_COUNT);
    if (idx == DX_INTPT_B) {
	discard_intpt += n;
    } else if (idx == DX_RANDOM) {
	discard_random += n;
    } else {
	g->discard[idx] += n;
    }
}

void
STATDISCARD_CLEANUP(pan_group_p g, buffer_idx_t idx, int n)
{
    assert(idx < BUFFERS_COUNT);
    if (idx == DX_INTPT_B) {
	discard_cleanup_intpt += n;
    } else if (idx == DX_RANDOM) {
	discard_cleanup_random += n;
    } else {
	g->discard_cleanup[idx] += n;
    }
}



int *
init_grp_stats()
{
    int *stat;
    int         i;

    stat = pan_malloc(STAT_COUNT * sizeof(int));
    for (i = 0; i < STAT_COUNT; i++) {
	stat[i] = 0;
    }
    return stat;
}


#endif		/* STATISTICS */


int *
init_grp_discards()
{
    int        *discard;
    int         i;

    discard = pan_malloc(BUFFERS_COUNT * sizeof(int));
    for (i = 0; i < BUFFERS_COUNT; i++) {
	discard[i] = 0;
    }
    return discard;
}


void
clear_grp_stats(int *stat)
{
    pan_free(stat);
}


void
clear_grp_discards(int *discard)
{
    pan_free(discard);
}


void
init_group_module_stats(void)
{
    stat_lock = pan_mutex_create();
}


void
clear_group_module_stats(void)
{
    pan_mutex_clear(stat_lock);
}


static boolean
is_mem_stat(stat_idx_t stat)
{
    switch (stat) {
        case ST_SEND_PB:		return TRUE;
        case ST_RETRY_PB:		return TRUE;
        case ST_SEND_BB:		return TRUE;
        case ST_RETRY_BB:		return TRUE;
        case ST_ACCEPT_PB:		return FALSE;
        case ST_ACCEPT_BB:		return FALSE;
        case ST_SUICIDE:		return TRUE;
        case ST_SYNC_UC:		return FALSE;
        case ST_SYNC_MC:		return FALSE;
        case ST_WATCH:			return FALSE;
        case ST_REXMIT_PB:		return FALSE;
        case ST_REXMIT_BB:		return FALSE;
        case ST_STATUS:			return TRUE;
        case ST_REXMIT_REQ:		return TRUE;
        case ST_REXMIT_REQ_AT_LEAVE:	return TRUE;
        case ST_SEQ_DATA_UPCALL:	return FALSE;
        case ST_SEQ_CNTRL_UPCALL:	return FALSE;
        case ST_ORDR_DATA_UPCALL:	return FALSE;
        case ST_ORDR_CNTRL_UPCALL:	return FALSE;
        case ST_MEM_ACCEPT_DIRECT:	return TRUE;
        case ST_MEM_ACCEPT_LATE:	return TRUE;
	case ST_LATE_HOME:		return FALSE;
    }
    pan_panic("illegal stat switch\n");
    return FALSE;					/*NOTREACHED*/
}


static boolean
is_seq_stat(stat_idx_t stat)
{
    switch (stat) {
        case ST_SEND_PB:		return FALSE;
        case ST_RETRY_PB:		return FALSE;
        case ST_SEND_BB:		return FALSE;
        case ST_RETRY_BB:		return FALSE;
        case ST_ACCEPT_PB:		return TRUE;
        case ST_ACCEPT_BB:		return TRUE;
        case ST_SUICIDE:		return FALSE;
        case ST_SYNC_UC:		return TRUE;
        case ST_SYNC_MC:		return TRUE;
        case ST_WATCH:			return TRUE;
        case ST_REXMIT_PB:		return TRUE;
        case ST_REXMIT_BB:		return TRUE;
        case ST_STATUS:			return FALSE;
        case ST_REXMIT_REQ:		return FALSE;
        case ST_REXMIT_REQ_AT_LEAVE:	return FALSE;
        case ST_SEQ_DATA_UPCALL:	return TRUE;
        case ST_SEQ_CNTRL_UPCALL:	return TRUE;
        case ST_ORDR_DATA_UPCALL:	return FALSE;
        case ST_ORDR_CNTRL_UPCALL:	return FALSE;
        case ST_MEM_ACCEPT_DIRECT:	return FALSE;
        case ST_MEM_ACCEPT_LATE:	return FALSE;
	case ST_LATE_HOME:		return TRUE;
    }
    pan_panic("illegal stat switch\n");
    return FALSE;					/*NOTREACHED*/
}



static boolean
is_mem_discard(buffer_idx_t discard)
{
    switch (discard) {
        case DX_INTPT_B:		return FALSE;
        case DX_ORD_B:			return TRUE;
        case DX_SEQ_B:			return FALSE;
        case DX_HIST_B:			return FALSE;
        case DX_BB_B:			return TRUE;
        case DX_BB_ACCEPT:		return TRUE;
        case DX_BB_RETRIAL:		return TRUE;
        case DX_BB_REXMIT:		return TRUE;
        case DX_RETRIAL:		return FALSE;
        case DX_REXMIT:			return TRUE;
        case DX_SELF:			return TRUE;
        case DX_PREMATURE:		return TRUE;
        case DX_AWAIT_LEAVE:		return FALSE;
        case DX_OUT_OF_ORDER:		return FALSE;
	case DX_HOME_FRAG:		return TRUE;
	case DX_RANDOM:			return FALSE;
    }
    pan_panic("illegal discard switch\n");
    return FALSE;					/*NOTREACHED*/
}


static boolean
is_seq_discard(buffer_idx_t discard)
{
    switch (discard) {
        case DX_INTPT_B:		return FALSE;
        case DX_ORD_B:			return FALSE;
        case DX_SEQ_B:			return TRUE;
        case DX_HIST_B:			return TRUE;
        case DX_BB_B:			return FALSE;
        case DX_BB_ACCEPT:		return FALSE;
        case DX_BB_RETRIAL:		return FALSE;
        case DX_BB_REXMIT:		return FALSE;
        case DX_RETRIAL:		return TRUE;
        case DX_REXMIT:			return FALSE;
        case DX_SELF:			return FALSE;
        case DX_PREMATURE:		return FALSE;
        case DX_AWAIT_LEAVE:		return TRUE;
        case DX_OUT_OF_ORDER:		return TRUE;
	case DX_HOME_FRAG:		return FALSE;
	case DX_RANDOM:			return FALSE;
    }
    pan_panic("illegal discard switch\n");
    return FALSE;					/*NOTREACHED*/
}




char *
pan_group_sprint_stats(int *stat)
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 5);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 5;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:%-5s", pan_grp_me, "grp");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_mem_stat(i)) {
	    sprintf(buf, "%6s ", stat_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:%-5s", pan_grp_me, "grp");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_mem_stat(i)) {
	    sprintf(buf, "%6d ", stat[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    return buf_start;
}


char *
pan_group_sprint_seq_stats(int *stat)
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 5);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 5;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:%-5s", pan_grp_me, "grp");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_seq_stat(i)) {
	    sprintf(buf, "%6s ", stat_name[i]);
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:%-5s", pan_grp_me, "grp");
    buf = strchr(buf, '\0');
    for (i = 0; i < STAT_COUNT; i++) {
	if (is_seq_stat(i)) {
	    if (i == ST_SYNC_MC) {
		sprintf(buf, "%6d ", stat[i] - stat[ST_WATCH]);
	    } else {
		sprintf(buf, "%6d ", stat[i]);
	    }
	    buf = strchr(buf, '\0');
	}
    }
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    return buf_start;
}


char *
pan_group_sprint_discards(int discard[], int discard_cleanup[])
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 3);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 3;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:grp/d ", pan_grp_me);
    buf = strchr(buf, '\0');
    if (discard_intpt > 0) {
	sprintf(buf, "%5s ", buffer_name[DX_INTPT_B]);
	buf = strchr(buf, '\0');
    }
#ifdef RANDOM_DISCARD
    sprintf(buf, "%5s ", buffer_name[DX_RANDOM]);
    buf = strchr(buf, '\0');
#endif
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_mem_discard(i)) {
	    sprintf(buf, "%5s ", buffer_name[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:grp/d ", pan_grp_me);
    buf = strchr(buf, '\0');
    if (discard_intpt > 0) {
	sprintf(buf, "%5d ", discard_intpt);
	buf = strchr(buf, '\0');
    }
#ifdef RANDOM_DISCARD
    sprintf(buf, "%5d ", discard_random);
    buf = strchr(buf, '\0');
#endif
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_mem_discard(i)) {
	    sprintf(buf, "%5d ", discard[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

#ifdef PRINT_DISCARD_CLEANUP
    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "%2d:clean ", pan_grp_me);
    buf = strchr(buf, '\0');
    if (is_mem_discard(0)) {
	sprintf(buf, "%5d ", intpt_discard_cleanup);
	buf = strchr(buf, '\0');
    }
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_mem_discard(i)) {
	    sprintf(buf, "%5d ", discard_cleanup[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');
#endif
    return buf_start;
}


char
*pan_group_sprint_seq_discards(int discard[], int discard_cleanup[])
{
    int        i;
    char      *buf_end;
    char      *buf_start;
    char      *buf;

    buf     = pan_malloc(LINE_SIZE * 3);
    buf_start = buf;
    buf_end = buf + LINE_SIZE * 3;

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:grp/d ", pan_grp_me);
    buf = strchr(buf, '\0');
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_seq_discard(i)) {
	    sprintf(buf, "%5s ", buffer_name[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:grp/d ", pan_grp_me);
    buf = strchr(buf, '\0');
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_seq_discard(i)) {
	    sprintf(buf, "%5d ", discard[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');

#ifdef PRINT_DISCARD_CLEANUP
    if (buf_end < buf + LINE_SIZE) return buf_start;
    sprintf(buf, "S%1d:clean ", pan_grp_me);
    buf = strchr(buf, '\0');
    for (i = 1; i < BUFFERS_COUNT; i++)
	if (is_seq_discard(i)) {
	    sprintf(buf, "%5d ", discard_cleanup[i]);
	    buf = strchr(buf, '\0');
	}
    sprintf(buf, "\n");
    buf = strchr(buf, '\0');
#endif
    return buf_start;
}
