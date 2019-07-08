#ifndef __arc1__
#define __arc1__

#define BOOL unsigned int
#define PBOOL * unsigned int


#define MAXVARS 1600
#define MAXVARSSQ 2560000
#define MAXVAL  400
#define MAXPROC  100
#define MAXRELS  50


char * constraint[MAXVARSSQ]; 
char * relation[MAXRELS];       /* array of relation info */

void Init_constr(int * n, int * a, char * name);
void load_data(char *arg, int *pn, int *pa);
void Init_val( int *an, int *aa, char *nume, int   proc[], int np);
BOOL test_constr(int var1, int var2, int n);
BOOL test(int var1, int val1, int var2, int val2, int n, int a);

#endif 


