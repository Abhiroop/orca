#ifndef __matrix5__
#define __matrix5__

#include "po.h"
#include "instance.h"

typedef struct elem {
               double ar, ai, tr, ti;
	   } ELEM, * PELEM;

typedef struct interval {
    int first, variant;
} INTERVAL, * PINTERVAL;

typedef struct argum {
    int number;
    int verbose;
} ARGUM, * PARGUM;

/* the maximum length of a message is 16k (2 ** 14),
   each element has 2 * sizeof(double) = 32 (2** 5)
   so the length of a partition can be of max 512 (2 ** 9) 
 */ 

#define MAXK 20


/* 2 ** 17 =131072 */ 
#define MAXN 131072


#define TWOPI 6.28318530717959
#define FIRST 0
#define SECOND 1

#define max_index(ind, other) (ind > other)? ind: other
#define min_index(ind, other) (ind < other)? ind: other

void SumDouble(double *v1, double *v2);

void Initial_A(int sender,instance_p instance, void **out_arg);

void A_computation(int part, instance_p instance, void **out_arg);
void Get_Print_A(int sender,instance_p instance, void **out_arg);
void A_convert(int sender, instance_p instance, void **out_arg);
void A_sum(int sender, instance_p instance, void **sum);
void A_threshold(int sender, instance_p instance, void **out_arg);
void insert_pdg_A_computation(int sender,instance_p instance, void **out_arg);

int read_input_data(char *name, int size);
int new_fft_class(int me, int gsize, int pdebug);
instance_p fft_instance(int number);

int k_log_2(unsigned long int n);
#endif




