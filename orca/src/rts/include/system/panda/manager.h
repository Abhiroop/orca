#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "panda/panda.h"
#include "orca_types.h"

extern void man_start( void);

extern void man_end( void);

extern void man_init( manager_p man, double reads, double writes);

extern void man_clear( manager_p man);


extern void man_strategy( fragment_p obj, int replicated, int cpu);


/* to be used for shared objects  iso  f_lock()/f_unlock() 
 */
#define man_lock( obj)	f_lock( obj)

extern void man_unlock( fragment_p obj);


/* marshalling code 
 */
extern int man_nbytes( fragment_p obj);

extern char *man_marshall( char *buf, fragment_p obj);

extern char *man_unmarshall( char *buf, fragment_p obj);


/* piggy back info 
 */
extern void man_get_piggy_info( fragment_p obj, man_piggy_p info);

extern void man_pack_piggy_info( message_p msg, man_piggy_p info);

extern void man_unpack_piggy_info( message_p msg, fragment_p obj);

#define man_tick( obj, modified) \
	do \
	    if ( (obj)->fr_manager != NULL) { \
		(obj)->fr_manager->runtime.access_sum++; \
		if ( modified) { \
		    (obj)->fr_manager->runtime.write_sum++; \
	        } \
	    } \
	while (0)

extern void man_take_decision( fragment_p obj);
extern void man_await_migration( fragment_p obj);

#define man_op_finished( obj)	\
	do \
	    if ( (obj)->fr_manager != NULL) { \
		man_take_decision( obj); \
	    } \
	while (0)



/* process creation/termination 
 */
extern void man_inc( fragment_p obj, int src, int dst,
		    double reads, double writes);

extern void man_dec( fragment_p obj, int cpu, double reads, double writes);

extern void man_delete( fragment_p obj, double reads, double writes);

#endif
