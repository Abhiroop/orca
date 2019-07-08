/* $Id: rtsdep.h,v 1.17 1999/08/16 13:58:48 ceriel Exp $ */

/* Version for single process system (no forks allowed) */

struct t_objrts {
	int			oo_refcount;
};

#define o_refcount		o_rtsdep->oo_refcount

#define m_rts()		

#define m_flush(f)		fflush(f)

#define o_init_rtsdep(o, d, s) \
			((o)->o_rtsdep = m_malloc(sizeof(struct t_objrts)), \
			 (o)->o_refcount = 1)

#define o_kill_rtsdep(o) \
			m_free((o)->o_rtsdep)

#define o_isshared(o)	(0)

#define m_ncpus()	(1)
#define m_mycpu()	(0)

#define m_objdescr_reg(p, n, s)

#define m_strategy(a, b, c)

#define __Score(a,x,b,c,d)
#define __erocS(a,x,b,c,d)

#define o_start_read(o)		(1)
#define o_start_write(o)	(1)
#define o_end_read(o)		
#define o_end_write(o,x)	

/* No marshalling. */
#define o_rts_nbytes(p, d) 0
#define o_rts_marshall(p, data, d) \
                              p
#define o_rts_unmarshall(p, data, d) \
                              p
