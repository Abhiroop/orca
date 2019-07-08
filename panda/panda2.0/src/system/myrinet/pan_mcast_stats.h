/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group statistics module. */


#ifndef _SYS_MCAST_STATS_H_
#define _SYS_MCAST_STATS_H_

#include <assert.h>


#include "pan_sys_msg.h"

#include "pan_mcast_global.h"


typedef int pan_mcast_stat_t, *pan_mcast_stat_p;



#define STAT_COUNT 18

typedef enum STAT_IDX_T {
				/* Count sent messages */
	ST_SEND_GSB,
	ST_PURGE_SYNC,
	ST_SYNC_SYNC,
	ST_PURGE_ASYNC,
	ST_SYNC_ASYNC,
	ST_SYNC_WATCH,
	ST_REXMIT_REQ,
	ST_REXMIT_GSB,
	ST_REXMIT_HOME,
	ST_STATUS_SYNC,
	ST_STATUS_ASYNC,
				/* Count outgoing upcalls */
	ST_DIRECT_UPCALL,
	ST_UPCALL_ASIDE,
	ST_DELAYED_UPCALL,

				/* Count incoming upcalls */
	ST_LOW_LEVEL_UPCALL,

	ST_PURGE_PE_CNTRL_UPCALL,
	ST_ORDR_DATA_UPCALL,
	ST_ORDR_CNTRL_UPCALL,
}	stat_idx_t, *stat_idx_p;


#define DISCARD_COUNT 4

typedef enum BUFFER_IDX_T {
	DX_ORD_B,
	DX_HIST_B,
	DX_REXMIT,
	DX_NETWORK
}	buffer_idx_t, *buffer_idx_p;



#ifdef STATISTICS

void         STATINC(int idx);

void         STATDISCARD(int idx, int n);

void         STATDISCARD_CLEANUP(int idx, int n);

#else	/* STATISTICS */

#define STATINC(idx)
#define STATDISCARD(idx, n)
#define STATDISCARD_CLEANUP(idx, n)

#endif	/* STATISTICS */


pan_mcast_stat_p  pan_mcast_stats_create(void);

int              *pan_mcast_discards_create(void);

void              pan_mcast_stats_clear(pan_mcast_stat_p stat);

void              pan_mcast_discards_clear(int *discard);

void              pan_mcast_stats_init(void);

void              pan_mcast_stats_end(void);


char             *pan_mcast_sprint_stats(pan_mcast_stat_p stat);

char             *pan_mcast_sprint_purge_stats(pan_mcast_stat_p stat);

char             *pan_mcast_sprint_discards(int discard[],
					    int discard_cleanup[]);

char             *pan_mcast_sprint_purge_discards(int discard[],
						  int discard_cleanup[]);



void              pan_mcast_print_stats(pan_mcast_stat_p stat);

void              pan_mcast_print_discards(int discard[],
					   int discard_cleanup[]);

void              pan_mcast_print_purge_stats(pan_mcast_stat_p stat);

void              pan_mcast_print_purge_discards(int discard[],
						 int discard_cleanup[]);



#endif
