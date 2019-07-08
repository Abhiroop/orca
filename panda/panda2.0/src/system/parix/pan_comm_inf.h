#ifndef _SYS_T800_COMM_INF_
#define _SYS_T800_COMM_INF_


#include "pan_msg_cntr.h"


/*---- monitoring and statistics ---------------------------------------------*/

#define MEM_SLOT		0
#define LOST_SLOT		1
#define CONG_SLOT		2
#define DISC_SLOT		3
#define SEND_PB_DIRECT_SLOT	4
#define SEND_PB_INDIRECT_SLOT	5
#define SEND_GSB_SLOT		6
#define META_SLOT		7
#define TIME_SLOT		8
#define AHEAD_SLOT		9
#define UNICAST_SF_SLOT		10
#define UNICAST_SS_SLOT		11
#define UNICAST_QF_SLOT		12
#define UNICAST_QS_SLOT		13
#define UNICAST_RF_SLOT		14
#define UNICAST_RS_SLOT		15
#define UNICAST_A_SLOT		16

#define 	MEMBER_ITEMS		17

#define LISTENER_CNT		(MEMBER_ITEMS + 0)
#define UPCALL_CNT		(MEMBER_ITEMS + 1)
#define HISTORY_CNT		(MEMBER_ITEMS + 2)
#define SEND_CNT		(MEMBER_ITEMS + 3)
#define UNICAST_CNT		(MEMBER_ITEMS + 4)
#define PB_CNT			(MEMBER_ITEMS + 5)

#define 	COUNTER_ITEMS		6
#define 	COUNTER_RANGE		(3 * COUNTER_ITEMS)

#define 	INFO_ITEMS		(MEMBER_ITEMS + COUNTER_RANGE)

#define PB_DIRECT_SLOT			(INFO_ITEMS + 0)
#define PB_INDIRECT_SLOT		(INFO_ITEMS + 1)
#define GSB_SLOT			(INFO_ITEMS + 2)

#define 	SEQ_MEMBER_ITEMS	3

#define 	ON_MEMBER_ITEMS		(INFO_ITEMS + SEQ_MEMBER_ITEMS)

#define PB_COUNT		(ON_MEMBER_ITEMS + 0)
#define SEQ_THREADS_SLOT	(ON_MEMBER_ITEMS + 1)

#define 	SEQ_ITEMS		2

#define 	TOTAL_ITEMS		(ON_MEMBER_ITEMS + SEQ_ITEMS)


typedef int         info_t[TOTAL_ITEMS];

extern info_t       pan_comm_info_data;




#define pan_comm_info_set(i,val)	(pan_comm_info_data[i] = val)

#define pan_comm_info_get(i)		(pan_comm_info_data[i])


void pan_comm_info_register_counter(int n, pan_msg_counter_p c);

void pan_comm_info_seq_put(int *pb_direct, int *pb_indirect, int *gsb);

void pan_comm_info(char *str1, char *str2, char *str3, char *str4);

void pan_comm_info_start(void);

void pan_comm_info_end(void);

#endif
