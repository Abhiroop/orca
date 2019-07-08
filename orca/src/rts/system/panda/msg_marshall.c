#include <string.h>
#include "orca_types.h"
#include "manager.h"
#include "marshall.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#include "rts_internals.h"
#include "interface.h"


op_hdr_t *
mm_unpack_op_call(message_p m, fragment_p *f, tp_dscr **tdesc, void ***argv)
{
    otab_entry_p entry;
    op_hdr_t *hdr;
    char *buf;

    hdr = (op_hdr_t *)sys_message_pop(m, sizeof(op_hdr_t), alignof(op_hdr_t));

    *tdesc = (tp_dscr *) m_getptr(hdr->oh_tdreg);  /* convert to pointer! */

    /*
     * If this message was sent by someone else _and_ I can find the
     * fragment which the operation needs, then I unmarshall the
     * argument buffer.
     */
    if (hdr->oh_src != sys_my_pid && (entry = otab_lookup(&hdr->oh_oid))) {
	*f = &entry->frag;
	if (hdr->oh_argsize > 0) {
	    buf = sys_message_pop(m, hdr->oh_argsize, alignof(char));
	} else {
	    buf = NULL;
	}
	unmarshall_op_call(buf, *tdesc, hdr->oh_opindex, argv);
    } else {
	*argv = 0;
	*f    = (fragment_p)0;
    }
	
    return hdr;
}


void 
mm_pack_op_call(message_p m, oid_p oid, man_piggy_p piggy_info,
		tp_dscr *obj_type, int opindex, void **argv, 
		invocation_p inv)
{
    int argsize = nbytes_op_call(obj_type, opindex, argv);
    op_hdr_t *hdr;
    char *buf;

    man_pack_piggy_info(m, piggy_info);


    /* Pack the arguments. Inverse of mm_unpack_op_args().
     */
    buf = sys_message_push(m, argsize, alignof(char));
    (void)marshall_op_call(buf, obj_type, opindex, argv); /* arguments */


    /* Pack the header. Inverse of mm_unpack_op_header().
     */
    hdr = (op_hdr_t *)sys_message_push(m, sizeof(op_hdr_t), 
				       alignof(op_hdr_t));
    hdr->oh_argsize = argsize;                      /* arg. size */
    hdr->oh_opindex = opindex;                      /* operation index */
    hdr->oh_tdreg   = td_registration(obj_type);    /* type reg. index */
    hdr->oh_src     = sys_my_pid;
    oid_copy(oid, &hdr->oh_oid);
    hdr->oh_inv     = inv;
}


void
mm_pack_op_result(message_p m, int success, tp_dscr *obj_type, int opindex, void **argv)
{
    struct op_ret *ret;
    int retsize = 0;
    char *buf;

    if (success) {
	retsize = nbytes_op_return(obj_type, opindex, argv); /*argv */
	buf     = sys_message_push(m, retsize, alignof(char));

	(void)marshall_op_return(buf, obj_type, opindex, argv);
    } else if (argv) {
	free_op_return(obj_type, opindex, argv);
    }
    ret = (struct op_ret *)sys_message_push(m, sizeof(struct op_ret),
					    alignof(struct op_ret));
    ret->ok      = success;
    ret->retsize = retsize;
}


void
mm_unpack_op_result(message_p m, tp_dscr *obj_type, int opindex,
				int *success, void **argv)
{
    struct op_ret *ret;

    ret = (struct op_ret *)sys_message_pop(m, sizeof(struct op_ret),
					   alignof(struct op_ret));
    if (ret->ok) {
	char *buf;

	buf = sys_message_pop(m, ret->retsize, alignof(char));
	unmarshall_op_return(buf, obj_type, opindex, argv);
    }
    *success = ret->ok;
}


void
mm_pack_args(message_p msg, prc_dscr *descr, void **argv, int cpu)
{
	int size, *sbuf;
	char *buf;

	size  = nbytes_args(descr, argv, cpu);
	buf   = (char *)sys_message_push( msg, size, 1);
	sbuf  = (int *)sys_message_push(msg, sizeof(int), alignof(int));
	*sbuf = size;
	(void) marshall_args( buf, descr, argv, cpu);
}


void
mm_unpack_args(message_p msg, prc_dscr *descr, void ***argvp, int need_values)
{
	int *size;
	char *buf;

	size = (int *)sys_message_pop(msg, sizeof(int), alignof(int));
	buf  = (char *)sys_message_pop(msg, *size, 1);
	(void) unmarshall_args( buf, descr, argvp, need_values);
}


void 
mm_pack_sh_object(message_p m, fragment_p obj)
{
	int size, *sbuf;
	char *buf;

	size  = nbytes_object(obj);
	buf   = (char *)sys_message_push(m, size, 1);
	sbuf  = (int *)sys_message_push(m, sizeof(int), alignof(int));
	*sbuf = size;
	(void) marshall_object( buf, obj);
}


void 
mm_unpack_sh_object(message_p m, fragment_p obj)
{
	int *size;
	char *buf;

	size = (int *)sys_message_pop(m, sizeof(int), alignof(int));
	buf  = (char *)sys_message_pop( m, *size, 1);
	(void) unmarshall_object( buf, obj);
}
