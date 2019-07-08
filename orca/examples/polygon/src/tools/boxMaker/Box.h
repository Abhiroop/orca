/*############################################################################*/
/*####   Box.h:  types "Box", "BoxArray" + functions on these types.      ####*/
/*##                                                                        ##*/

/*                                                                            */
/* This library was build to support the creation of boxes by expansion.      */
/*                                                                            */

#ifndef BOX_H
#define BOX_H

/*----------------------------------------------------------------------------*/
/* Includes.                                                                  */
/*----------------------------------------------------------------------------*/

#include <stdlib.h>

#ifndef NULL
#define NULL  0
#endif

/*----------------------------------------------------------------------------*/
/* Type "FreeSides" declarations, macros and functions.                       */
/*----------------------------------------------------------------------------*/

enum SIDEKINDS {
    NONE  = 0x0,                        /* No bit. */
    LEFT  = 0x1,                        /* Bit 0. */
    UP    = 0x2,                        /* Bit 1. */
    RIGHT = 0x4,                        /* Bit 2. */
    DOWN  = 0x8,                        /* Bit 3. */
    ALL   = (LEFT + UP + RIGHT + DOWN)  /* Bits 0, 1, 2 and 3. */
}; /* These constans are declared to be used for the type "FreeSides"    */

typedef unsigned char  FreeSides;
	/*                                                                    */
	/* This type will instantiate a variable which will indicate whether  */
	/* a side is free or not. It can also be used to hold a "SIDEKIND".   */
	/* For the good order the updating of this variable is not done       */
	/* automatically and must be done by the user of this library.        */


/*  Functional macros.  */

#define                                                         \
 /* void */                                                     \
turnSideFree(                                                   \
  pFreeSides, /* It's side(s) 'side' will be turned to free. */ \
  side        /* Can be LEFT, UP, RIGHT, DOWN or ALL. */        \
) (                                                             \
    (*(pFreeSides)) |= (side)                                   \
)

#define                                                             \
 /* void */                                                         \
turnSideNotFree(                                                    \
  pFreeSides, /* It's side(s) 'side' will be turned to not-free. */ \
  side        /* Can be LEFT, UP, RIGHT, DOWN or ALL. */            \
) (                                                                 \
    (*(pFreeSides)) &= ~(side)                                      \
)

#define                                                                   \
 /* Boolean */  /* Will be TRUE (non-zero) if "side" of "freeSides" is */ \
		/*  free, and FALSE (zero) if not. If "side" is ALL    */ \
		/*  then it will be TRUE if one of the sides is free.  */ \
isSideFree(                                                               \
  freeSides, /* The concerning freeSide. */                               \
  side       /* Can be LEFT, UP, RIGHT, DOWN or ALL. */                   \
) (                                                                       \
    ((freeSides) & (side))                                                \
)

#define                                                                     \
 /* FreeSide */ /* Returns the next side after "side". If "side" is DOWN */ \
		/*  or NONE the next will be LEFT.                       */ \
nextSide(                                                                   \
  side /* Side to take the next from. */                                    \
) (                                                                         \
    (side != NONE)? ((side << 1) % 15): LEFT                                \
)

#define                                                                    \
 /* FreeSide */ /* Will return a random side (LEFT, UP, RIGHT or DOWN). */ \
		/*  It doesn't check whether the returned side is free  */ \
		/*  or not.                                             */ \
randomSide(                                                                \
  /* void */                                                               \
) (                                                                        \
    1 << (random() % 4)                                                    \
)

/*----------------------------------------------------------------------------*/
/* Type "Box" declarations, macros and functions.                             */
/*----------------------------------------------------------------------------*/

typedef  struct {
	    int  x;
 	    int  y;
	}  Coordinate;
 
typedef struct {
	    Coordinate  corner0;    /* The two corners which         */
	    Coordinate  corner2;    /*  declare the whole rectangle. */
	    FreeSides   freeSides;  /* Indicates which box-sides are free. */
	}  Box;


/*  Functional macros.  */

#define                                           \
 /* Box* */  /* Will point to a allocated box. */ \
allocBox(                                         \
  /* void */                                      \
) (                                               \
    (Box*)malloc(sizeof(Box))                     \
)

#define                           \
 /* void */                       \
freeBox(                          \
  box /* Box to be dealocated */  \
) (                               \
    free(box)                     \
)

#define                                                       \
 /* int */  /* Will be the length of 'box' in x-direction. */ \
boxLengthX(                                                   \
  box /* Box to take the length of. */                        \
) (                                                           \
    (box).corner2.x - (box).corner0.x + 1                     \
)

#define                                                       \
 /* int */  /* Will be the length of 'box' in y-direction. */ \
boxLengthY(                                                   \
  box /* Box to take the length of. */                        \
) (                                                           \
    (box).corner2.y - (box).corner0.y + 1                     \
)

/*----------------------------------------------------------------------------*/
/* Type "BoxArray" declarations, macros and functions.                        */
/*----------------------------------------------------------------------------*/

typedef Box** BoxArray;


/*  Functional macros.  */

#define                                            \
 /* void */                                        \
addBox(                                            \
  boxArray, /* To put the box in. */               \
  index,    /* Place to put box in. */             \
  pBox      /* Pointing to the box to be added. */ \
) (                                                \
    (boxArray)[(index)] = (pBox)                   \
)

#define                                     \
 /* Box* */                                 \
getBox(                                     \
  boxArray, /* To extract box out of. */    \
  index     /* Points to the wanted box. */ \
) (                                         \
    (boxArray)[(index)]                     \
)

#define                                                               \
 /* Boolean */  /* Will be TRUE if initialization succeeded */        \
initBoxArray(                                                         \
  pBoxArray, /* To bind the allocated space to. */                    \
  size       /* The number of boxes in the array to be allocated. */  \
) (                                                                   \
    (*(pBoxArray) = (BoxArray)malloc((size) * sizeof(Box*))) != NULL  \
)

#define                                                                     \
 /* Boolean */                                                              \
resizeBoxArray(                                                             \
  pBoxArray, /* The array to be resized. */                                 \
  size      /* New size to be reallocated. */                               \
) (                                                                         \
    (*(pBoxArray) = (BoxArray)realloc(*(pBoxArray), (size) * sizeof(Box*))) \
    != NULL                                                                 \
)


/*  Function prototypes.  */

 void
swapBoxes(
  BoxArray boxArray, /* To swap boxes in. */
  int      index1,   /* Index of the first swap box. */
  int      index2    /* Index of the first swap box. */
);

#endif

/*##                                                                        ##*/
/*####   Box.h                                                            ####*/
/*############################################################################*/
