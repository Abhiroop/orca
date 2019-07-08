#ifndef __misc__
#define __misc__

#include "stdio.h"


#define init_module() init_module_fctn(&me,&group_size,&proc_debug,\
				       moi,gsize,pdebug,&initialized)
#define sys_error(c) { if (c) 						\
			 { fprintf(stderr,"sys_error-> %s: " #c "\n", 	\
				   MODULE_NAME);			\
			     exit(0); } 				\
		     }
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

typedef enum { FALSE = 0, TRUE = 1} boolean;

#define set_bit(v,b) ((v) | (1 << (b)))
#define reset_bit(v,b) (~( (~(v)) | (1 << (b))))
#define reverse_bit(v,b) ((((v) & (1 << (b)))) ? \
			  (~( (~(v)) | (1 << (b)))) : ((v) | (1 << (b))))
#define is_one(v,b) (((v) & (1 << (b))) ? TRUE : FALSE)
#define is_zero(v,b) (!is_one(v,b))

#define max(a,b) ((a)<(b) ? (b) : (a))
#define min(a,b) ((a)>(b) ? (b) : (a))
#define ceiling(a,b) (((a) % (b))==0 ? ((a)/(b)) : ((a)/(b))+1)

int init_module_fctn(int *me, int *group_size, int *proc_debug,
		     int moi, int gsize, int pdebug, int *initialized);
char *get_argument(int argc, char *argv[], char *flag);
int reverse_bits(int val, int bits);
int log2(int i);
int my_pow(int i,int j);
#endif

