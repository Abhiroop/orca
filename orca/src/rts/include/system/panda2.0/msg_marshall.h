/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __MSG_MARSHALL_H__
#define __MSG_MARSHALL_H__

#include "pan_sys.h"
#include "orca_types.h"


#define mm_free_op_args(tdesc, opindex, argv) \
  ((*(td_operations(tdesc)[opindex]).op_free_op_return)(argv))

#ifdef PURE_WRITE
extern op_hdr_t *mm_unpack_op_call(pan_msg_p m, fragment_p *f, void ***argv,
				   int *pure_write);
#else
extern op_hdr_t *mm_unpack_op_call(pan_msg_p m, fragment_p *f, void ***argv,
				   man_piggy_p *piggy);
#endif

extern void mm_unpack_op_args(pan_msg_p m, tp_dscr **obj_type, int *opindex,
			      void ***argv);

extern void mm_pack_op_call(pan_msg_p m, fragment_p obj,
			    man_piggy_p piggy_info, int opindex, void **argv,
			    void *ptr);

extern void mm_pack_op_result(pan_msg_p m, int success, 
			      tp_dscr *obj_type, int opindex, void **argv);

extern void mm_unpack_op_result(pan_msg_p m, tp_dscr *obj_type, int opindex,
				int *success, void **argv);

extern void mm_pack_args(pan_msg_p msg, prc_dscr *descr, void **argv, int cpu);

extern void mm_unpack_args(pan_msg_p msg, prc_dscr *descr, void ***argvp,
			   int need_values);

extern void mm_pack_sh_object(pan_msg_p msg, fragment_p obj);

extern void mm_unpack_sh_object(pan_msg_p msg, fragment_p obj);

#endif
