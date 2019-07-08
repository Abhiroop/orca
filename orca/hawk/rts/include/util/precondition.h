/*#####################################################################*/
/* Saniya Ben Hassen. September 1995. */
/*#####################################################################*/

#ifndef __precondition__
#define __precondition__

#ifdef PRECONDITION_ON

#include "assert.h"

#define precondition(c) assert(c)
#define precondition_p(c) assert(c)

#else

#define precondition(c) 
#define precondition_p(c) 

#endif

#endif
