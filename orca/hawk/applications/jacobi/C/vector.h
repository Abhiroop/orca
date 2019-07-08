#ifndef __vector__
#define __vector__

#include "po.h"
#include "instance.h"

#define PRECISION  0.0001

void MaxDouble(double *v1, double *v2);

void do_check_result(int sender, instance_p vector, void **args);
void print_system(int sender, instance_p vector, void **args);
void init_system(int sender, instance_p vector, void **args);
void update_x(int part_num, instance_p vector, void **args);

int init_vector_class(int moi, int gsize, int pdebug);
int finish_vector_class(void);
instance_p vector_instance(int vector_size);

#endif

