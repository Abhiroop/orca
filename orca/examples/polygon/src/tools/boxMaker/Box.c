/*############################################################################*/
/*####   Box.c: Function implementations of declarations made in Box.h    ####*/
/*##                                                                        ##*/

/*----------------------------------------------------------------------------*/
/* Includes.                                                                  */
/*----------------------------------------------------------------------------*/

#include "Box.h"

/*----------------------------------------------------------------------------*/
/* swapBoxes()                                                                */
/*----------------------------------------------------------------------------*/

 void
swapBoxes(
  BoxArray boxArray, /* To swap boxes in. */
  int      index1,   /* Index of the first swap box. */   
  int      index2    /* Index of the first swap box. */
) {
  Box* pSwapBox; /* Helping hand during swapping. */

    pSwapBox = boxArray[index1];
    boxArray[index1] = boxArray[index2];
    boxArray[index2] = pSwapBox;
} /* swapBoxes */

/*##                                                                        ##*/
/*####   Box.c:                                                           ####*/
/*############################################################################*/
