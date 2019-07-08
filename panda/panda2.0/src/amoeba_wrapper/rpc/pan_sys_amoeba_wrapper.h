#ifndef _PAN_SYS_AMOEBA_WRAPPER_
#define _PAN_SYS_AMOEBA_WRAPPER_

/* This file includes extensions to the standard system interface */
#include "pan_sys.h"


#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

/***** Message interface *****/

#define UNIVERSAL_ALIGNMENT	   8
#define aligned(p, align)	   ((((long)(p)) & ((align) - 1)) == 0)
#define do_align(p, align)   	   (((p) + (align) - 1) & ~((align) - 1))
#define SYS_DEFAULT_MESSAGE_SIZE   do_align(8*1024, UNIVERSAL_ALIGNMENT)

extern int pan_msg_data_len(pan_msg_p message);
extern void pan_msg_truncate(pan_msg_p message, int length);


/***** Error handling *****/

#include "pan_error.h"

#define sys_panic		pan_panic


/***** Memory management *****/

static void *sys_malloc(int size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL) {
		printf( "malloc(%d) failed\n", size);
		exit(55);
	}
	return ptr;
}


static void *sys_realloc(void *old, int size)
{
	void *ptr;

	if ((ptr = realloc(old, size)) == NULL) {
		Printf( "realloc(0x%x,%d) failed\n", old, size);
		exit(55);
	}
	return ptr;
}


/***** Synchronization (inlining) *****/

#include "pan_sync.h"

#endif
