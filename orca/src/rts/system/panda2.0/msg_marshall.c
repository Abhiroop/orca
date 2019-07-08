/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include "pan_sys.h"
#include "interface.h"
#include "manager.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#ifdef PURE_WRITE
#include "proxy.h"
#endif
#include "rts_internals.h"

typedef struct ma_object {
    int mo_size;  /* size of object data + RTS part + manager part */
    int mo_flags; /* object status + send_data_bit */
} ma_object_t, *ma_object_p;


#ifdef PURE_WRITE
op_hdr_t *
mm_unpack_op_call(pan_msg_p m, fragment_p *objp, void ***argv, int *pure_write)
{
    op_hdr_t *hdr;
    char *buf;
    op_dscr *op;
    fragment_p obj;
    int unmarshall;

    hdr = pan_msg_pop(m, sizeof(op_hdr_t), alignof(op_hdr_t));

    /*
     * If this message was sent by someone else _and_ I can find the
     * fragment which the operation needs, then I unmarshall the
     * argument buffer.
     */
    obj = otab_lookup(hdr->oh_oid);
    if (hdr->oh_src == rts_my_pid) {   /* sent by this node */
	assert(obj);
        *pure_write = rts_is_pure_write(obj->fr_type, hdr->oh_opindex);
	unmarshall = *pure_write;

    } else if (obj) {                  /* we have the object */
        *pure_write = rts_is_pure_write(obj->fr_type, hdr->oh_opindex);
	unmarshall = 1;

    } else {                           /* unknown object; ignore message */
        *pure_write = 0;
	unmarshall = 0;
    }
    *objp = obj;

    if (unmarshall) {
	if (hdr->oh_argsize > 0) {
	    buf = pan_msg_pop(m, hdr->oh_argsize, alignof(char));
	} else {
	    buf = NULL;
	}
	op = &(td_operations((*objp)->fr_type)[hdr->oh_opindex]);
	(*op->op_unmarshall_op_call)(buf, argv);
    } else {
	*argv = 0;
    }
	
    return hdr;
}
#else
op_hdr_t *
mm_unpack_op_call(pan_msg_p m, fragment_p *objp, void ***argv, man_piggy_p *piggy)
{
    op_hdr_t *hdr;
    char *buf;
    op_dscr *op;

    hdr = (op_hdr_t *)pan_msg_pop(m, sizeof(op_hdr_t), alignof(op_hdr_t));

    *piggy = (man_piggy_p)pan_msg_pop(m, sizeof(man_piggy_t),
				      alignof(man_piggy_t));

    /*
     * If this message was sent by someone else _and_ I can find the
     * fragment which the operation needs, then I unmarshall the
     * argument buffer.
     */
    if (hdr->oh_src != rts_my_pid && (*objp = otab_lookup(hdr->oh_oid))) {
	if (hdr->oh_argsize > 0) {
	    buf = pan_msg_pop(m, hdr->oh_argsize, alignof(char));
	} else {
	    buf = NULL;
	}
	op = &(td_operations((*objp)->fr_type)[hdr->oh_opindex]);
	(*op->op_unmarshall_op_call)(buf, argv);
    } else {
	*argv = 0;
	*objp = 0;
    }
	
    return hdr;
}
#endif

void 
mm_pack_op_call(pan_msg_p m, fragment_p obj, man_piggy_p piggy_info,
		int opindex, void **argv, void *ptr)
{
    op_dscr *op = &(td_operations(obj->fr_type)[opindex]);
    int argsize = (*op->op_size_op_call)(argv);
    op_hdr_t *hdr;
    char *buf;

    /* Pack the arguments. Inverse of mm_unpack_op_args().
     */
    buf = pan_msg_push(m, argsize, alignof(char));
    (void)(*op->op_marshall_op_call)(buf, argv);

    man_pack_piggy_info(m, piggy_info);

    /* Pack the header. Inverse of mm_unpack_op_header().
     */
    hdr = (op_hdr_t *)pan_msg_push(m, sizeof(op_hdr_t), 
				       alignof(op_hdr_t));
    hdr->oh_argsize = argsize;                      /* arg. size */
    hdr->oh_opindex = opindex;                      /* operation index */
    hdr->oh_src     = rts_my_pid;
    hdr->oh_oid     = obj->fr_oid;
    hdr->oh_ptr     = ptr;
}

void
mm_pack_op_result(pan_msg_p m, int success, tp_dscr *obj_type, int opindex, void **argv)
{
    struct op_ret *ret;
    int retsize = 0;
    char *buf;
    op_dscr *op;

    if (success) {
        op      = &(td_operations(obj_type)[opindex]);
	retsize = (*op->op_size_op_return)(argv); /*argv */
	buf     = pan_msg_push(m, retsize, alignof(char));

	(void)(*op->op_marshall_op_return)(buf, argv);
    } else if (argv) {
        op = &(td_operations(obj_type)[opindex]);
	(*op->op_free_op_return)(argv);
    }
    ret = (struct op_ret *)pan_msg_push(m, sizeof(struct op_ret),
					    alignof(struct op_ret));
    ret->ok      = success;
    ret->retsize = retsize;
}

void
mm_unpack_op_result(pan_msg_p m, tp_dscr *obj_type, int opindex,
				int *success, void **argv)
{
    op_dscr *op = &(td_operations(obj_type)[opindex]);
    struct op_ret *ret;

    ret = (struct op_ret *)pan_msg_pop(m, sizeof(struct op_ret),
					   alignof(struct op_ret));
    if (ret->ok) {
	char *buf;

	buf = pan_msg_pop(m, ret->retsize, alignof(char));
	(*op->op_unmarshall_op_return)(buf, argv);
    }
    *success = ret->ok;
}

void
mm_pack_args(pan_msg_p msg, prc_dscr *descr, void **argv, int cpu)
{
    int size, *sbuf;
    char *buf;
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;


    size = (*descr->prc_size_args)(argv);
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
	if (par->par_mode == SHARED) {
	    size += o_shared_nbytes(argv[i], par->par_descr, cpu);
	}
    }
    buf   = (char *)pan_msg_push(msg, size, 1);
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
	if (par->par_mode == SHARED) {
	    buf = o_shared_marshall(buf, argv[i], par->par_descr, cpu);
	}
    }
    (void)(*descr->prc_marshall_args)(buf, argv);
    sbuf  = (int *)pan_msg_push(msg, sizeof(int), alignof(int));
    *sbuf = size;
}

void
mm_unpack_args(pan_msg_p msg, prc_dscr *descr, void ***argvp, int need_values)
{
    int *size;
    char *buf;
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;

    if (nf > 0) {
        *argvp = m_malloc(nf * sizeof(void *));
    }
    else *argvp = 0;
    size = (int *)pan_msg_pop(msg, sizeof(int), alignof(int));
    buf  = (char *)pan_msg_pop(msg, *size, 1);
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
	if (par->par_mode == SHARED) {
	    buf = o_shared_unmarshall(buf, &((*argvp)[i]), par->par_descr);
	}
	else (*argvp)[i] = 0;
    }
    if (need_values) {
	(void)(*descr->prc_unmarshall_args)(buf, *argvp);
    }
}

void 
mm_pack_sh_object(pan_msg_p m, fragment_p obj)
{
    obj_info *oi = td_objinfo(obj->fr_type);
    int size, mflags;
    ma_object_p mbuf;
    char *buf;
    
    mflags = rts_prepare_object_marshall(obj);
    size   = rtspart_nbytes(obj, obj->fr_type, mflags);
    if (mflags & 1) size += (*oi->obj_size_obj)(obj);
    buf   = pan_msg_push(m, size, 1);
    buf   = rtspart_marshall(buf, obj, obj->fr_type, mflags);
    if (mflags & 1) (void)(*oi->obj_marshall_obj)(buf, obj);
    mbuf  = pan_msg_push(m, sizeof(ma_object_t), alignof(ma_object_t));
    mbuf->mo_size  = size;
    mbuf->mo_flags = mflags;
}

void 
mm_unpack_sh_object(pan_msg_p m, fragment_p obj)
{
    obj_info *oi = td_objinfo(obj->fr_type);
    ma_object_p mbuf;
    int got_data_bit;
    char *buf;
    
    mbuf = pan_msg_pop(m, sizeof(ma_object_t), alignof(ma_object_t));
    buf  = (char *)pan_msg_pop(m, mbuf->mo_size, 1);

    /* At OrcaMain and with arrays of shared objects two instances of
     * the "same" structure t_object will be created. To avoid
     * inconsistencies, always pre-allocate o_fields even though the data
     * may never be stored at this site.
     */
    if (!obj->fr_fields) {			/* avoid stale copy! */
	obj->fr_fields = m_malloc(td_objrec(obj->fr_type)->td_size);
    }

    got_data_bit = mbuf->mo_flags & 1;
    if (got_data_bit) {
	if (obj->fr_flags & MAN_VALID_FIELD) {	/* avoid space leak */
	    if (td_objinfo(obj->fr_type)->obj_rec_free) {
	      (*(td_objinfo(obj->fr_type)->obj_rec_free))(obj->fr_fields);
	    }
	} else {
	    obj->fr_flags |= MAN_VALID_FIELD;
	}
    }
    buf = rtspart_unmarshall(buf, obj, obj->fr_type, mbuf->mo_flags);
    if (mbuf->mo_flags & 1) {
	(void)(*oi->obj_unmarshall_obj)(buf, obj);
    }
}
