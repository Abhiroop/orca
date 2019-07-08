#ifndef __grid__
#define __grid__

#include "po.h"
#include "instance.h"

extern double omega;

#define BLACK FALSE
#define RED TRUE

void MaxDouble(double *v1, double *v2);

pdg_p pdg_UpdateGrid(instance_p instance);

void PrintGrid(int sender, instance_p instance, void **args);
void InitGrid(int sender, instance_p instance, void **args);
void UpdateGrid(int sender, instance_p instance, void **args);

int new_grid_class(int moi, int gsize, int pdebug);
instance_p grid_instance(int num_rows, int num_cols, handler_p handler);

#endif
