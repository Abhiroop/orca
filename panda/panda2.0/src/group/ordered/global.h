#ifndef __GROUP_GLOBAL_H__
#define __GROUP_GLOBAL_H__

#include "pan_sys.h"


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif



/* This is a farce */
#ifdef HETEROGENEOUS
#ifndef ENDIANNESS
#define ENDIANNESS  BIG_ENDIAN   /* or LITTLE_ENDIAN */
#endif
#endif


#define NO_GROUP_ID  -1
typedef int group_id_t;


/* Receiver thread policy parameters */
#define RCVR_STACK_SIZE  10240
#define RCVR_PRIORITY    (pan_thread_maxprio())
#define RCVR_BUFFER_SIZE 128	/* for 'pan_rbuf_init, is only # of pointers! */

#define MAX_GROUPS  4    /* small to reduce memory needs (HPH) */


extern int		pan_grp_me;		/* Cache pan_my_pid() */
extern int		pan_grp_n_platforms;	/* Cache pan_nr_platforms */

extern pan_nsap_p	pan_grp_nsap;
extern pan_mutex_p	pan_grp_lock;		/* shared lock for this module*/

void       pan_grp_global_start(void);
void       pan_grp_global_end(void);

#endif
