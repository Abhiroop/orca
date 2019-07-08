#ifndef __int_obj__
#define __int_obj__

#include "po.h"
#include "instance.h"

void init_int(int sender, instance_p instance, void **out_arg);
void up_int(int sender, instance_p instance, void **out_arg);
void down_int(int sender, instance_p, void **out_arg);

int new_int(int me, int gsize, int pdebug);
instance_p int_instance(set_p processors);

#endif
