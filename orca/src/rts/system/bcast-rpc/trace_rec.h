/* $Id: trace_rec.h,v 1.3 1994/03/11 10:52:30 ceriel Exp $ */

extern int trace_on;		/* tracing on/off */
extern int trace_event[];	/* array of NTRACE_EVENT switches */
extern long time_clock_diff;

#define FORK_RECORD	1
#define OP_RECORD	2
#define SUSPEND_RECORD	3
#define OPEREXIT_RECORD	4	/* WARNING: 4 must < NTRACE_EVENT! */
#define FLUSH_RECORD	5	/* also generate event when buffer is flushed */


/* values for o_kind and s_opkind */
#define BROADCAST_OPER	0
#define RPC_OPER	1
#define REMOTE_OPER	2
#define UPDATE_OPER	3

typedef struct fork_rec {
	int			f_target_cpu;
	void *			f_func;
} fork_rec_t, *fork_rec_p;

typedef struct op_rec {
	void			*o_descr;
	t_object		*o_obj;
	int			o_objid;
	int			o_procid;
	int			o_kind;
} op_rec_t, *op_rec_p;

typedef struct suspend_rec {
	void			*s_descr;
	t_object		*s_obj;
	int			s_objid;
	int			s_procid;
	int			s_opkind;
} suspend_rec_t, *suspend_rec_p;

typedef struct operexit_rec {
	void			*e_descr;
	t_object		*e_obj;
	int			e_objid;
	int			e_procid;
	int			e_opkind;
	int			e_when;	/* 0 or 1 */
} operexit_rec_t, *operexit_rec_p;

typedef struct flush_rec {
	int	f_start_flush;	/* time when flushing started */
} flush_rec_t, *flush_rec_p;


typedef struct trace_rec {
	int	t_rec_type;
	int	t_time_stamp;
	int	t_cpu;
	union	t_rec {
		fork_rec_t	fork_rec;
		op_rec_t	op_rec;
		suspend_rec_t	suspend_rec;
		operexit_rec_t	operexit_rec;
		flush_rec_t	flush_rec;
	} 	t_rec;
} trace_rec_t, *trace_rec_p;

#define t_target_cpu	t_rec.fork_rec.f_target_cpu
#define t_func		t_rec.fork_rec.f_func

#define t_descr		t_rec.op_rec.o_descr
#define t_obj		t_rec.op_rec.o_obj
#define t_objid		t_rec.op_rec.o_objid
#define t_procid	t_rec.op_rec.o_procid
#define t_kind		t_rec.op_rec.o_kind

#define t_block_oper	t_rec.suspend_rec.s_descr
#define t_block_obj	t_rec.suspend_rec.s_obj
#define t_block_objid	t_rec.suspend_rec.s_objid
#define t_block_procid	t_rec.suspend_rec.s_procid
#define t_block_opkind	t_rec.suspend_rec.s_opkind

#define t_operexit_oper		t_rec.operexit_rec.e_descr
#define t_operexit_obj		t_rec.operexit_rec.e_obj
#define t_operexit_objid	t_rec.operexit_rec.e_objid
#define t_operexit_procid	t_rec.operexit_rec.e_procid
#define t_operexit_opkind	t_rec.operexit_rec.e_opkind
#define t_operexit_when		t_rec.operexit_rec.e_when

#define t_start_flush		t_rec.flush_rec.f_start_flush


#define put_rec(typ, rec, p) \
	 {(rec)->t_rec_type = typ; \
	 (rec)->t_time_stamp = sys_milli() + time_clock_diff; \
	 (rec)->t_cpu = p;}

#define put_fork_rec(rec, cpu, target, f) \
	{if (trace_on && trace_event[FORK_RECORD]) { \
	 put_rec(FORK_RECORD, (rec), cpu); \
	 (rec)->t_target_cpu = target; \
	 (rec)->t_func = f; \
	 trace_put((rec), sizeof(trace_rec_t));} }

#define put_op_rec(rec, cpu, desc, obj, objid, p, kind) \
	{if (trace_on && trace_event[OP_RECORD]) { \
	 put_rec(OP_RECORD, (rec), cpu); \
	 (rec)->t_descr = desc; \
	 (rec)->t_obj = obj; \
	 (rec)->t_objid = objid; \
	 (rec)->t_procid = p; \
	 (rec)->t_kind = kind; \
	 trace_put((rec), sizeof(trace_rec_t));} }

#define put_suspend_rec(rec, cpu, desc, obj, objid, p, kind) \
	{if (trace_on && trace_event[SUSPEND_RECORD]) { \
	 put_rec(SUSPEND_RECORD, (rec), cpu); \
	 (rec)->t_block_oper = desc; \
	 (rec)->t_block_obj = obj; \
	 (rec)->t_block_objid = objid; \
	 (rec)->t_block_procid = p; \
	 (rec)->t_block_opkind = kind; \
	 trace_put((rec), sizeof(trace_rec_t));} }

#define put_operexit_rec(rec, cpu, desc, obj, objid, p, kind, when) \
	{if (trace_on && trace_event[OPEREXIT_RECORD]) { \
	 put_rec(OPEREXIT_RECORD, (rec), cpu); \
	 (rec)->t_operexit_oper = desc; \
	 (rec)->t_operexit_obj = obj; \
	 (rec)->t_operexit_objid = objid; \
	 (rec)->t_operexit_procid = p; \
	 (rec)->t_operexit_opkind = kind; \
	 (rec)->t_operexit_when = when; \
	 trace_put((rec), sizeof(trace_rec_t));} }

#define put_flush_rec(rec, cpu, t) \
	{if (trace_on && trace_event[FLUSH_RECORD]) { \
	 put_rec(FLUSH_RECORD, (rec), cpu); \
	 (rec)->t_start_flush = t; \
	 unsafe_trace_put((rec), sizeof(trace_rec_t));} }

