#ifndef __lock__
#define __lock__

/*********************************************************************/
/*********************************************************************/
/* Lock abstract data type. */
/*********************************************************************/
/*********************************************************************/

#include "po_timer.h"

typedef struct po_lock_s po_lock_t, *po_lock_p;

int init_lock(int me, int group_size, int proc_debug);
int finish_lock(void);

po_lock_p new_lock(void);
int free_lock(po_lock_p);

int lock(po_lock_p);
int unlock(po_lock_p);

#endif
