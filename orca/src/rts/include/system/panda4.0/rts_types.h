/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_TYPES_H__
#define __RTS_TYPES_H__

#include "pan_sys.h"
#include "pan_rpc.h"
#include "orca_types.h"

extern int rts_my_pid;
extern int rts_nr_platforms;


/* RTS types */

typedef t_object fragment_t, *fragment_p;

typedef struct process process_t, *process_p;

typedef struct rts_object rts_object_t, *rts_object_p;  /* not opaque */

typedef void (*thrpool_func_p)(void *job);

typedef struct tp_job tp_job_t, *tp_job_p;

typedef struct thrpool thrpool_t, *thrpool_p;

typedef struct shared_id shared_id_t, *shared_id_p;

/* RTS object flags bits */
#define RO_SHARED     0x01   /* set iff object is shared */
#define RO_REPLICATED 0x02   /* set iff object is replicated */
#define RO_MANAGER    0x04   /* set iff manager located with this replica */

#define RO_MASK       0x07   /* extracts all flags */

/* Fragment status is determined by the following bits in the
 * fragment's flags field: RO_SHARED, RO_REPLICATED, and RO_MANAGER.
 * There are 6 legal combinations:
 */
#define F_UNSHARED   ( (~RO_SHARED & ~RO_REPLICATED  & ~RO_MANAGER) & RO_MASK )
#define F_REPLICATED ( ((RO_SHARED |  RO_REPLICATED) & ~RO_MANAGER) & RO_MASK )
#define F_MANAGER    ( ( RO_SHARED |  RO_REPLICATED  |  RO_MANAGER) & RO_MASK )
#define F_REMOTE     ( ( RO_SHARED & ~RO_REPLICATED  & ~RO_MANAGER) & RO_MASK )
#define F_OWNER      ( ((RO_SHARED & ~RO_REPLICATED) |  RO_MANAGER) & RO_MASK )
#define F_IN_TRANSIT ( (~RO_SHARED & (RO_REPLICATED  | RO_MANAGER)) & RO_MASK )
#define F_TEMP_ILL   ( ((~RO_SHARED & ~RO_REPLICATED) | RO_MANAGER) & RO_MASK )
#define F_ILLEGAL    ( -1 )

typedef enum f_status {
    f_illegal    = F_ILLEGAL,      /* no fragment should have this status */
    f_unshared   = F_UNSHARED,     /* used by only one local process */
    f_replicated = F_REPLICATED,   /* replicated with remote manager */
    f_manager    = F_MANAGER,      /* replicated with local manager */
    f_remote     = F_REMOTE,       /* single copy at remote location */
    f_owner      = F_OWNER,        /* single copy at this platform */
    f_in_transit = F_IN_TRANSIT	   /* temporary status when object migrates */
} f_status_t, *f_status_p;

typedef struct manager manager_t, *manager_p;
typedef struct piggy_info piggy_t, *piggy_p;
typedef struct piggy_info man_piggy_t, *man_piggy_p;

typedef struct account_t account_t, *account_p;

typedef enum { AC_READ, AC_WRITE, AC_SIZE} access_t;
typedef enum { AC_REMOTE, AC_LOCAL, AC_LOCAL_FAST,
	       AC_REMOTE_BC, AC_LOCAL_BC} source_t;

struct account_t {
    int	proc_loc[AC_SIZE];   /* reads/writes isssued by local processes */
    int	proc_rpc[AC_SIZE];   /* outgoing RPCs of local processes */
    int	 proc_bc[AC_SIZE];   /* broadcast operations of local processes */
    int	 RTS_rpc[AC_SIZE];   /* incoming RPC operations */
    int	  RTS_bc[AC_SIZE];   /* total number of broadcast operations */
};

typedef struct continuation cont_t, *cont_p;
typedef struct cont_queue cont_queue_t, *cont_queue_p;
typedef int (*cont_func_p)(void *state);

typedef enum i_status {
    I_BLOCKED,      /* operation blocked */
    I_COMPLETED,    /* operation completed successfully */
    I_FAILED,       /* target obj. moved or became (non)replicated */
    I_DO_IT_YOURSELF/* local thread's continuation should eval the operation */
} i_status_t, *i_status_p;

typedef struct invocation invocation_t, *invocation_p;
typedef struct {short op; short cpu; fragment_p obj; void *state;} op_cont_info;

typedef int (*oper_p)(int op, int upcall, pan_msg_p msg, void *proto);

typedef struct { short op; short cpu; fragment_p obj;} op_inv_info;

typedef struct op_hdr op_hdr_t;
typedef struct op_ret op_ret_t;

/* manager flag bits in object */
#define MAN_FUZZY_STATE 0x10            /* object in transient state */
#define MAN_LOCAL_ONLY  0x20            /* is object known at other CPUs? */
#define MAN_VALID_FIELD 0x40            /* No garbage in data field? */

#define MAN_FLAGS	(MAN_FUZZY_STATE|MAN_LOCAL_ONLY|MAN_VALID_FIELD)

/* Koen: use floats iso doubles to save on space,
 *	 use floats iso ints to save on conversion costs in manager code
 */

struct manager {
    int fixed_strategy;	     /* needed for strategy call */
    struct {
        int write_sum;       /* sum of write scores */
        int access_sum;      /* sum of access scores */
        int max_access;      /* maximum access value over all platforms */
        int max_location;    /* platform having max access value */
	float cache_bcast;   /* costs if object is replicated */
	float cache_rpc;     /* costs if object is stored at 'max_location' */
    } compiler;
    struct {
        int write_sum;       /* sum of write scores */
        int access_sum;      /* sum of access scores */
        int max_access;      /* maximum access value over all platforms */
        int max_location;    /* platform having max access value */
	int delay;	     /* to delute the number of decisions */
	float hist_bcast;    /* weighted average bcast costs */
	float prev_bcast;    /* previous hist_bcast, so we can compute delta */
	float hist_rpc;      /* weighted average rpc costs */
	float prev_rpc;      /* previous hist_rpc */
    } runtime;
};

struct piggy_info {
    int access_sum;          /* sum of compile-time access scores */
    int nr_accesses;         /* #reads + #writes up until now */
    int delta_reads;         /* #local reads since last external operation */
};


#define CONT_SIZE        64                  /* max state size in bytes */

struct continuation {
    char          c_buf[CONT_SIZE];          /* typeless cont buffer  */
                                             /* MUST BE FIRST IN STRUCT! */
    cont_queue_t *c_queue;                   /* back pointer to queue */
    int         (*c_func)(void *state);      /* continuation function */
    int		  c_thread;		     /* separate thread?      */
    pan_cond_p    c_cond;                    /* when using threads    */
    struct continuation *c_next;             /* next saved cont       */
};


struct cont_queue {
    struct continuation *cq_freelist;        /* free list of continuations */
    struct continuation *cq_contlist;        /* pending continuations      */
    int                  cq_allocated;       /* #not on free list */
};


struct shared_id {
    int        si_oid;       /* global object id */
    fragment_p si_object;    /* local object pointer */
};

struct rts_object {
    int          oid;	     /* uniq global id (offset in otab) */
    fragment_p	 frag;	     /* link back to the complete fragment */
    int          flags;      /* flag bits */
    cont_queue_t wlist;      /* list of blocked operation invocations */
    piggy_t      info;       /* piggy_back info maintained for object manag. */
    account_t    account;    /* statistics per object */
    int          total_refs; /* global reference count */
    int          owner;      /* owning platform when object is not replicated */
    manager_p    manager;    /* manager state */
    char        *name;	     /* Orca name of the object */
#ifdef TRACING
    short        home;	     /* source of object creation */
    fragment_p   home_obj;   /* link to this struct (at home only!) */
#endif
};

struct invocation {
    fragment_p     frag;      /* local fragment under invocation */
    f_status_t     f_status;  /* fragment status at suspend time */
    op_dscr        *op;       /* operation being invoked */
    int		   upcall;    /* RPC upcall id */
    void         **argv;      /* invocation arguments */
    process_p      self;      /* pointer to RTS process descriptor */
    int		  *op_flags;
    pan_cond_p     resumed;   /* used for blocking and waking invoker */ 
    i_status_t     result;
};


struct process {
    pan_thread_p        tid;        /* Panda thread id                 */
    struct proc_descr  *pdscr;      /* Orca process descriptor         */
    void              **argv;       /* argument vector                 */
    int                 depth;      /* operation nesting               */
    int                 blocking;   /* set if an operation would block */
#ifdef PURE_WRITE
    int                 p_pipelined;/* # successfully pipelined writes */
    int                 p_nwrites;  /* # pending pure writes */
    fragment_p          p_object;   /* pipelining operations on this obj. */
    pan_cond_p          p_flushed;  /* all pure writes flushed */
#endif
};


/* This header is prepended to all operation messages.
 */
struct op_hdr {
    int          oh_src;     /* invoker's platform id */
    int          oh_oid;     /* object id of object being operated on */
    void        *oh_ptr;     /* operation record or RTS process structure */
    int          oh_opindex; /* operation index */
};


/* This is prepended to all operation reply messages.
 */
struct op_ret {
    int ok;         /* Did the operation succeed? */
};


#define TP_MAXJOBSIZE                12

struct tp_job {
    char           tpj_buf[TP_MAXJOBSIZE];
    struct tp_job *tpj_next;
};

struct thrpool {
    pan_mutex_p       tp_lock;        /* pool lock, shared by workers */
    pan_cond_p        tp_cond;        /* signal change of full/empty status */
    int               tp_done;        /* for termination */
    int               tp_priority;    /* priority of workers */
    int               tp_maxsize;     /* max. no workers in thread pool */
    int               tp_workers;     /* current no. of workers */
    int               tp_idle;        /* current no. of idle workers */
    int               tp_increment;   /* #workers to add when pool empty */
    int               tp_jobsize;     /* size of a job */
    struct tp_job    *tp_freelist;    /* free list of job structures */
    struct tp_job    *tp_head;        /* job list: head pointer */
    struct tp_job    *tp_tail;        /* job list: tail pointer */
    pan_thread_p     *tp_id;          /* holds thread ids for termination */
    thrpool_func_p    tp_upcall;      /* upcall function used by workers */
};

#endif
