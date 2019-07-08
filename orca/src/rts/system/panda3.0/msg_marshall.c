/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef MARSHALL_INLINE
#define MARSHALL_SRC
#endif

#include <assert.h>
#include <string.h>
#include "pan_sys.h"
#include "orca_types.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#ifdef PURE_WRITE
#include "proxy.h"
#endif
#include "rts_internals.h"
#include "interface.h"
#include "rts_comm.h"
#include "rts_prot_stack.h"

#ifdef MARSHALL_SRC
#include "manager.h"			/* break include cycle */
#undef INLINE_FUNCTIONS
#endif
#include "inline.h"

#ifndef NSTATISTICS
extern int mm_ucast_bytes;
extern int mm_ucast_nr;
extern int mm_mcast_bytes;
extern int mm_mcast_nr;
#endif

typedef struct ma_object {
    int mo_size;  /* size of object data + RTS part + manager part */
    int mo_flags; /* object status + send_data_bit */
} ma_object_t, *ma_object_p;

typedef struct { op_hdr_t oph; man_piggy_t pigh;} comb_hdr_t;


#ifdef PURE_WRITE
INLINE op_hdr_t *
mm_unpack_op_call(void *m, int *len, fragment_p *objp, op_dscr **op,
		  void ***argv, man_piggy_p *piggy_info, int *pure_write)
{
    op_hdr_t *hdr;
    char *buf;
    int unmarshall;
    comb_hdr_t *rts_hdr;
    fragment_p obj;

    rts_hdr = rts_ps_pop(m, len, sizeof(op_hdr_t));
    hdr = &rts_hdr->oph;
    *piggy_info = &rts_hdr->pigh;

    buf = rts_ps_pop(m, len, hdr->oh_argsize);

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
	*op = &(td_operations((*objp)->fr_type)[hdr->oh_opindex]);
	(*(*op)->op_unmarshall_op_call)(buf, argv);
    } else {
	*argv = 0;
	*objp = 0;
	*op = 0;
    }
	
    return hdr;
}

#else		/* PURE_WRITE */

op_hdr_t *
mm_unpack_op_call(void *m, int *len, fragment_p *objp, op_dscr **op,
		  void ***argv, man_piggy_p *piggy_info)
{
    op_hdr_t *hdr;
    char *buf;
    comb_hdr_t *rts_hdr;

    rts_hdr = rts_ps_pop(m, len, sizeof(comb_hdr_t));
    hdr = &rts_hdr->oph;
    *piggy_info = &rts_hdr->pigh;

    buf = rts_ps_pop(m, len, hdr->oh_argsize);

    /*
     * If this message was sent by someone else _and_ I can find the
     * fragment which the operation needs, then I unmarshall the
     * argument buffer.
     */
    if (hdr->oh_src != rts_my_pid && (*objp = otab_lookup(hdr->oh_oid))) {
	*op = &(td_operations((*objp)->fr_type)[hdr->oh_opindex]);
	(*(*op)->op_unmarshall_op_call)(buf, argv);
    } else {
	*argv = 0;
	*objp = 0;
	*op = 0;
    }
	
    return hdr;
}
#endif		/* PURE_WRITE */


INLINE void * 
mm_pack_op_call(int *size, int *len, fragment_p obj, op_dscr *op, void **argv,
		void *ptr, int mcast)
{
    int argsize = (*op->op_size_op_call)(argv);
    char *buf;
    op_hdr_t *hdr;
    man_piggy_p piggy_hdr;
    comb_hdr_t *rts_hdr;
    void *m;

#ifndef NSTATISTICS
    if (mcast) {
    	mm_mcast_nr++;
    	mm_mcast_bytes += argsize;
    } else {
    	mm_ucast_nr++;
    	mm_ucast_bytes += argsize;
    }
#endif 

    *size = ps_align(argsize) + ps_align(sizeof(comb_hdr_t)) +
	    ps_align(mcast ? RC_MCAST_SIZE : RC_REQ_SIZE) + rc_trailer;
    m = pan_malloc(*size);
    *len = 0;

    /* Pack the arguments. Inverse of mm_unpack_op_args().
     */
    buf = rts_ps_push(m, len, argsize);
    (void)(*op->op_marshall_op_call)(buf, argv);

    /* Push combined operation and piggyback header
     */
    rts_hdr = rts_ps_push(m, len, sizeof(comb_hdr_t));
    hdr = (op_hdr_t *)&rts_hdr->oph;
    piggy_hdr = &rts_hdr->pigh;
 
    /* Pack the piggy back info for object distribution decisions.
     */
    man_get_piggy_info(obj, piggy_hdr);

    /* Pack the header. Inverse of mm_unpack_op_header().
     */
    hdr->oh_argsize = argsize;			/* arg. size */
    hdr->oh_opindex = op->op_index;		/* operation index */
    hdr->oh_src     = rts_my_pid;
    hdr->oh_oid     = obj->fr_oid;
    hdr->oh_ptr     = ptr;

    return m;
}

INLINE void
mm_pack_op_result(void **m, int *size, int *len, int success, 
		  op_dscr *op, void **argv)
{
    struct op_ret *ret;
    int ret_len = 0;
    int ret_size;
    char *buf;

    if (success) {
	ret_len = (*op->op_size_op_return)(argv);
#ifndef NSTATISTICS
    	mm_ucast_nr++;
    	mm_ucast_bytes += ret_len;
#endif 
    }

    ret_size = ps_align(ret_len) + ps_align(sizeof(struct op_ret)) +
	       RC_REPL_SIZE + rc_trailer;
    if (*m == NULL || ret_size > *size) {
	pan_free(*m);
	*m = pan_malloc(ret_size);
	*size = ret_size;
    }

    *len = 0;

    if (success) {
	buf = rts_ps_push(*m, len, ret_len);
	(void)(*op->op_marshall_op_return)(buf, argv);
    } else if (argv) {
	(*op->op_free_op_return)(argv);
    }

    ret = rts_ps_push(*m, len, sizeof(struct op_ret));
    ret->ok      = success;
    ret->retsize = ret_len;
}

INLINE void
mm_unpack_op_result(void *m, int *len, op_dscr *op, int *success, void **argv)
{
    struct op_ret *ret;

    ret = rts_ps_pop(m, len, sizeof(struct op_ret));
    if (ret->ok) {
	char *buf;

	buf = rts_ps_pop(m, len, ret->retsize);
	(*op->op_unmarshall_op_return)(buf, argv);
    }
    *success = ret->ok;
}

#ifdef OUTLINE

void
mm_pack_args(void **msg, int *size, int *len, prc_dscr *descr, void **argv,
	     int cpu)
{
    int arg_size, *sbuf;
    char *buf;
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;

    arg_size = (*descr->prc_size_args)(argv);
#ifndef NSTATISTICS
    mm_mcast_nr++;
    mm_mcast_bytes += arg_size;
#endif
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
      if (par->par_mode == SHARED) {
          arg_size += o_shared_nbytes(argv[i], par->par_descr, cpu);
      }
    }

    buf   = rc_msg_push(msg, size, len, arg_size);

    for (i = 0, par = td_params(d); i < nf; i++, par++) {
      if (par->par_mode == SHARED) {
          buf = o_shared_marshall(buf, argv[i], par->par_descr, cpu);
      }
    }
    (void)(*descr->prc_marshall_args)(buf, argv);

    sbuf  = rc_msg_push(msg, size, len, sizeof(int));
    *sbuf = arg_size;
}

void
mm_unpack_args(void *msg, int *len, prc_dscr *descr, void ***argvp,
	       int need_values)
{
    int *arg_size;
    char *buf;
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;

    if (nf > 0) {
        *argvp = m_malloc(nf * sizeof(void *));
    }
    else *argvp = 0;
    arg_size = rc_msg_pop(msg, len, sizeof(int));
    buf  = rc_msg_pop(msg, len, *arg_size);
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
mm_pack_sh_object(void **m, int *size, int *len, fragment_p obj)
{
    obj_info *oi = td_objinfo(obj->fr_type);
    int obj_size, mflags;
    ma_object_p mbuf;
    char *buf;
    
    mflags = rts_prepare_object_marshall(obj);
    obj_size = rtspart_nbytes(obj, obj->fr_type, mflags);
    if (mflags & 1) {
	int user_size = (*oi->obj_size_obj)(obj);
	obj_size += user_size;
#ifndef NSTATISTICS
	if (*len == 0) mm_ucast_nr++;	/* multiple objects per message */
    	mm_ucast_bytes += user_size;
#endif
    }
    buf = rc_msg_push(m, size, len, obj_size);
    buf = rtspart_marshall(buf, obj, obj->fr_type, mflags);
    if (mflags & 1) (void) (*oi->obj_marshall_obj)(buf, obj);
    mbuf  = rc_msg_push(m, size, len, sizeof(ma_object_t));
    mbuf->mo_size  = obj_size;
    mbuf->mo_flags = mflags;
}

void 
mm_unpack_sh_object(void *m, int *len, fragment_p obj)
{
    obj_info *oi = td_objinfo(obj->fr_type);
    ma_object_p mbuf;
    int got_data_bit;
    char *buf;
    
    mbuf = rc_msg_pop(m, len, sizeof(ma_object_t));
    buf  = rc_msg_pop(m, len, mbuf->mo_size);

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

void
mm_print_stats( void)
{
#ifndef NSTATISTICS
    printf( "%2d) UCAST #msg %6d, #bytes %10d, "
                 "MCAST #msg %6d, #bytes %10d\n",
            rts_my_pid, mm_ucast_nr, mm_ucast_bytes, mm_mcast_nr, mm_mcast_bytes);
#endif
}


void
mm_reset_stats(void)
{
#ifndef NSTATISTICS
    mm_mcast_nr = 0;
    mm_mcast_bytes = 0;
    mm_ucast_nr = 0;
    mm_ucast_bytes = 0;
#endif 
    pan_stats_reset();
}

#endif

#undef MARSHALL_SRC
