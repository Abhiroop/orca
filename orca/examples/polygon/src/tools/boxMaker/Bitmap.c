/*############################################################################*/
/*####   Bitmap.c:  Implementation of the functions declared in Bitmap.h. ####*/
/*##                                                                        ##*/

/*----------------------------------------------------------------------------*/
/* Includes.                                                                  */
/*----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <math.h>

#include "Bitmap.h"

/*----------------------------------------------------------------------------*/
/* initBitmap()                                                               */
/*----------------------------------------------------------------------------*/

 Boolean
initBitmap(
  Bitmap* bm, /* Bitmap that will be allocated of size n x m. */
  int     n,  /* Rows, n character pointers. */
  int     m   /* Columns, character arrays of size ceil(m / BITS_NBR) */
) {
  int     i;        /* Loop variable */
  size_t  arrSize;  /* Size of one char-array of m bits. */

    if (((*bm) = (Bitmap)calloc(n, sizeof(char *))) == NULL) {
	return( NULL );
    } /* if */

    arrSize = (size_t)ceil((double)m / (double)BITS_NBR);
    for (i=0; (i < n); i++) {
        if (((*bm)[i] = (char *)calloc(arrSize, sizeof(char))) == NULL) {
	    return( FALSE );
	} /* if */
    } /* for i */

    return (TRUE);
} /* initBitmap */

/*----------------------------------------------------------------------------*/
/* freeBitmap()                                                               */
/*----------------------------------------------------------------------------*/

 void
freeBitmap(
  Bitmap* bm, /* Bitmap to be deallocated. */
  int     n,  /* Rows. Must be of the same size as was allocated. */
  int     m   /* Columns. Is just passed for the sake of clarity. */
) {
  int  i;  /* Loop variable. */

    for (i=0; (i < n); i++) {
        free((*bm)[i]);
    } /* for i */
    free((*bm));
    *bm = NULL;
} /* freeBitmap */

/*##                                                                        ##*/
/*####   Bitmap.c:                                                        ####*/
/*############################################################################*/
