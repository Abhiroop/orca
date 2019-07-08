#ifndef __po_marshall__
#define __po_marshall__

#include "sys_po.h"
#include "misc.h"
#include "message.h"

void marshall_partition(message_p m, instance_p i, int partnum);
void *unmarshall_partition(message_p m, instance_p i, int partnum);

#endif
