/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __MANAGER_H__
#define __MANAGER_H__

#undef DECL
#if defined(MANAGER_SRC) || !defined(INLINE_FUNCTIONS)
#define DECL    extern
#else
#define DECL    static
#endif

#include "pan_sys.h"
#include "orca_types.h"

#define BIG  (1<<25)      /* constant to force object placement */

extern void man_start( void);

extern void man_end( void);

extern void man_init( manager_p man, float reads, float writes);

extern void man_clear( manager_p man);


extern void man_strategy( fragment_p obj, int replicated, int cpu);

extern void man_kill_object(fragment_p obj);

#define rts_man_check(obj) \
    do { \
        if (obj->fr_total_refs == 0) man_kill_object(obj); \
    } while(0);

/* 
 * Marshalling code 
 */
extern int man_nbytes( fragment_p obj);

extern char *man_marshall( char *buf, fragment_p obj);

extern void man_unmarshall( pan_msg_p p, fragment_p obj);


/*
 * piggy back info 
 */

extern void man_take_decision( fragment_p obj);

DECL void man_get_piggy_info( fragment_p obj, man_piggy_p info);

DECL void man_tick( fragment_p obj, int modified);

DECL void man_op_finished( fragment_p obj);

DECL void man_process_piggy_info( man_piggy_p info, fragment_p obj,
				  int src_cpu);



/* process creation/termination 
 */
extern void man_inc(fragment_p obj, int src, int dst,
		    float reads, float writes);

extern void man_dec( fragment_p obj, int cpu, float reads, float writes);

extern void man_delete( fragment_p obj, float reads, float writes, int nr);

#if !defined(MANAGER_SRC) && defined(INLINE_FUNCTIONS)
#define MANAGER_INLINE
#include "src/manager.c"
#endif

#endif
