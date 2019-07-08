#ifndef __RTS_TYPES_H__
#define __RTS_TYPES_H__

#include "panda/panda.h"
#include "orca_types.h"


/* RTS types */

typedef t_object fragment_t, *fragment_p;

typedef struct process process_t, *process_p;

typedef struct rts_object rts_object_t, *rts_object_p;  /* not opaque */

typedef void (*thrpool_func_p)(void *job);

typedef struct tp_job tp_job_t, *tp_job_p;

typedef struct thrpool thrpool_t, *thrpool_p;


/* RTS object flags bits */
#define RO_SHARED     0x01   /* set iff object is shared */
#define RO_REPLICATED 0x02   /* set iff object is replicated */
#define RO_MANAGER    0x04   /* set iff manager located with this replica */

#define RO_MASK       0x07   /* extracts all flags */

/* Fragment status is determined by the following bits in the
 * fragment's flags field: RO_SHARED, RO_REPLICATED, and RO_MANAGER.
 * There are 6 legal combinations:
 */
#define F_MAX_STATUS 8  /* 3 bits can accommodate 8 values */

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
typedef struct man_piggy man_piggy_t, *man_piggy_p;

typedef struct account_t account_t, *account_p;

typedef enum { AC_READ, AC_WRITE, AC_SIZE} access_t;
typedef enum { AC_REMOTE, AC_LOCAL, AC_LOCAL_FAST} source_t;

struct account_t {
    int	proc_loc[AC_SIZE];   /* reads/writes isssued by local processes */
    int	proc_rpc[AC_SIZE];   /* outgoing RPCs of local processes */
    int	 proc_bc[AC_SIZE];   /* broadcast operations of local processes */
    int	 RTS_rpc[AC_SIZE];   /* incoming RPC operations */
    int	  RTS_bc[AC_SIZE];   /* total number of broadcast operations */
};

typedef struct continuation cont_t, *cont_p;
typedef struct cont_queue cont_queue_t, *cont_queue_p;
typedef int (*cont_func_p)(void *state, mutex_t *lock);

typedef enum i_status {
    I_BLOCKED,      /* operation blocked */
    I_COMPLETED,    /* operation completed successfully */
    I_FAILED        /* target obj. moved or became (non)replicated */
} i_status_t, *i_status_p;

typedef struct invocation invocation_t, *invocation_p;

typedef struct otab_entry otab_entry_t, *otab_entry_p;
typedef struct obj_id oid_t, *oid_p;

typedef void (*operation_p)(int op, pan_upcall_t upcall, message_p request);

typedef struct { short op; short cpu; rts_object_p obj;} op_inv_info;
typedef struct { short op; short cpu; rts_object_p obj; void *state;} op_cont_info;

typedef struct op_hdr op_hdr_t;
typedef struct op_ret op_ret_t;

struct obj_id {
	short        g_offset;		/* uniq global id (offset in otab) */
	short        cpu;		/* source of object creation */
	rts_object_p rts;		/* 'local' pointer (+ cpu => uniqe) */
};

/* manager flag bits in object */
#define MAN_FUZZY_STATE 0x10            /* object in transcient state */
#define MAN_LOCAL_ONLY  0x20            /* is object known at other CPUs? */
#define MAN_VALID_FIELD 0x40            /* No garbage in data field? */

#define MAN_FLAGS	(MAN_FUZZY_STATE|MAN_LOCAL_ONLY|MAN_VALID_FIELD)


struct manager {
    struct {
        double write_sum;    /* sum of write scores */
        double access_sum;   /* sum of access scores */
        double max_access;   /* maximum access value over all platforms */
        int max_location;    /* platform having max access value */
    } compiler, runtime;     /* maintain compiler and runtime info separately */
};

struct piggy_info {
    double access_sum;     /* sum of compile-time access scores */
    int nr_accesses;       /* #reads + #writes up until now */
    int delta_reads;       /* #local reads since last external operation */
};

struct man_piggy {
    int cpu;               /* source identification */
    piggy_t p_info;        /* the actual info */
};

#define CONT_SIZE        64                  /* max state size in bytes */

struct continuation {
    char          c_buf[CONT_SIZE];          /* typeless cont buffer  */
                                             /* MUST BE FIRST IN STRUCT! */
    cont_queue_t *c_queue;                   /* back pointer to queue */
    int         (*c_func)(void *state, mutex_t *lock);  
                                             /* continuation function */
    int		  c_thread;		     /* separate thread?      */
    cond_t        c_cond;                    /* when using threads    */
    struct continuation *c_next;             /* next saved cont       */
};


struct cont_queue {
    mutex_t             *cq_lock;            /* queue lock                 */
    struct continuation *cq_freelist;        /* free list of continuations */
    struct continuation *cq_contlist;        /* pending continuations      */
    int                  cq_allocated;       /* #not on free list */
};


struct rts_object {
    oid_t        oid;        /* object id; unique over all platforms */
    fragment_p	 frag;	     /* link back to the complete fragment */
    int          flags;      /* flag bits */
    mutex_t      lock;
    cont_queue_t wlist;      /* list of blocked operation invocations */
    piggy_t      info;       /* piggy_back info maintained for object manag. */
#ifdef ACCOUNTING
    account_t    account;    /* statistics per object */
#endif
    int          total_refs; /* global reference count */
    int          owner;      /* owning platform when object is not replicated */
    manager_p    manager;    /* manager state */
    char        *name;	     /* Orca name of the object */
};

struct invocation {
    fragment_p     frag;      /* local fragment under invocation */
    f_status_t     f_status;  /* fragment status at suspend time */
    tp_dscr       *obj_type;  /* type descriptor of object addressed */
    int            opindex;   /* operation being invoked */
    pan_upcall_t   upcall;    /* RPC upcall id */
    void         **argv;      /* invocation arguments */

    cond_t         resumed;   /* used for blocking and waking invoker */ 
    i_status_t     result;
};


struct process {
    thread_t            tid;        /* Panda thread id                 */
    struct proc_descr  *pdscr;      /* Orca process descriptor         */
    void              **argv;       /* argument vector                 */
    int                 depth;      /* operation nesting               */
    int                 blocking;   /* set if an operation would block */
};


struct otab_entry {
    oid_t        oid;     /* global object id          */
    fragment_t   frag;    /* fragment descriptor       */
};


/* This header is prepended to all operation messages.
 */
struct op_hdr {
    int          oh_src;     /* invoker's platform id */
    oid_t        oh_oid;     /* object id of object being operated on */
    invocation_p oh_inv;     /* invoker's operation record; only for mcasts */
    int          oh_argsize; /* argument size */
    int          oh_opindex; /* operation index */
    int          oh_tdreg;   /* type descriptor handle */
};


/* This is prepended to all operation reply messages.
 */
struct op_ret {
    int ok;         /* Did the operation succeed? */
    int retsize;    /* size of returned values    */
};


#define TP_MAXJOBSIZE                12

struct tp_job {
    char           tpj_buf[TP_MAXJOBSIZE];
    struct tp_job *tpj_next;
};

struct thrpool {
    mutex_t           tp_lock;        /* pool lock, shared by workers */
    cond_t            tp_cond;        /* signal change of full/empty status */
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
    thread_p          tp_id;          /* holds thread ids for termination */
    thrpool_func_p    tp_upcall;      /* upcall function used by workers */
};

#endif
