/*---------------------------------------------------------------------------*/

/*  Generate a random NxN skyline-matrix A. and N-vector b
 *  Each nonzero element can range from 1 to max. 
 *  Diagonal elements range from max+1 to 2*max. A simple heuristic apparantly
 *  to avoid singularity and/or instability.
 *
 *  N and max are command line arguments.
 *
 *  format         :  N
 *                    skyline-matrix
 *	              vector
 *
 *  skyline-matrix :  skyline-columns 
 * 		      skyline-rows 
 *				              
 *  skyline-cols   :  {col-len(i) {elt}( j = 1..col-len(i) }( i = 1..N )
 *  skyline-rows   :  {row-len(i) {elt}( j = 1..row-len(i) }( i = 1..N )
 *
 *  vector         :  {elt}( j = 1 .. N )
 *
 *  0  <=  row-length(i)  <   i
 *  1  <=  col-length(i)  <=  i 
 *
 *  N   >= 1
 *  elt >= 1
 */

/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <malloc.h>

static unsigned long next = 1;

int rand()
{
	next = next * 1103515245 + 12345;
	return next & 0x7fffffff;
}

/*---------------------------------------------------------------------------*/

char *Me ;

/*---------------------------------------------------------------------------*/

#define EPF fprintf(stderr,

#define USAGE { EPF "Usage : %s [-s <skip>] [-i <minfill>] [-a <maxfill>] <size> [<maxval>]\n", Me ); exit(1) ; }

#define CHKARG( c, f, v ) \
	{ if(!*++nextarg){ EPF "Missing -%c flag argument\n", c ); USAGE ; } \
          sscanf( *nextarg, f, v ); \
        }
 
/*---------------------------------------------------------------------------*/

main(argc,argv)

  int    argc ;
  char **argv ;
{
  int    i,j,jr           ;
  int    size,sky         ;
  int    skip     = 1     ;
  int    max      = 100   ;
  int    argcount = 0     ;
  int    r        = 0     ;
  int    minfill  = 0     ;
  int    maxfill  = 100   ;
  long   filled   = 0L    ;
  char **nextarg          ;

  /* command line processing */

  Me = *argv ;

  while( *++argv ){

    if( **argv != '-' ){

      switch( ++argcount ){
      case 1  : sscanf( *argv, "%d", &size ) ; break ;
      case 2  : sscanf( *argv, "%d", &max  ) ; break ;
      default : fprintf( stderr, "Ignoring arg \"%s\"\n", *argv );
      }
      continue ;
    }

    nextarg = argv ; 

    while( *++*argv ){
      switch( **argv ){
      case 's' : CHKARG( 's', "%d", &skip ); 
		 if( skip < 1 ){
		   fprintf( stderr, "-s flag argument should be > 0\n" );
		   exit(1) ;
                 }
		 break ;
      case 'i' : CHKARG( 'i', "%d", &minfill ); 
		 if( minfill < 0 || minfill > 100 ){
		   fprintf( stderr, "-i flag argument out of range 0..100 \n" );
		   exit(1) ;
                 }
		 break ;
      case 'a' : CHKARG( 'a', "%d", &maxfill );
		 if( maxfill < 1 || maxfill > 100 ){
		   fprintf( stderr, "-a flag argument out of range 1..100 \n" );
		   exit(1) ;
                 }
		 break ;
      default  : fprintf( stderr, "Ignoring flag \"-%c\"\n", **argv );
      }
    }
    argv = nextarg ;
  }
  if(minfill>maxfill) { EPF "minfill > maxfill\n" ); USAGE ; }
  if(argcount<1) USAGE ;

  EPF "Size: %d, Max: %d, Skip: %d, %d %% <= Fill <= %d %%\n",
       size, max, skip, minfill, maxfill );  

  /*EPF "%d",minfill );*/
  max++ ;

  /**  <size> x <size> matrix A  **/

  printf( "%d\n\n", size );

  for( i=0 ; i<size ; i++ ){		/**  skyline-columns		**/
                                        /**  0 <= column size <= i	**/
       r  =  minfill            * i / 100 ;
       jr = (maxfill - minfill) * i / 100 ;

       sky = i%skip ? 0 : r + (jr?rand() % jr:0);	
       printf( "%d\t", sky + 1 );	/**  ( includes diagonal )	**/

       filled += sky + 1;

       for( j=0 ; j<sky ; j++ )		/**  <sky> nonzero elt.s	**/

	    printf( "%d ", rand() % max );

       printf( "%d\n", rand() % max + max ); /**  diagonal elt.	**/
  }

  putchar('\n');

  for( i=0 ; i<size ; i++ ){		/**   skyline-rows		**/
       					/**  0 <= row size <= i		**/
       r  =  minfill            * i / 100 ;
       jr = (maxfill - minfill) * i / 100 ;

       sky = i%skip ? 0 : r + (jr?rand() % jr:0);	
       printf("%d\t", sky );		/**  (exclude diagonal)		**/

       filled += sky ;

       for( j=0 ; j<sky ; j++ )         /**  <sky> nonzero elt.s	**/

	    printf( "%d ", rand() % max );

       putchar('\n');
  }
  putchar('\n');

  EPF "Density: %3.2lf %%\n",(double)filled/(double)(size*size)*100 );
  fflush(stderr);

  /**  <size>-vector b  **/

  for( j = 0 ; j < size ; j++ )
       printf("%3d ", rand() % max );

  putchar('\n');
}

/*---------------------------------------------------------------------------*/
