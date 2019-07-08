#ifdef TEST_MARSHALL
#include <interface.h>
#include <assert.h>
#include "test_marshall.h"
#include "pan_sys.h"

int
pan_msg_consume(struct msg *p, void *b, int sz)
{
	if (sz > p->len - (p->p - p->buf)) {
		sz = p->len - (p->p - p->buf);
		fprintf(stderr, "wrong pan_msg_consume???\n");
	}
	memcpy(b, p->p, sz);
	p->p += sz;
	return sz;
}

void marshall_and_unmarshall_args(struct op_descr *op, void **args, void ***argp){
	int	szvec = (*(op->op_size_op_call))(args);
	pan_iovec_p
		vec = m_malloc(szvec * sizeof(*vec));
	struct msg
		mbuf;
	int	i;
	
	(void) (*(op->op_marshall_op_call))(vec, args);
	mbuf.len = 0;
	for (i = 0; i < szvec; i++) {
		mbuf.len += vec[i].len;
	}
	mbuf.p = mbuf.buf = m_malloc(mbuf.len);
	for (i = 0; i < szvec; i++) {
		memcpy(mbuf.p, vec[i].data, vec[i].len);
		mbuf.p += vec[i].len;
	}
	mbuf.p = mbuf.buf;
	(*(op->op_unmarshall_op_call))(&mbuf, argp);
	m_free(mbuf.buf);
	m_free(vec);
}

void marshall_and_unmarshall_return(struct op_descr *op, void **argp, void **args)
{
	int	szvec = (*(op->op_size_op_return))(argp);
	pan_iovec_p
		vec = m_malloc(szvec * sizeof(*vec));
	struct msg
		mbuf;
	int	i;
	
	(void) (*(op->op_marshall_op_return))(vec, argp);
	mbuf.len = 0;
	for (i = 0; i < szvec; i++) {
		mbuf.len += vec[i].len;
	}
	mbuf.p = mbuf.buf = m_malloc(mbuf.len);
	for (i = 0; i < szvec; i++) {
		memcpy(mbuf.p, vec[i].data, vec[i].len);
		mbuf.p += vec[i].len;
	}
	(*(op->op_free_op_return))(argp);
	mbuf.p = mbuf.buf;
	(*(op->op_unmarshall_op_return))(&mbuf, args);
	m_free(mbuf.buf);
	m_free(vec);
}
#endif
