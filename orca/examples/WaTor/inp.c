/* 1993 for WatTor
/*
/* 
MODULE SPECIFICATION Inputc;

FUNCTION init_val( lung, lat, NF, NS, ABF, ABS, TS : SHARED integer;
                   fish, shark : SHARED Vali; UNIT : integer);

FUNCTION random ( secventa : SHARED Vali);

FUNCTION init_random();

END;

*/

#define MAXINT 65536 


#include <stdio.h>
#include <interface.h>
#if __STDC__
#include <stdlib.h>
#else
char *calloc();
#endif

#include <assert.h>
#include <string.h>


#define bool char
#define true 1
#define false 0

char nume_fisier[20];

void  read_random(padr,  l)
unsigned char  * padr;
int l;
{
  while (l-- > 0) {
  	*padr++ = (rand() >> 5);
  }
}





void f_Inputc__init_val(punit, pnr, plung, plat, pNF, pNS, pABF, pABS, pTS,
                        fish, shark)

int * punit;     /* dimension of the elementary element */
int * pnr;       /* number of operations in simulation */
int * plung;     /* the vertical dimension of the ocean */
int * plat;      /* the horizontal dimension of the ocean */
int * pNF;       /* the number of fishes */
int * pNS;       /* the number of sharks */
int * pABF;      /* age of breeding for the fishes */
int * pABS;      /* age of breeding for the sharks */
int * pTS;       /* time of starvation */
t_array * fish;  /* initial positions for fises */
t_array * shark; /* initila position for sharks */

{ 
  int i, j;
  int * fishp = fish   -> a_data;
  int * sharkp = shark -> a_data;
  unsigned short int x, y;
  bool gasit;

  printf("\nelementary dimension = "); scanf("%i", punit);
  printf("\nnumber of operations = "); scanf("%i", pnr);
  printf("\nfirst dimension = ");  scanf("%i", plung);
  printf("\nsecond dimension = "); scanf("%i", plat);
  printf("\nnumber of fishes = "); scanf("%i", pNF);
  printf("\nnumber of sharks = "); scanf("%i", pNS);
  printf("\nage of breeding for fishes = "); scanf("%i", pABF);
  printf("\nage of breeding for sharks = "); scanf("%i", pABS);
  printf("\ntime of starvation = "); scanf("%i", pTS);
  printf("\nresult file = "); scanf ("%s", nume_fisier);

  *plung *= *punit; *plat *= *punit;
  fish -> a_dims[0].a_lwb = 0; fish -> a_dims[0].a_nel = 2 * (*pNF);
  fish -> a_sz = 2 *(*pNF); fish -> a_offset = 0;


  for(i = 0; i < 2 * (*pNF); i +=2){
    do{ read_random((char*)&x, 2);
        x = x * (*plung)/MAXINT;
        read_random((char *)&y,2);
        y = y *  (*plat)/MAXINT;
        gasit = false;
        for (j = 0; ((j < i) && !gasit); j+=2)
           if ((x == fishp[j]) && (y == fishp[j + 1]))
              gasit = true;
        } while (gasit); 
    fishp[i] = x;
    fishp[i + 1] = y;
/*
      printf(" fish %i %i \n", fishp[i], fishp[i + 1]);
*/
  }

  shark -> a_dims[0].a_lwb = 0; shark -> a_dims[0].a_nel = 2 * (*pNS);
  shark -> a_sz = 2 *(*pNS); shark->a_offset = 0;
  for (i = 0; i < 2 * (*pNS); i +=2){
    do {
         read_random((char *)&x,2);
         x = x * (*plung)/MAXINT;
         read_random((char *)&y,2);
         y = y *  (*plat)/MAXINT;
        gasit = false;
         for (j = 0; ((j < 2 *(*pNF)) && !gasit); j+=2)
           if ((x == fishp[j]) && (y == fishp[j + 1]))
              gasit = true;
         if (!gasit){
          for (j = 0; ((j < i) && !gasit); j+=2)
           if ((x == sharkp[j]) && (y == sharkp[j + 1]))
              gasit = true;
         }
        } while (gasit); 
   sharkp[i] = x ;
   sharkp[i + 1] = y;
/*  printf("shark %i %i \n", sharkp[i], sharkp[i + 1]);
*/
  }
}


void f_Inputc__init_random(void)
{
  srand(10);
  }
 
void f_Inputc__random(secv)

t_array * secv ;

{
  unsigned int  short x, y;
  int * psecp = secv -> a_data;
  int i;
static  int variantex[24][4] =
                      {  {0, -1, +1,   0},
                         {0, -1,  0,  +1},
                         {0, +1,  -1,  0},
			 {0, +1,  0,  -1},
                         {0,  0, -1,  +1},
		         {0,  0, +1,  -1},

         		 {-1, 0,  1,   0},
         		 {-1, 0,  0,   1},
         		 {-1, 1,  0,   0},
         		 {-1, 1,  0,   0},
         		 {-1, 0,  1,   0},
         		 {-1, 0,  0,   1},
                        
			 {1,  0,  0,  -1},
			 {1,  0, -1,   0},
			 {1, -1,  0,   0},
		         {1, -1,  0,   0},
			 {1,  0,  0,  -1},
			 {1,  0, -1,   0},

			 {0, 0, -1,  1},
			 {0, 0,  1, -1},
			 {0,-1,  0,  1},
		         {0,-1,  1,  0},
			 {0, 1, -1,  0}, 
			 {0, 1,  0, -1}};


static  int  variantey[24][4] =
                       {   {-1, 0, 0,  1},
			   {-1, 0, 1,  0},
			   {-1, 0, 0,  1},
		           {-1, 0, 1,  0},
	                   {-1, 1, 0,  0},
			   {-1, 1, 0,  0},
 
			   {0, -1, 0,  1},
			   {0, -1, 1,  0},
			   {0,  0,-1,  1},
		           {0,  0, 1, -1},
			   {0,  1, 0, -1},
			   {0,  1, -1, 0},
                          
                           
                           {0, -1, 1,  0},
			   {0, -1, 0,  1},
			   {0,  0,-1,  1},
		           {0,  0, 1, -1},
			   {0,  1, -1, 0},
			   {0,  1, 0 ,-1},
                          
			   {1,  -1, 0, 0},
                           {1,  -1, 0, 0},
                           {1,   0, -1, 0},
			   {1,   0,  0,-1},
                           {1,   0,  0, -1},
                           {1,   0,  -1, 0}};

  read_random((char *)&x,2);
  y  =  24 * x / MAXINT;
  if(y > 23)
    { printf("\n secquence error \n"); 
      y = 0;
     }
  secv -> a_dims[0].a_lwb = 0; secv -> a_dims[0].a_nel = 8;
  secv -> a_sz = 8; secv -> a_offset = 0;
  for(i = 0; i < 4; i++){
    psecp[ 2 * i] = variantex[y][i];
    psecp[ 2 * i + 1] = variantey[y][i];
  }
}

FILE * out_fp;

void  prel_rez(var, n, text)
 t_array * var;
 int n;
 char * text;
{
  int i, j, max, sum, m;
  float average;
  int *varp = var->a_data;

  sum = 0; max = varp[0]; m = 0; average = 0;
  for (i = 1; i <= n; i++){
   fprintf(out_fp, "%s [ %i ] = %d\n", text, i, varp[i]);

   printf( "%s [ %i ] = %d\n", text, i, varp[i]);

   sum += varp[i];
   if (max < varp[i])
     max = varp[i];
   if(varp[i]) 
     m++;
  }

  if(m)
    average = (1. * sum) / m;

 fprintf(out_fp, " %s  => max = %d, sum = %d average = %f\n", 
          text, max, sum, average);
 printf(" %s  => max = %d, sum = %d average = %f\n", 
          text, max, sum, average);
}
void prel_rez_10(var, n, text)
 t_array * var;
 int n;
 char * text;
{
  int i, j;
  float max, sum, m;
  float average;
  int *varp = var->a_data;

  sum = 0; max = varp[0]; m = 0; average = 0;
  for (i = 1; i <= n; i++){
/*   fprintf(out_fp, "%s [ %i ] = %d\n", text, i, varp[i]);

   printf( "%s [ %i ] = %d\n", text, i, varp[i]);
*/
   sum += varp[i];
   if (max < varp[i])
     max = varp[i];
   if(varp[i]) 
     m++;
  }

  if(m)
    average = (1. * sum) / m;
 fprintf(out_fp, " %s  => max = %f, sum = %f average = %f\n", 
          text, max/ 1000., sum/ 1000., average/ 1000.0);
 printf(" %s  => max = %f, sum = %f average = %f\n", 
          text, max/1000., sum/ 1000., average/ 1000.);
}



void f_Inputc__write_rezultate(nslaves, nf, ns, time, timp, fish_n, shark_n, fish_e,
                               shark_s, noper_fis, noper_sh)
int nslaves, nf, ns, time;
t_array *timp, * fish_n, * shark_n, * fish_e, * shark_s, * noper_fis, * noper_sh;

{

 
 out_fp = fopen(nume_fisier, "a");
 fprintf(out_fp, "\nnslaves = %d, time = %f\n", nslaves, time / 1000.);
 printf("\nnslaves = %d, time = %f\n", nslaves, time / 1000.);
 fprintf(out_fp, "\nat the end there are %i fishes and %i sharks",nf, ns);
 printf("\nat the end there are %i fishes and %i sharks\n",nf, ns);
 prel_rez_10(timp, nslaves, "time");
 prel_rez(fish_n, nslaves, "new fishes");
 prel_rez(shark_n, nslaves, "new sharks");
 prel_rez(fish_e, nslaves, "eaten fishes");
 prel_rez(shark_s, nslaves, "starving sharks");
 prel_rez(noper_fis, nslaves, "operations on fishes");
 prel_rez(noper_sh, nslaves, "operation on sharks");

 fprintf(out_fp, "\n");

}
