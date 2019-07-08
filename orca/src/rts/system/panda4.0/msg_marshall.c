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
#include "pan_align.h"
#include "orca_types.h"
#include "msg_marshall.h"
#include "obj_tab.h"
#ifdef PURE_WRITE
#include "proxy.h"
#endif
#include "rts_internals.h"
#include "interface.h"
#include "rts_comm.h"

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

typedef struct { op_hdr_t oph; man_piggy_t pigh;} comb_hdr_t;

extern int mm_res_proto_start;
extern int mm_res_proto_top;
#define mm_res_hdr(p)	((op_ret_t *) ((char *) (p) + mm_res_proto_start))
extern int mm_comb_proto_start;
extern int mm_comb_proto_top;
#define mm_comb_hdr(p)	((comb_hdr_t *) ((char *) (p) + mm_comb_proto_start))

typedef struct ma_object {
    int mo_size;  /* size of object data + RTS part + manager part */
    int mo_flags; /* object status + send_data_bit */
    int mo_usersize;
} ma_object_t, *ma_object_p;


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
	rts_unlock();
	(*(*op)->op_unmarshall_op_call)(buf, argv);
	rts_lock();
    } else {
	*argv = 0;
	*objp = 0;
	*op = 0;
    }

    return hdr;
}

#else		/* PURE_WRITE */

op_hdr_t *
mm_unpack_op_call(pan_msg_p m, void *proto, fragment_p *objp, op_dscr **op,
		  void ***argv, man_piggy_p *piggy_info)
{
    op_hdr_t *hdr;
    comb_hdr_t *rts_hdr;

    rts_hdr = mm_comb_hdr(proto);	/* sizeof(comb_hdr_t)); */
    hdr = &rts_hdr->oph;
    *piggy_info = &rts_hdr->pigh;

    /*
     * If this message was sent by someone else _and_ I can find the
     * fragment which the operation needs, then I unmarshall the
     * argument buffer.
     */
    if (hdr->oh_src != rts_my_pid && (*objp = otab_lookup(hdr->oh_oid))) {
	*op = &(td_operations((*objp)->fr_type)[hdr->oh_opindex]);
	rts_unlock();
	(*(*op)->op_unmarshall_op_call)(m, argv);
	rts_lock();
    } else {
	*argv = 0;
	*objp = 0;
	*op = 0;
    }

    return hdr;
}
#endif		/* PURE_WRITE */

static int
rc_iov_msg_len(pan_iovec_p iov, int len)
{
    int sz = 0;
    int i;
    for (i = 0; i < len; i++) {
	sz += iov->len;
	iov++;
    }
    return sz;
}

INLINE int
mm_iovec_len(pan_iovec_p p)
{
    assert(p != NULL);
    return (p-1)->len;
}

INLINE pan_iovec_p
mm_get_iovec(int len)
{
    pan_iovec_p p = mm_cached_iovecs;

    if (p) mm_cached_iovecs = p->data;

    if (p && p->len < len) {
	m_free(p);
	p = 0;
    }
    if (p == 0) {
	p = m_malloc((len+len+1) * sizeof(pan_iovec_t));
    	p->len = len+len;
    }
    return p+1;
}

INLINE void
mm_free_iovec(pan_iovec_p p)
{
    assert(p != NULL);
    p = p-1;
    p->data = mm_cached_iovecs;
    mm_cached_iovecs = p;
}

INLINE pan_iovec_p
mm_pack_op_call(int *argsize, void *proto, fragment_p obj, op_dscr *op,
		void **argv, void *ptr, int mcast)
{
    char *buf;
    op_hdr_t *hdr;
    man_piggy_p piggy_hdr;
    comb_hdr_t *rts_hdr;
    pan_iovec_p	iov;

    *argsize = (*op->op_size_op_call)(argv);

    iov = mm_get_iovec(*argsize);

    /* Pack the arguments.
     */
    (void)(*op->op_marshall_op_call)(iov, argv);

    /* Push combined operation and piggyback header.
     */
    rts_hdr = mm_comb_hdr(proto);	/* sizeof(comb_hdr_t)); */
    hdr = (op_hdr_t *)&rts_hdr->oph;
    piggy_hdr = &rts_hdr->pigh;

    /* Pack the piggy back info for object distribution decisions.
     */
    man_get_piggy_info(obj, piggy_hdr);

    /* Pack the header.
     */
    hdr->oh_opindex = op->op_index;		/* operation index */
    hdr->oh_src     = rts_my_pid;
    hdr->oh_oid     = obj->fr_oid;
    hdr->oh_ptr     = ptr;

#ifndef NSTATISTICS
    if (mcast) {
    	mm_mcast_nr++;
    	mm_mcast_bytes += rc_iov_msg_len(iov, *argsize);
    } else {
    	mm_ucast_nr++;
    	mm_ucast_bytes += rc_iov_msg_len(iov, *argsize);
    }
#endif 

    return iov;
}

INLINE pan_iovec_p
mm_pack_op_result(int *size, void *proto, int success, 
		  op_dscr *op, void **argv)
{
    op_ret_t *ret;
    pan_iovec_p iov;

    if (success) {
	*size = (*op->op_size_op_return)(argv);
    } else {
	*size = 0;
    }
    iov = mm_get_iovec(*size);

    if (success) {
	(void)(*op->op_marshall_op_return)(iov, argv);
#ifndef NSTATISTICS
    	mm_ucast_nr++;
    	mm_ucast_bytes += rc_iov_msg_len(iov, *size);
#endif 
    }

    ret = mm_res_hdr(proto);		/* sizeof(op_ret_t)); */
    ret->ok      = success;
    return iov;
}

INLINE void
mm_unpack_op_result(pan_msg_p m, void *proto, op_dscr *op, int *success,
		    void **argv)
{
    op_ret_t *ret;

    ret = mm_res_hdr(proto);
    if (ret->ok) {
	(*op->op_unmarshall_op_return)(m, argv);
    }
    *success = ret->ok;
}

#ifdef OUTLINE

#ifndef NSTATISTICS
int mm_ucast_bytes;
int mm_ucast_nr;
int mm_mcast_bytes;
int mm_mcast_nr;
#endif

pan_iovec_p mm_cached_iovecs;
int mm_res_proto_start;
int mm_res_proto_top;
int mm_comb_proto_start;
int mm_comb_proto_top;

pan_iovec_p
mm_pack_args(int *arg_size, prc_dscr *descr, void **argv,
	     int cpu)
{
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;
    pan_iovec_p iov, iovp;

    *arg_size = (*descr->prc_size_args)(argv);
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
      if (par->par_mode == SHARED) {
          *arg_size += o_shared_nbytes(argv[i], par->par_descr, cpu);
      }
    }

    iov = iovp = mm_get_iovec(*arg_size);

    for (i = 0, par = td_params(d); i < nf; i++, par++) {
      if (par->par_mode == SHARED) {
          iovp = o_shared_marshall(iovp, argv[i], par->par_descr, cpu);
      }
    }
#ifndef NSTATISTICS
    mm_mcast_nr++;
    mm_mcast_bytes += rc_iov_msg_len(iov, *arg_size);
#endif
    (void)(*descr->prc_marshall_args)(iovp, argv);
    return iov;
}

void
mm_unpack_args(pan_msg_p m, prc_dscr *descr, void ***argvp,
	       int need_values)
{
    tp_dscr *d = descr->prc_func;
    int nf = td_nparams(d);
    int i;
    par_dscr *par;

    if (nf > 0) {
        *argvp = m_malloc(nf * sizeof(void *));
    }
    else *argvp = 0;
    for (i = 0, par = td_params(d); i < nf; i++, par++) {
      if (par->par_mode == SHARED) {
          o_shared_unmarshall(m, &((*argvp)[i]), par->par_descr);
      }
      else (*argvp)[i] = 0;
    }
    if (need_values) {
      (*descr->prc_unmarshall_args)(m, *argvp);
    }
}

void
mm_pack_sh_object(pan_iovec_p *iov, int *size, fragment_p obj)
{
    obj_info *oi = td_objinfo(obj->fr_type);
    int obj_size, mflags;
    ma_object_p mbuf;
    char *buf;
    int		user_size;
    int		start = *size;
    pan_iovec_p	m;

    m = *iov;

    mflags = rts_prepare_object_marshall(obj);
    obj_size = rtspart_nbytes(obj, obj->fr_type, mflags);
    if (mflags & 1) {
	user_size = (*oi->obj_size_obj)(obj);
    } else {
	user_size = 0;
    }
    if (mm_iovec_len(m) < start+user_size+2) {
	m = mm_get_iovec(start+user_size+2);
	memcpy(m, *iov, sizeof(pan_iovec_t) * start);
	mm_free_iovec(*iov);
	*iov = m;
    }

    m[start].data = m_malloc(sizeof(ma_object_t));
    m[start].len  = sizeof(ma_object_t);
    mbuf  = m[start].data;
    mbuf->mo_size  = obj_size;
    mbuf->mo_flags = mflags;
    mbuf->mo_usersize = user_size;
    start++;

    m[start].len  = obj_size;
    m[start].data = m_malloc(obj_size);
    rtspart_marshall(m[start].data, obj, obj->fr_type, mflags);
    start++;

    if (user_size) { 
	(void)(*oi->obj_marshall_obj)(&m[start], obj);
#ifndef NSTATISTICS
	if (*size == 0) mm_ucast_nr++;	/* multiple objects per message */
    	mm_ucast_bytes += rc_iov_msg_len(&m[start], user_size);
#endif
    }
    start += user_size;

    *size = start;
}

void 
mm_unpack_sh_object(pan_msg_p m, fragment_p obj)
{
    ma_object_t mbuf;
    obj_info *oi = td_objinfo(obj->fr_type);
    int got_data_bit;

    /* At OrcaMain and with arrays of shared objects two instances of
     * the "same" structure t_object will be created. To avoid
     * inconsistencies, always pre-allocate o_fields even though the data
     * may never be stored at this site.
     */
    if (!obj->fr_fields) {			/* avoid stale copy! */
	obj->fr_fields = m_malloc(td_objrec(obj->fr_type)->td_size);
    }

    pan_msg_consume(m, &mbuf, sizeof(mbuf));

    got_data_bit = mbuf.mo_flags & 1;
    if (got_data_bit) {
	if (obj->fr_flags & MAN_VALID_FIELD) {	/* avoid space leak */
	    if (td_objinfo(obj->fr_type)->obj_rec_free) {
	      (*(td_objinfo(obj->fr_type)->obj_rec_free))(obj->fr_fields);
	    }
	} else {
	    obj->fr_flags |= MAN_VALID_FIELD;
	}
    }

    rtspart_unmarshall(m, obj, obj->fr_type, mbuf.mo_flags);
    if (got_data_bit) {
	(void)(*oi->obj_unmarshall_obj)(m, obj);
    }
}

int
mm_clean_sh_msg(pan_iovec_p iov, int i)
{
    ma_object_p mbuf = iov[i].data;
    m_free(iov[i+1].data);
    i = i + mbuf->mo_usersize + 2;
    m_free(mbuf);
    return i;
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

void
mm_start(void)
{
    mm_res_proto_start = align_to(rts_reply_proto_top, op_ret_t);
    mm_res_proto_top = mm_res_proto_start + sizeof(op_ret_t);
    if (rts_rpc_proto_top > rts_mcast_proto_top) {
    	mm_comb_proto_start = align_to(rts_rpc_proto_top, op_ret_t);
    }
    else {
    	mm_comb_proto_start = align_to(rts_mcast_proto_top, op_ret_t);
    }
    mm_comb_proto_top = mm_comb_proto_start + sizeof(comb_hdr_t);
}
#endif

#undef MARSHALL_SRC
