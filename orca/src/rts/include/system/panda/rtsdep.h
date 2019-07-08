#ifndef __RTSDEP_H__
#define __RTSDEP_H__

#include "rts_types.h"
#include "process.h"
#include "fragment.h"
#include "trace/trace.h"


/* An object fragment's RTS-dependent part consists of the object's
 * type and an RTS object. 
 */
struct t_objrts {
    tp_dscr      *type;  /* object's type descriptor */
    rts_object_t  obj;   /* RTS part of object */
};


extern int rts_base_pid;

#define m_ncpus()	(sys_nr_platforms - (rts_base_pid - 1))
#define m_mycpu()	(sys_my_pid - rts_base_pid)
#define m_rts()

#define m_flush(f)	fflush(f)

void	m_strategy(t_object *o, int replicated, int owner);

#define o_start_read(o)          f_start_read( (o) )
#define o_end_read(o)            f_end_read( (o) )
#define o_start_write(o)         f_start_write( (o) )
#define o_end_write(o, res)      f_end_write( (o) )
#define o_setblocking()          p_set_blocking( p_self() )
#define o_getblocking()          ( (t_boolean) p_get_blocking( p_self() ) )

#define o_doing_operation()      p_doing_operation( p_self() )

#define o_isshared(o)		 ( f_get_status( o) != f_unshared)

extern void __Score(void *data, tp_dscr *d, double score, double naccess, double uncertainty);
extern void __erocS(void *data, tp_dscr *d, double score, double naccess, double uncertainty);

extern int o_shared_nbytes(void *arg, tp_dscr *descr, int cpu);
extern char *o_shared_marshall(char *p, void *arg, tp_dscr *descr, int cpu);
extern char *o_shared_unmarshall(char *p, void **arg, tp_dscr *descr);

extern int o_rts_nbytes( t_object *op, tp_dscr *d, int flags);
extern char *o_rts_marshall( char *p, t_object *op, tp_dscr *d, int flags);
extern char *o_rts_unmarshall( char *p, t_object *op, tp_dscr *d, int flags);

#endif
