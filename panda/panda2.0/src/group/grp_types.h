/*
 * (c) copyright 1995 by	the	Vrije	Universiteit, Amsterdam, The	Netherlands.
 * For	full	copyright	and	restrictions	on	use	see	the	file	COPYRIGHT	in	the
 * top	level	of	the	Panda	distribution.
 */

/* Basic macros and types
 */

#ifndef _GROUP_GRP_BASICS_H_
#define _GROUP_GRP_BASICS_H_

#include "pan_sys.h"



#ifdef __GNUC__
#  define	INLINE __inline__
#else
#  define	INLINE
#endif


#if (defined	TRUE) || (defined	FALSE)
#  ifndef	FALSE
#    define	FALSE 0
#  endif
#  ifndef	TRUE
#    define	TRUE 1
#  endif

typedef	int	boolean;

#else

typedef	enum {FALSE = 0, TRUE = 1} boolean;

#endif



#ifndef	alignof
#  include <stddef.h>
#  define alignof(tp)		offsetof(struct{char __a; tp __tp;}, __tp)
#endif


typedef	enum	BYTE_ORDER_T {
    BIG_ENDIAN,
    LITTLE_ENDAN
} byte_order_t;




typedef	struct	HIST_T	hist_t, *hist_p;

struct	HIST_T {
    pan_fragment_p     *buf;
    int			size;
    int			last;
    int			next;
};



typedef	struct	NUM_BUF_T	num_buf_t, *num_buf_p;

struct	NUM_BUF_T {
    pan_fragment_p     *buf;
    int			size;
    int			last;
    int			next;
};



			/* These export opaque types */
#include "grp_bb.h"
#include "grp_frag.h"
#include "grp_memlist.h"




			/* Group structure: */
struct	PAN_GROUP_T {

				/* I. Shared between all threads */
        int		seq;			/* Sequencer platform */
	char	       *name;
        int		gid;			/* Number of this group */
	pan_cond_p	group_cleared;
        pan_cond_p	group_size_changed;	/* for await_size */
					/* The various flags */
	int		is_rexmitting;		/* orderer */
	int		is_joining;		/* orderer */
	int		is_in_suicide;		/* orderer */
	int		is_seq_alive;		/* orderer */
	int		is_syncing;		/* sequencer */
	int		is_done;		/* orderer, sequencer */
	int		is_cleared;		/* sender, upcall */
	int		is_want_to_leave;		/* sender */
	int		is_ordr_done;		/* orderer */
	int		is_ordr_busy;		/* orderer */

				/* II. For orderer only */
#ifndef NO_ORDERER_THREAD
	pan_thread_p	orderer;
	pan_cond_p	ordr_work;		/* signal ordr timeout daemon */
#endif
	void	      (*receive)(pan_msg_p msg);/* upcall function */
	int		next_accept;		/* cache ord_buf->next */
	int		n_members;		/* total #members */
        int		global_seqno;		/* Member: sequencer's latest
						 * accepted	seqno */
	int		last_rexmit_request;	/* last rexmit request seqno */
        int		last_status;		/* last ack-ed seqno */
	int		my_join;		/* "my" join seqno */
					/* Message buffers */
        num_buf_t	ord_buf;		/* ordered msgs for this grp */
        bb_buf_p	bb_buf;			/* bb buffer, host X id */
        frag_buf_p	frag_buf;		/* fragment buffer, host X id */

				/* III. For sender threads */
        unsigned short	messid;			/* "my" next message */
	int		outstanding_frags;	/* Cannot leave if any outst */
	pan_cond_p	no_outstanding_frags;	/* CV that no more outst */
	int		waiting_senders;	/* For FIFO group snd/rcve */
	pan_cond_p	send_is_free;		/* For FIFO group snd/rcve */
	boolean		outstanding_msg;	/* For FIFO group snd/rcve */

				/* IV. For sequencer only */
	int		n_members_seq;		/* total #members */
        pan_pset_p	members;		/* member set for admin */
        mem_list_t	member_info;		/* Seq. records member status */
	boolean		home_member_alive;	/* Cache for frequent tests */
        int		global_min_seqno;	/* global minimum seqno */
	int		watch_counter;		/* check aliveness of members */
	int		home_leave_seqno;	/* leave seqno of home member */
	int		seq_senders;		/* #concurrent seq upcalls */
					/* Message buffers */
        hist_t		hist;			/* history buffer */
#ifdef NO_ORDERER_THREAD
	pan_fragment_p	seq_cp_frag;		/* fragment for late delivery */
#endif

				/* Statistics: */
	int	       *stat;
	int	       *discard;
	int	       *discard_cleanup;
};




typedef	enum	GRP_MSG_T {
    GRP_JOINREQ,
    GRP_JOIN,
    GRP_LEAVEREQ,
    GRP_LEAVE,
    GRP_PB_REQ,
    GRP_PB_ACPT,
    GRP_BB_REQ,
    GRP_BB_ACPT,
    GRP_RETRANS,
    GRP_SYNC,
    GRP_STATUS,
    GRP_SUICIDE
}   grp_msg_t, *grp_msg_p;


typedef	enum	HDR_FLAG_T {
    HDR_is_blank	= 0,
    HDR_is_bigendian	= (0x1 << 0),
    HDR_is_suicide	= (0x1 << 1),
    HDR_is_rexmit	= (0x1 << 2),
    HDR_is_continuation	= (0x1 << 3)
}   hdr_flag_t, *hdr_flag_p;


typedef	struct	GRP_HDR_T	grp_hdr_t, *grp_hdr_p;

struct	GRP_HDR_T {
    unsigned char	flags;		/* An or-ing of hdr_flag_t's */
    unsigned char	type;		/* Actually, a grp_msg_t */
    short int		gid;
    short int		sender;
    short int		sender_id;
    int			seqno;
    int			messid;
};


#endif
