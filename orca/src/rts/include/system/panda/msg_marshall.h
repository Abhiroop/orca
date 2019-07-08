#ifndef __MSG_MARSHALL_H__
#define __MSG_MARSHALL_H__

#include "orca_types.h"


#define mm_free_op_args(tdesc, opindex, argv) \
  free_op_return(tdesc, opindex, argv)

extern op_hdr_t *mm_unpack_op_call(message_p m,
				   fragment_p *f,
				   tp_dscr **tdesc,
				   void ***argv);

extern void mm_unpack_op_args(message_p m, tp_dscr **obj_type, int *opindex,
			      void ***argv);

extern void mm_free_op_args(tp_dscr *obj_type, int opindex, void **argv);

extern void mm_pack_op_call(message_p m, oid_p oid, man_piggy_p piggy_info,
			    tp_dscr *obj_type, int opindex, void **argv,
			    invocation_p inv);

extern void mm_pack_op_result(message_p m, int success, 
			      tp_dscr *obj_type, int opindex, void **argv);

extern void mm_unpack_op_result(message_p m, tp_dscr *obj_type, int opindex,
				int *success, void **argv);

extern void mm_pack_args(message_p msg, prc_dscr *descr, void **argv, int cpu);

extern void mm_unpack_args(message_p msg, prc_dscr *descr, void ***argvp,
			   int need_values);

extern void mm_pack_sh_object(message_p msg, fragment_p obj);

extern void mm_unpack_sh_object(message_p msg, fragment_p obj);

#endif
