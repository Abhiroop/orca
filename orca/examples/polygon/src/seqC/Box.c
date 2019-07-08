/*############################################################################*/
/*####   Box.c: Function implementations of declarations made in Box.h    ####*/
/*##                                                                        ##*/

/*----------------------------------------------------------------------------*/
/* Includes.                                                                  */
/*----------------------------------------------------------------------------*/

#include "Box.h"

/*----------------------------------------------------------------------------*/
/* addBoxToList()                                                             */
/*----------------------------------------------------------------------------*/

 void
addBoxToList(
  Box       box,     /* Item that is going be put in front of the list.*/
  BoxList*  boxList  /* List to add box to. */
){
  BoxListPt  bItem;  /* In order to create a new item for the list. */

    bItem = (BoxListPt)malloc(sizeof(BoxListItem));
    bItem->box = box;
    bItem->next = *boxList;
    *boxList = bItem;
} /* addBoxToList */

/*----------------------------------------------------------------------------*/
/* delFirstBoxFromList()                                                      */
/*----------------------------------------------------------------------------*/
 
 void
delFirstBoxFromList(
  BoxList*  boxList  /* List to delete the 1st entry from. */
){
  BoxList  p;    /* Helping pointer. */

    if (*boxList != NULL) {
	p = *boxList;
	*boxList = (*boxList)->next;
	free(p);
    } /* if */
} /* FirstBoxFromList */

/*----------------------------------------------------------------------------*/
/* delNextBoxFromList()                                                       */
/*----------------------------------------------------------------------------*/

void  delNextBoxFromList(
  BoxList* boxList,  /* List to swap first item to the back if nessesairy. */
  BoxList  boxP      /* Next is to be deleted from boxList. */
){
  BoxList  p;    /* Helping pointer. */

    if (*boxList != NULL) {
	if (boxP == NULL) {
	    p = *boxList;
	    *boxList = p->next;
	    free(p);
        } /* if */
	else {
	    p = boxP->next;
	    boxP->next = p->next;
	    free(p);
        } /* else */
    } /* if */
} /* BoxNextFromList */

/*----------------------------------------------------------------------------*/
/* listLength()                                                               */
/*----------------------------------------------------------------------------*/

 int
listLength(
        BoxList  p  /* The list to get the length of. */
) {
  int  length = 0;

    for(; p != NULL; p = p->next) {
        length++;
    } /* for */

    return( length );
} /* listLength */


/*##                                                                        ##*/
/*####   Box.c:                                                           ####*/
/*############################################################################*/
