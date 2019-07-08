#ifndef __vector__
#define __vector__

#include "po.h"
#include "instance.h"
#include "po_invocation.h"

typedef struct elem {
               double ar, ai;
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


void Initial_A(int sender,instance_p instance, void **args);
void A_computation(int part, instance_p instance, void **args);
void insert_pdg_A_computation(int sender,instance_p instance, void **args);

int new_fft_class(int me, int gsize, int pdebug);
instance_p fft_instance(int number,handler_p handler);

int k_log_2(unsigned long int n);
#endif




