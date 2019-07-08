/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __MSG_MARSHALL_H__
#define __MSG_MARSHALL_H__

#undef DECL
#if defined(MARSHALL_SRC) || !defined(INLINE_FUNCTIONS)
#define DECL	extern
#else
#define DECL	static
#endif

#include "pan_sys.h"
#include "orca_types.h"


#define mm_free_op_args(op, argv)	((op->op_free_op_return)(argv))

extern int mm_res_proto_top;
extern int mm_comb_proto_top;
extern pan_iovec_p mm_cached_iovecs;

#ifdef PURE_WRITE
DECL op_hdr_t *mm_unpack_op_call(void *m, int *len, fragment_p *objp,
			         op_dscr ** op, void ***argv,
				 man_piggy_p *piggy, int *pure_write);
#else
DECL op_hdr_t *mm_unpack_op_call(pan_msg_p m, void *proto, fragment_p *objp,
			         op_dscr **op, void ***argv,
				 man_piggy_p *piggy);
#endif

DECL pan_iovec_p mm_pack_op_call(int *size, void *proto, fragment_p obj,
				 op_dscr *op, void **argv, void *ptr,
				 int mcast);

DECL pan_iovec_p mm_pack_op_result(int *size, void *proto, int success, 
			    op_dscr *op, void **argv);

DECL void mm_unpack_op_result(pan_msg_p m, void *proto, op_dscr *op,
			      int *success, void **argv);

DECL pan_iovec_p mm_get_iovec(int len);

DECL void mm_free_iovec(pan_iovec_p p);

extern pan_iovec_p mm_pack_args(int *size, prc_dscr *descr,
			 	void **argv, int cpu);

extern void mm_unpack_args(pan_msg_p msg, prc_dscr *descr, void ***argvp,
			   int need_values);

extern void mm_pack_sh_object(pan_iovec_p *msg, int *size, fragment_p obj);

extern void mm_unpack_sh_object(pan_msg_p msg, fragment_p obj);

extern int mm_clean_sh_msg(pan_iovec_p iov, int i);

extern void mm_print_stats(void);

extern void mm_reset_stats(void);

extern void mm_start(void);

#if !defined(MARSHALL_SRC) && defined(INLINE_FUNCTIONS)
#define MARSHALL_INLINE
#include "src/msg_marshall.c"
#endif

#endif
