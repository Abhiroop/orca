#ifndef _SYS_AMOEBA_GLOBAL_
#define _SYS_AMOEBA_GLOBAL_

#include <stdlib.h>
#include <assert.h>


#ifndef INLINE
#  ifdef __GNUC__
#    define INLINE __inline__
#  else
#    define INLINE
#  endif
#endif

#define MIN(x,y)	((x) < (y) ? (x) : (y))
#define MAX(x,y)	((x) > (y) ? (x) : (y))


#endif
