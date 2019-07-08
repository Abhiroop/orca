/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: alloc.c,v 1.6 1995/07/31 09:05:02 ceriel Exp $ */

#include <interface.h>

pthread_mutex_t malloc_lock;

/* On Solaris 2, malloc/free are MT-safe */

void *m_malloc(size_t n)
{
  void *p;
  
#ifndef SOLARIS2
  pthread_mutex_lock(&malloc_lock);
#endif
  p = malloc(n);
#ifndef SOLARIS2
  pthread_mutex_unlock(&malloc_lock);
#endif

  if (p == NULL && n != 0) {
	m_liberr("Limit exceeded", "out of memory");
  }
  return p;
}

void *m_realloc(void *p, size_t n)
{
#ifndef SOLARIS2
  pthread_mutex_lock(&malloc_lock);
#endif
  if (p == NULL) p = malloc(n);
  else p = realloc(p, n);
#ifndef SOLARIS2
  pthread_mutex_unlock(&malloc_lock);
#endif

  if (p == NULL && n != 0) {
	m_liberr("Limit exceeded", "out of memory");
  }
  return p;
}

void m_free(void *p)
{
#ifndef SOLARIS2
  pthread_mutex_lock(&malloc_lock);
#endif
  free(p);
#ifndef SOLARIS2
  pthread_mutex_unlock(&malloc_lock);
#endif
}
