#ifndef __reduction_function__
#define __reduction_function__

typedef void (*reduction_function_p)(void *, void *);

#define reduction_function reduction_function_p 

int init_reduction_function(void);
int finish_reduction_function(void);

int register_reduction_function(reduction_function_p rf);
int release_reduction_function_p(reduction_function_p rf);
int release_reduction_function(int rf);

int function_index(reduction_function_p rf);
reduction_function_p function_pointer(int rf);

#endif

