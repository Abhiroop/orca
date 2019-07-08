/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: graph.c,v 1.16 1998/05/15 10:31:04 ceriel Exp $ */

#include <interface.h>

#undef max
#define max(a,b)	((a) > (b) ? (a) : (b))

t_nodename nil;

void *
g_getnode(void *gg, size_t sz)
{
  t_graph	*g = gg;
  register void *pn = g->g_freenodes;
  register t_ndfree *pf;
  register int i;

  if (! pn) {
  	pf = (t_ndfree *) m_malloc(sizeof(t_ndfree));
  	pf->g_nxtblk = g->g_ndlist;
  	g->g_ndlist = pf;
  	pf->g_blk = m_malloc(INITSZ * sz);
  	pn = pf->g_blk;
	for (i = INITSZ; i > 0; i--) {
		*(void **) pn = (char *) pn + sz;
		pn = (char *) pn + sz;
	}
	pn = (char *) pn - sz;
	*(void **) pn = 0;
	pn = pf->g_blk;
  }
  g->g_freenodes = *(void **) pn;
  memset(pn, '\0', sz);
  return pn;
}

void
g_freeblocks(void *gg)
{
  t_graph	*g = gg;
  register int	i = g->g_size;
  register t_mt *gn = g->g_mt;

  while (--i >= 0) {
	if (! nodeisfree(gn) && gn->g_node) {
		*(void **)(gn->g_node) = g->g_freenodes;
		g->g_freenodes = gn->g_node;
		gn->g_node = 0;
	}
	gn++;
  }
  m_free((char *) g->g_mt);
  g->g_size = 0;
  g->g_freelist = 0;
  g->g_mt = 0;
  while (g->g_ndlist) {
	t_ndfree *nxt = g->g_ndlist->g_nxtblk;

	m_free(g->g_ndlist->g_blk);
	m_free((void *) (g->g_ndlist));
	g->g_ndlist  = nxt;
  }
  g->g_freenodes = 0;
}

#ifndef NO_AGE
void g_addnode(t_nodename *res, void *p, size_t sz)
#else
t_nodename g_addnode( void *p, size_t sz)
#endif
{
  register t_graph *g = p;
  register t_mt *gn;
#ifdef NO_AGE
  t_nodename res;
#endif

  if (g->g_freelist == 0) {
	register int i;

	g->g_mt = m_realloc((void *) g->g_mt, max(INITSZ, g->g_size+g->g_size) * sizeof(t_mt));
	if (g->g_size) {
		g->g_freelist = g->g_size + 1;
		i = g->g_size;
		g->g_size += g->g_size;
	}
	else {
#ifndef NO_AGE
		g->g_freelist = 1;
#else
		/* Don't use 0'th entry; 0 is reserved for the NIL value. */
		g->g_freelist = 2;
#endif
		g->g_size = INITSZ;
		i = 0;
	}
	gn = &g->g_mt[i];
	for (; i < g->g_size; i++) {
		gn->g_age = 2;	/* first non-zero even number */
		gn->g_nextfree = i+2;
		gn++;
	}
	gn--;
	gn->g_nextfree = 0;
  }
  gn = &g->g_mt[g->g_freelist-1];
  assert(nodeisfree(gn));
  gn->g_age++;
#ifndef NO_AGE
  res->n_index = gn - g->g_mt;
  res->n_age = gn->g_age;
#else
  res = gn - g->g_mt;
#endif
  g->g_freelist = gn->g_nextfree;
  gn->g_node = g_getnode(g, sz);
#ifdef NO_AGE
  return res;
#endif
}

#ifndef NO_AGE
void g_deletenode(register t_nodename *n, void *p, void (*f)(void *))
#else
void g_deletenode(register t_nodename n, void *p, void (*f)(void *))
#endif
{
  register t_graph *g = p;
  register t_mt *gn;

#ifndef NO_AGE
  gn = &g->g_mt[n->n_index];
#else
  gn = &g->g_mt[n];
#endif
  if (f) (*f)(gn->g_node);
  gn->g_age++;
  assert(nodeisfree(gn));
  *(void **)(gn->g_node) = g->g_freenodes;
  g->g_freenodes = gn->g_node;
  gn->g_nextfree = g->g_freelist;
  g->g_freelist = gn - g->g_mt + 1;
}
