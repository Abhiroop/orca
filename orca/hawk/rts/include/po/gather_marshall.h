#ifndef __gather_marshall__
#define __gather_marshall__

#include "sys_po.h"
#include "misc.h"
#include "message.h"

void gather_marshall(message_p m, instance_p i, int partnum, char *sb, int ss);
void *gather_unmarshall(message_p m, instance_p i, int partnum, char *rb, int rs);

#endif
