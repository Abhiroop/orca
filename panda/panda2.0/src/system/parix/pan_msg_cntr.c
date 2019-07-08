/* this is not a pool of messages, but some administration for
    managing quotas of messages */

#include <string.h>
#include <stdlib.h>

#include <sys/sem.h>

#include "pan_sys.h"

#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_msg_cntr.h"

#include "pan_parix.h"


void
pan_msg_counter_init(pan_msg_counter_p counter, int nr, char *name)
{
    counter->total_nr = nr;
    counter->max_used = nr;
    counter->name = pan_malloc(strlen(name) + 1);
    strcpy(counter->name, name);
    InitSem(&(counter->sema), nr);
}


void
pan_msg_counter_clear(pan_msg_counter_p counter)
{
    if (counter->sema.Count != counter->total_nr) {
	pan_sys_printf("(msg_counter_clear) %s: %d\n",
		   counter->name, counter->sema.Count);
    }
    /* pan_free(counter->name); */
}
