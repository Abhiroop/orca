/*==================================================*/
/* Original Author: Irina Athanasiu. */
/*  arc_aux.c  24 Jul 1995               */
/*  auxiliary functions for domain operations       */
/*==================================================*/

#include "arc.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int NN;                         /* number of variables in network */
int a, AA;                      /* number of values in each domain */
int e;                          /* number of edges (constraints) */
int d;                          /* number of different relations */
float sat;                      /* satisfiability of relations */
int r;                          /* number of relations per constraint */


char * domain;


 
/***********************************************************/
/* load_data - load constraint information from input file */
/***********************************************************/
void load_data(char *arg, int * pn, int * pa)
   {
   FILE    *data_fp;               /* constraint file pointer */
   char info[20];                  /* dummy char array to check EOF         */
   int a, i,k,l,m,s,t,rel;       /* Looping variables                 */
   unsigned long logic;
   int j, a1, i1, m1, l1, k1;

   data_fp = fopen(arg, "r");
   if(data_fp == NULL)
     {
	 printf("\n %s  FILE NOT FOUND", arg);
	 exit(0);
     }
   fscanf(data_fp, "%s %d", info, pn );  /* read in # of variables */
   fscanf(data_fp, "%s %d", info, pa );  /* read in size of domains */
   a = *pa;
   fscanf(data_fp, "%s %d", info, &e );  /* read in # of constraints */
   fscanf(data_fp, "%s %d", info, &d );  /* read in # of different relations */
   fscanf(data_fp, "%s %f", info, &sat );  /* satifiability */
   fscanf(data_fp, "%s %d", info, &r );  /* read in # of relation pairs */


   /* First we read in the list of relations.  each edge will use    */
   /* One of these relations */

   for(i = 0; i < MAXRELS; i++)
     relation[i] = 0;
   for (i = 0; i < MAXVARSSQ; i++)
      constraint[i] = 0;
   for  ( i=0 ; i<d ; i++) 
     {
	 fscanf(data_fp, "%s", info);  /* read in word "relation"  */
	 relation[i] = (char *) malloc ((a*a)*sizeof(char));
	 j = (a * a + 31) / 32; /* # of words */
	 a1 = a * a - 1;
	 i1 = -1;
	 for ( k=0 ; k< j ; k++ ) {
	     fscanf(data_fp, "%x", &m);
	     /* printf("%x(%d-1) ", m, THIS_NODE); */ 
	     logic = 0x1;
	     if (a1 < 32) 
	       m1 = a1;
	     else
	       m1 = 31;
	     for (l = 0; l < m1; l++)
	       logic = logic << 1;
	     
	     for ( l1 = l; l1 > -1 ; l1--) {
		 i1++;
		 if (logic & m) { k1 = i1 / a; m1 = i1 % a;
				  *(relation[i]+
				    ((k1*a+ m1)*sizeof(char))) = '1';
			      } 
		 logic = logic >> 1; 
	     }
        a1 -= 32;
      }
    }
 
   /*    printf("Read in edges \n"); */
   /* Now, read in the edge constraints */
   while(fscanf(data_fp, "%s", info) != EOF) 
     {
	 fscanf(data_fp, "%d %d %d", &s, &t, &rel);
	 constraint[(s*(*pn))+t] = relation[rel];
	 constraint[(t*(*pn))+s] = relation[rel];
     }
 fclose(data_fp);
}


char input_file[20];
char output_file[20];
char nume_fisier_read[20] ;
char nume_fisier_write[20];
char nume_program[20];

 
int count[MAXVARS];        /* counts the total number of domains */
int chosen[MAXVARS];        /* defines selected variables */


/***********************************************************/
/* functions for the partitioning                          */
/***********************************************************/
void load_data_for_partition(int n, FILE * data_fp)
   {
       char info[20];                /* dummy char array to check EOF */
       int i,s,t;                    /* Input variables */

       for (i=0; i<MAXVARS; i++) count[i] = chosen[i] = 0;

       /* read in variables and connectivity */
       for (i=0 ; i<n ; i++) 
	 {
	     fscanf(data_fp, "%s %d %d", info, &s, &t );
	     count[s] = t;
	 }
   }
 

/***********************************************************/
/* min - find the min of two values                        */
/***********************************************************/

 

int MIN(int a, int b)
   {
       return(a > b ? b : a);
   }
/***********************************************************/
/* MAX - find the MAX of two values                        */
/***********************************************************/
int MAX(int a, int b)
   {
       return(a > b ? a : b);
   }

/***********************************************************/
/* make_file - load constraint information from input file */
/***********************************************************/
int make_file(int n, FILE * out_fp, int processors[], int p)
{

    int i,j;                      /* Looping variables */
    int average;          /* average of domains */
    int total;            /* total number of domains */
    int temp_max;         /* total number of domains */
    int not_found;
    int goal_count;

    /* count the total # of connections - find average */
    total = 0;
    for ( i=0 ; i<n ; i++) total = total + count[i];

    for ( i=0 ; i<(p-1) ; i++) {
	temp_max = 0;
	for ( j=0 ; j<n ; j++) if (!chosen[j]) temp_max=MAX(count[j],temp_max);
	average = total/(p-i);
	while (average > 1) {
	    not_found=1;
	    goal_count = 1+(MIN(average,temp_max));
	    while(not_found) {
		goal_count--;  j=n-1;
		while ((j>(-1)) && (not_found)) {
		    if ((!chosen[j]) && (count[j] == goal_count)) 
		      not_found = 0;
		    else j--;
		}
		if ((goal_count == 2) && (j==-1)) not_found = 0;
            }
	    if (j>-1) {
		processors[j] = i;  chosen[j] = 1;
		average = average - count[j];
		total = total - count[j];
            }
	    else average = 0;
	}
    }
    for ( i=0 ; i<n ; i++) if (chosen[i] == 0) processors[i] = (p-1);

    /* output the node and partition information */
    fprintf(out_fp,"partition %i\n", p); 
    for ( i=0 ; i<n ; i++) {
	fprintf(out_fp,"member %i %3d\n", processors[i], i); 
    }
    return(0);
}


  void Init_val( int *an, int *aa, char *nume,  int processors[], int np)
{

   FILE    *data_fp;               /* constraint file pointer */
   FILE    *out_fp;                 /* file with partitions */


   int n, a;
   char info[20];

   data_fp = fopen(nume, "r");
   if(data_fp == NULL)
     {
	 printf("\n %s  FILE NOT FOUND", nume);
	 exit(0);
     }
   fscanf(data_fp, "%s %d", info, an );  /* read in # of variables */
   fscanf(data_fp, "%s %d", info, aa );  /* read in size of domains */
   fclose(data_fp);
   n = *an; a = *aa;

/*
  partition
*/
   sprintf(input_file,"%s.kl",nume);
   data_fp = fopen(input_file,"r");
   if(data_fp == NULL)
     {
	 printf("\n %s  FILE NOT FOUND", input_file);
	 exit(0);
     }
   load_data_for_partition(n, data_fp);
   fclose(data_fp);
   sprintf(output_file,"%s.part", nume);
   out_fp=fopen(output_file,"w");
   if(data_fp == NULL)
     {
	 printf("\n %s  FILE NOT FOUND", output_file);
	 exit(0);
     }

   if (make_file(n, out_fp, processors, np)) 
      fprintf(out_fp,"\nProblems!\n"), exit(0);
   fclose(out_fp);

}


void Init_constr(int * pn, int * pa, char * name)
{ 
    load_data(name, pn, pa);
}

/*********************************************/
/* verify that there is a predicate          */
/*********************************************/
BOOL test_constr(int var1, int var2, int n)
{
  return (constraint[var1 * n + var2]  != 0);
}

/*********************************************/
/* verify the predicate                      */
/*********************************************/

BOOL test(int var1, int val1, int var2, int val2, int n, int a)
{
/*
  IF var1 < var2          # Rij(li, lj) Ri1i2(k,l)=Ri2i1(l,k)
    THEN
      suport := suport OR relation[constraint[var1][var2]][val1][val2];
    ELSE
      suport := suport OR relation[constraint[var1][var2]][val2][val1];
  FI;
*/
    return  (var1 < var2) ? (  *(constraint[(var1*n)+var2]+
                                 ((val1* a +val2)*sizeof(char))) == '1') : 
                            (  *(constraint[(var1*n)+var2]+
                                 ((val2 * a +val1)*sizeof(char))) == '1');
                         
}
