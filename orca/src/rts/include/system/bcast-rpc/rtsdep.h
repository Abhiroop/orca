/* $Id: rtsdep.h,v 1.25 1999/08/16 13:58:46 ceriel Exp $ */

#include <amoeba.h>
#include <semaphore.h>

#define TRUE 1
#define FALSE 0

/* Waiting list implementation. */
#define WCHUNK	10
typedef struct waiting t_waiting;
struct waiting {
	int	w_cnt;			/* w_process slots in use */
	int	w_cpu[WCHUNK];		/* CPU of waiting process */
	void	*w_process[WCHUNK];	/* id of waiting process */
	t_waiting *w_next;
};

#ifdef OPTIMIZED
/* In the optimized version, we decide on compile-time and run-time information
 * if objects should be replicated or not. If they are not replicated, one
 * processor is the owner. Obj_state_t stores the necessary information to make
 * the decision. The information given through __Score and the process
 * descriptor on fork is used to update the state.
 */
typedef struct ObjectState {            /* state about the obj usage per cpu */
    int                 o_refcnt;       /* is object used? */
    int                 o_replica;      /* is a copy here? */
    int                 o_score;        /* score for cpu */
    int                 o_naccess;      /* # accesses on obj */
    int                 o_uncertainty;  /* uncertainty about the score */
} t_obj_state;
#endif

typedef struct t_objrts {
	int	oo_owner;		/* if not replicated: owner */
	t_waiting *oo_waitinglist;	/* list of suspended processes */
	int	oo_id;			/* unique object identification */
	mutex	oo_mutex;		/* to lock the object */
	char	oo_replicated;		/* is this object replicated? */
	char	oo_strategy_set;	/* set when Strategy is called */
	int	oo_nread;		/* # read operations */
	int	oo_nwrite;		/* # write operations */
	int	oo_nremote;		/* # remote operations (RPC) */
	int	oo_nreceived;		/* # received operations (RPC) */
	int	oo_nowner;		/* # local owner operations */
	int	oo_shared;
	int	oo_refcount;
	tp_dscr	*oo_dscr;
#ifdef OPTIMIZED
	semaphore oo_semdata;		/* sem used during migrating data */
	t_obj_state *oo_state;		/* per cpu keep info about object */
	struct t_objrts *oo_next;
#endif
} t_objrts;

#define o_owner		o_rtsdep->oo_owner
#define o_waitinglist	o_rtsdep->oo_waitinglist
#define o_id		o_rtsdep->oo_id
#define o_mutex		o_rtsdep->oo_mutex
#define o_replicated	o_rtsdep->oo_replicated
#define o_strategy_set	o_rtsdep->oo_strategy_set
#define o_nread		o_rtsdep->oo_nread
#define o_nwrite	o_rtsdep->oo_nwrite
#define o_nremote	o_rtsdep->oo_nremote
#define o_nreceived	o_rtsdep->oo_nreceived
#define o_nowner	o_rtsdep->oo_nowner
#define o_shared	o_rtsdep->oo_shared
#define o_refcount	o_rtsdep->oo_refcount
#define o_dscr		o_rtsdep->oo_dscr
#ifdef OPTIMIZED
#define o_semdata	o_rtsdep->oo_semdata
#define o_state		o_rtsdep->oo_state
#define o_next		o_rtsdep->oo_next
#endif

#define OBJ_READ_INC(o)         (o)->o_nread++          /* increment read */
#define OBJ_WRITE_INC(o)        (o)->o_nwrite++         /* increment write */
#define OBJ_REMOTE_INC(o)       (o)->o_nremote++        /* increment remote */
#define OBJ_RECEIVED_INC(o)     (o)->o_nreceived++      /* increment received */
#define OBJ_OWNER_INC(o)        (o)->o_nowner++         /* increment owner */

struct opstat {
    int oo_nlocal;              /* # local operations (nonshared object) */
    int oo_nread_trial;         /* # trials for read guards */
    int oo_nwrite_trial;        /* # trials for write guards */
    int oo_nread;               /* # read guards */
    int oo_nwrite;              /* # bcast guards */
    int oo_nblock;              /* # bcast operations that blocked */
    int r_nlocal;               /* # local RPC operations */
    int r_nremote;              /* # remote RPC operations */
};

#define BSTINC(f)		opstat.f++

extern int ncpus;
extern int this_cpu;

#ifdef PROGRAM_MUST_SWITCH

#define SCHEDINIT	32768

extern int schedcount;

#define m_rts()		if (--schedcount == 0) \
				schedcount = SCHEDINIT, threadswitch()

#else

#define m_rts()

#endif

#define __erocS(a,x,b,y,c)

#define m_ncpus()	(ncpus)

#define m_mycpu()	(this_cpu)

#define m_objdescr_reg(p, n, s)

#define m_flush(f)	fflush(f)

#define o_isshared(o)		((o)->o_shared != 0)
