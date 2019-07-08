/*############################################################################*/
/*####   Bitmap.h:  A "Bitmap" type plus functions to work on that type.  ####*/
/*##                                                                        ##*/

/*                                                                            */
/* This library contains a 2 dimensional bitmap implementation.               */
/*                                                                            */
/* Implementation details:                                                    */
/* This Bitmap has been implemented with a array of character-pointers        */
/* pointing to character arrays. A character is 8 bits so to get to bit       */
/* bm(n,m) you have to get the character on place bm[n][m/8], and then the    */
/* (m % 8)th bit of that character.                                           */
/*                                                                            */


#ifndef Bitmap_h
#define Bitmap_h

/*----------------------------------------------------------------------------*/
/* Includes.                                                                  */
/*----------------------------------------------------------------------------*/

#include "Boolean.h"

/*----------------------------------------------------------------------------*/
/* Constants.                                                                 */
/*----------------------------------------------------------------------------*/

#ifndef NULL
#  define NULL		0
#endif

#define  BITS_NBR	8  /* Number of bits in a char. */

/*----------------------------------------------------------------------------*/
/* Types.                                                                     */
/*----------------------------------------------------------------------------*/

typedef char** Bitmap;
	/* This type represents a 2 dimensional bitmap.                       */

/*----------------------------------------------------------------------------*/
/* Functional macros.                                                         */
/*----------------------------------------------------------------------------*/

#define                                                    \
 /* void */                                                \
turnBitOn(                                                 \
  bm, /* Bitmap to turn the bit(n,m) 'on' in. */           \
  n,  /* Row coordinate of the bit. */                     \
  m   /* Collumn coordinate of the bit. */                 \
) (                                                        \
    (bm)[n][(m)/BITS_NBR] |= ((char)1) << ((m) % BITS_NBR) \
)

#define                                                         \
  /* void */                                                    \
turnBitOff(                                                     \
  bm, /* Bitmap to turn the bit(n,m) 'off' in. */               \
  n,  /* Row coordinate of the bit. */                          \
  m   /* Collumn coordinate of the bit. */                      \
) (                                                             \
    (bm)[n][(m)/BITS_NBR] &= ~( ((char)1) << ((m) % BITS_NBR) ) \
)

#define                                                              \
 /* Boolean */                                                       \
isBitOn(                                                             \
  bm, /* Bitmap to check wether bit(n,m) is on, if so return TRUE */ \
      /*  (non-zero) otherwise return FALSE (zero).               */ \
  n,  /* Row coordinate of the bit. */                               \
  m   /* Collumn coordinate of the bit. */                           \
) (                                                                  \
    (bm[n][(m)/BITS_NBR] & ((char)1) << ((m) % BITS_NBR))            \
)

/*----------------------------------------------------------------------------*/
/* Function prototypes.                                                       */
/*----------------------------------------------------------------------------*/

 Boolean
initBitmap(
  Bitmap* bm, /* Bitmap that will be allocated of size n x m. */
  int     n,
  int     m
);

 void
freeBitmap(
  Bitmap* bm, /* Bitmap, of n x m, to be deallocated. */
  int     n,  /* Rows. Must be of the same size as was allocated. */
  int     m   /* Columns. Is just passed for the sake of clarity. */
);

#endif

/*##                                                                        ##*/
/*####   Bitmap.h                                                         ####*/
/*############################################################################*/
