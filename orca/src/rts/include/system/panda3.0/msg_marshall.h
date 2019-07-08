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


#define mm_free_op_args(op, argv)	((*op->op_free_op_return)(argv))

#ifdef PURE_WRITE
DECL op_hdr_t *mm_unpack_op_call(void *m, int *len, fragment_p *objp,
			         op_dscr ** op, void ***argv,
				 man_piggy_p *piggy, int *pure_write);
#else
DECL op_hdr_t *mm_unpack_op_call(void *m, int *len, fragment_p *objp,
			         op_dscr **op, void ***argv,
				 man_piggy_p *piggy);
#endif

DECL void mm_unpack_op_args(void *m, int *len, tp_dscr **obj_type,
			    int *opindex, void ***argv);

DECL void *mm_pack_op_call(int *size, int *len, fragment_p obj, op_dscr *op,
			   void **argv, void *ptr, int mcast);

DECL void mm_pack_op_result(void **m, int *size, int *len, int success, 
			    op_dscr *op, void **argv);

DECL void mm_unpack_op_result(void *m, int *len, op_dscr *op,
			      int *success, void **argv);

extern void mm_pack_args(void **msg, int *size, int *len, prc_dscr *descr,
			 void **argv, int cpu);

extern void mm_unpack_args(void *msg, int *len, prc_dscr *descr, void ***argvp,
			   int need_values);

extern void mm_pack_sh_object(void **msg, int *size, int *len, fragment_p obj);

extern void mm_unpack_sh_object(void *msg, int *len, fragment_p obj);

extern void mm_print_stats(void);

extern void mm_reset_stats(void);

#if !defined(MARSHALL_SRC) && defined(INLINE_FUNCTIONS)
#define MARSHALL_INLINE
#include "src/msg_marshall.c"
#endif

#endif
