/*############################################################################*/
/*####   Box.h:  types "Box", "BoxList" + functions on these types.       ####*/
/*##                                                                        ##*/

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
/* Type "Box" declarations, macros and functions.                             */
/*----------------------------------------------------------------------------*/

typedef  struct {
	    int  x;
 	    int  y;
	}  Coordinate;
 
typedef struct {
	    Coordinate  corner0;    /* The two corners which         */
	    Coordinate  corner2;    /*  declare the whole rectangle. */
	    long int    surface;    /* Remaining surface to be overlayed. */
	}  Box;

/*  Functional macros.  */

/*
void  calcBoxSurface( boxP* Box )
*/
    /*                                                                        */
    /* Calculates the surface of a box and puts it in the field box.surface.  */
    /*                                                                        */
#define  calcBoxSurface( boxP ) \
		(boxP)->surface = \
		    ((boxP)->corner2.x - (boxP)->corner0.x) *  \
		    ((boxP)->corner2.y - (boxP)->corner0.y)

/*
int  noMoreBoxSurface( box Box )
*/
    /*                                                                        */
    /* returns non-zero (true) if the surface of box is 0 or less. and zero   */
    /* (false) if not.                                                        */
    /*                                                                        */
#define  noMoreBoxSurface( box ) \
		((box).surface <= 0)

/*
void  substractFromBoxSurface( theBoxP *Box,  minusBox Box ) \
*/
    /*                                                                        */
    /* Substracts the surface of minusBox from the surface of theBox.         */
    /*                                                                        */
#define  substractFromBoxSurface( theBoxP, minusBox ) \
		(theBoxP)->surface -= (minusBox).surface

/*----------------------------------------------------------------------------*/
/* Type "BoxList" declarations, macros and functions.                         */
/*----------------------------------------------------------------------------*/

typedef struct BoxListStruct* BoxListPt;
 
typedef struct BoxListStruct {
	    Box        box;
	    BoxListPt  next;
	}  BoxListItem;

typedef BoxListPt  BoxList;
 

/*  Functional macros.  */

/* 
void  initBoxList( BoxList* boxList )
*/
    /*                                                                        */
    /* Should be called before using any of the other functions on a boxList. */
    /*                                                                        */
#define  initBoxList( boxList ) \
		*(boxList) = NULL

/*
int  isEmptyBoxList( BoxList boxList )
*/
    /*                                                                        */
    /* Returns non-zero (true) if boxList contains no element, and zero       */
    /* (false) if not.                                                        */
    /*                                                                        */
#define  isEmptyBoxList( boxList ) \
		((boxList) == NULL)


/*  Function prototypes.  */

void  addBoxToList( Box box,  BoxList* boxlist);
    /*                                                                        */
    /* Adds box to boxList as the first item in the list. The box is copied.  */

void  delFirstBoxFromList( BoxList* boxList );
    /*                                                                        */
    /* Deletes the first item from the list "boxList".                        */

void  delNextBoxFromList( BoxList* boxList,  BoxList boxP );
    /*                                                                        */
    /* Deletes the item pointed by by boxP->next. If boxP is NULL then the    */
    /* head is deleted from the list.                                         */

int  listLength( BoxList boxList );
        /*                                                                    */
        /* Returns the length of the boxList.                                 */

#endif

/*##                                                                        ##*/
/*####   Box.h                                                            ####*/
/*############################################################################*/
