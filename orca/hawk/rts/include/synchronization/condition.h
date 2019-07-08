/*********************************************************************/
/*********************************************************************/
/* Condition variable abstract data type. */
/*********************************************************************/
/*********************************************************************/
#ifndef __condition__
#define __condition__

#include "po_timer.h"

typedef struct condition_s condition_t, *condition_p;

int init_condition(int me, int group_size, int proc_debug);
int finish_condition(void);

condition_p new_condition(void);
int free_condition(condition_p);

int signal_condition(condition_p);
int tawait_condition(condition_p, po_time_t time, unit_t unit);
int await_condition(condition_p);

#endif
