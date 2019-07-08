/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Group statistics module. */


#ifndef _GROUP_GRP_STATS_H_
#define _GROUP_GRP_STATS_H_

#include "pan_group.h"



#define STAT_COUNT 22

typedef enum STAT_IDX_T {
	ST_SEND_PB,
	ST_RETRY_PB,
	ST_SEND_BB,
	ST_RETRY_BB,
	ST_ACCEPT_PB,
	ST_ACCEPT_BB,
	ST_SUICIDE,
	ST_SYNC_UC,
	ST_SYNC_MC,
	ST_WATCH,
	ST_REXMIT_PB,
	ST_REXMIT_BB,
	ST_STATUS,
	ST_REXMIT_REQ,
	ST_REXMIT_REQ_AT_LEAVE,
	ST_SEQ_DATA_UPCALL,
	ST_SEQ_CNTRL_UPCALL,
	ST_ORDR_DATA_UPCALL,
	ST_ORDR_CNTRL_UPCALL,
	ST_MEM_ACCEPT_DIRECT,
	ST_MEM_ACCEPT_LATE,
	ST_LATE_HOME
}	stat_idx_t, *stat_idx_p;


#define BUFFERS_COUNT 16

typedef enum BUFFER_IDX_T {
	DX_INTPT_B,
	DX_ORD_B,
	DX_SEQ_B,
	DX_HIST_B,
	DX_BB_B,
	DX_BB_ACCEPT,
	DX_BB_RETRIAL,
	DX_BB_REXMIT,
	DX_RETRIAL,
	DX_REXMIT,
	DX_SELF,
	DX_PREMATURE,
	DX_AWAIT_LEAVE,
	DX_OUT_OF_ORDER,
	DX_HOME_FRAG,
	DX_RANDOM
}	buffer_idx_t, *buffer_idx_p;



#ifdef STATISTICS

void         STATINC(pan_group_p g, stat_idx_t idx);

void         STATDISCARD(pan_group_p g, buffer_idx_t idx, int n);

void         STATDISCARD_CLEANUP(pan_group_p g, buffer_idx_t idx, int n);

#else	/* STATISTICS */

#define STATINC(g, idx)
#define STATDISCARD(g, idx, n)
#define STATDISCARD_CLEANUP(g, idx, n)

#endif	/* STATISTICS */


int  *init_grp_stats(void);
int  *init_grp_discards(void);
void  clear_grp_stats(int *stat);
void  clear_grp_discards(int *discard);

void  init_group_module_stats(void);
void  clear_group_module_stats(void);


char *pan_group_sprint_stats(int *stat);
char *pan_group_sprint_seq_stats(int *stat);
char *pan_group_sprint_discards(int discard[], int discard_cleanup[]);
char *pan_group_sprint_seq_discards(int discard[], int discard_cleanup[]);


#endif
