#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "misc.h"
#include "precondition.h"

/*=======================================================================*/
int
init_module_fctn(int *me, int *group_size, int *proc_debug,
		 int moi, int gsize, int pdebug, int *initialized) {

  if ((*initialized)++) return 0;
  precondition((moi>=0)&&(moi<gsize));

  (*me)=moi;
  (*group_size)=gsize;
  (*proc_debug)=pdebug;
  return 1;
}

/*=======================================================================*/
/* Given argc and argv, this function returns the value of the
   argument associated with the given flag. For example, if a program
   is executed with -x 0 as argument, get_argument(argc,argv,"-x")
   will return "0". */
/*=======================================================================*/
char 
*get_argument(int argc, char *argv[], char *flag) {
  int i=0;
  
  for (i=0; i<(argc-1); i++) {
    if (strcmp(argv[i],flag)==0)
      return argv[i+1];
  }
  return NULL;
}

/*========================================================================*/
/* Reverses the bits of val according to the mask bits. */
/*========================================================================*/
int 
reverse_bits(int val, int bits) {
  int i;
  i=0;
  while (bits!=0) { 
    if (bits & 1) val=reverse_bit(val,i);
    bits = bits >> 1;
    i++;
  }
  return val;
}

/*========================================================================*/
/* Computes log base 2 of i. */
/*========================================================================*/
int 
log2(int i) {
  int j;
  j=0;
  do { 
    i = i>>1;
    j++;
  }
  while (i);
  return j-1;
}

/*========================================================================*/
/* Computes the jth power of an integer i. */
/*========================================================================*/
int 
my_pow(int i,int j) {
  int k,r;
 
  if (j<0) return 0;
  if (j==0) return 1;
  r=1;
  for (k=0;k<j;k++) r=r*i;
  return r;
}
