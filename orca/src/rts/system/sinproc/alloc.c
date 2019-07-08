/* $Id: alloc.c,v 1.3 1994/04/26 13:44:21 ceriel Exp $ */

#include <interface.h>

void *m_malloc(size_t n)
{
  void *p;
  
  p = malloc(n);

  if (p == NULL && n != 0) {
	m_liberr("Limit exceeded", "out of memory");
  }
  return p;
}

void *m_realloc(void *p, size_t n)
{
  if (p == NULL) p = malloc(n);
  else p = realloc(p, n);

  if (p == NULL && n != 0) {
	m_liberr("Limit exceeded", "out of memory");
  }
  return p;
}

void m_free(void *p)
{
  free(p);
}
