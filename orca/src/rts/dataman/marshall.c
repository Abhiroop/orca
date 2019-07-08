/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: marshall.c,v 1.40 1998/06/11 12:00:50 ceriel Exp $ */

#include <interface.h>

#if !defined(inline)
#if defined(__GNUC__)
#define inline __inline__
#else
#define inline
#endif
#endif

static inline void
fast_mem_copy(char *dest, char *source, unsigned int nbytes)
{
#if defined(sparc) || defined(__sparc__)
    /* Only optimize the full 8 byte aligned non-overlapping case,
     * since that's mostly what's happening.
     */
    if ((((unsigned) source | (unsigned) dest) & 0x7) == 0) {
	register double *src, *dst;
	register int ndoubles;

	src = (double *)source;
	dst = (double *)dest;
 	ndoubles = nbytes >> 3;

	/* With a smart compiler, this generates a tight loop on the sparc: */
	while (ndoubles > 1) {
	    dst[ndoubles - 1] = src[ndoubles - 1];
	    dst[ndoubles - 2] = src[ndoubles - 2];
	    ndoubles -= 2;
	}
	if (ndoubles) {
	    *dst = *src;
	}
	if (nbytes & 0x7) {
	    if (nbytes & 0x6) {
		/* still one, two or three aligned shorts to be done */
		register unsigned short *shortsrc, *shortdst;

		shortsrc = (unsigned short *) &src[nbytes >> 3];
		shortdst = (unsigned short *) &dst[nbytes >> 3];
		if (nbytes & 0x4) {
		    /* two or three shorts */
		    *(unsigned int *) shortdst = *(unsigned int *) shortsrc;
		    if (nbytes & 0x2) {
			shortdst[2] = shortsrc[2];
		    }
		} else {
		    /* just one short */
		    shortdst[0] = shortsrc[0];
		}
	    }
	    if (nbytes & 1) {
		/* just one more byte to do */
		dest[nbytes - 1] = source[nbytes - 1];
	    }
	}
    } else
#endif
    {
	/* Just let the default memcpy handle all other cases */
	memcpy(dest, source, nbytes);
    }
}

int sz_string(t_string *a)
{
#ifdef PANDA4
    return (a->a_sz <= 0) ? 1 : 2;
#else
    int sz = 2 * sizeof(a->a_dims[0].a_lwb);
    if (a->a_sz <= 0) return sz;
    return sz + a->a_sz * sizeof(t_char);
#endif
}

#ifdef PANDA4
pan_iovec_p ma_string(pan_iovec_p p, t_string *a)
{
    p->data = a;
    p->len = sizeof(t_string);
    p++;
    if (a->a_sz <= 0) return p;
    p->data = (t_char *) a->a_data + a->a_offset;
    p->len = a->a_sz * sizeof(t_char);
    p++;
    return p;
}
#else
char *ma_string(char *p, t_string *a)
{
    t_char *s;
    put_tp(a->a_dims[0].a_lwb, p);
    put_tp(a->a_dims[0].a_nel, p);
    if (a->a_sz <= 0) return p;
    s = (t_char *) a->a_data + a->a_offset;
    fast_mem_copy(p, s, a->a_sz * sizeof(t_char));
    p += a->a_sz * sizeof(t_char);
    return p;
}
#endif

#ifdef PANDA4
void um_string(void *p, t_string *a)
{
    t_char *s;
    pan_msg_consume(p, a, sizeof(t_string));
    if (a->a_sz <= 0) { a->a_sz = 0; a->a_data = 0; return; }
    s = (t_char *) m_malloc(a->a_sz * sizeof(t_char));
    a->a_data = s - a->a_offset;
    pan_msg_consume(p, s, a->a_sz * sizeof(t_char));
}
#else
char *um_string(char *p, t_string *a)
{
    t_char *s;
    get_tp(a->a_dims[0].a_lwb, p);
    get_tp(a->a_dims[0].a_nel, p);
    a->a_sz = a->a_dims[0].a_nel;
    if (a->a_sz <= 0) a->a_sz = 0;
    a->a_offset = a->a_dims[0].a_lwb;
    if (a->a_sz == 0) { a->a_data = 0;  return p; }
    s = (t_char *) m_malloc(a->a_sz * sizeof(t_char));
    a->a_data = s - a->a_offset;
    fast_mem_copy(s, p, a->a_sz * sizeof(t_char));
    p += a->a_sz * sizeof(t_char);
    return p;
}
#endif

#ifdef DATA_PARALLEL

#define MBUFSZ	1024
/* marshall data parallel Orca operation into a "Saniya-type" message. */
void
opmarshall_f(message_p mp, void **arg, void *d)
{
  op_dscr *op_descr = d;
  char buf[MBUFSZ];
  void *p = buf;
  int sz;

  sz = (*(op_descr->op_size_op_call))(arg);
  sz = (sz + 7) & ~7;
  if (sz > MBUFSZ) p = m_malloc(sz);
  (void) (*(op_descr->op_marshall_op_call))(p, arg);
  message_append_data(mp, p, sz);
  if (sz > MBUFSZ) m_free(p);
}

/* unmarshall Orca type */
char *
opunmarshall_f(char *p, void ***arg, void *d, void **args, int sender, instance_p ip)
{
  op_dscr *op_descr = d;
  int i, nparams;
  par_dscr *params;
  
  nparams = td_nparams(op_descr->op_func);
  params = td_params(op_descr->op_func);

  p = (*(op_descr->op_unmarshall_op_call))(p, arg);
  p = (char *) (((int) p + 7) & ~7);
  
  for (i = 0; i < nparams; i++, params++) {
	if (params->par_mode != IN /* && params->par_mode != SHARED */) {
		if (sender != m_mycpu()) {
			if (params->par_mode == GATHERED) {
				(*arg)[i] = (*p_gatherinit)(ip, (*arg)[i], params->par_descr);
			}
		}
		else {
			if (params->par_mode == GATHERED) {
				(*arg)[i] = (*p_gatherinit)(ip, args[i], params->par_descr);
			}
			else {
				(*arg)[i] = args[i];
			}
		}
	}
  }
  return p;
}

void
opfree_in_params_f(void **args, void *d, int remove_outparams)
{
  op_dscr *op_descr = d;
  int i, nparams;
  par_dscr *params;

  nparams = td_nparams(op_descr->op_func);
  params = td_params(op_descr->op_func);

  for (i = 0; i < nparams; i++, params++) {
	if (remove_outparams && params->par_mode == GATHERED) {
		m_free(args[i]);
	}
	else if (params->par_mode == IN && params->par_free != 0) {
		(*(params->par_free))(args[i]);
	}
  }
}
#endif
