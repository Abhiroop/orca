/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: print.c,v 1.8 1996/07/04 08:54:08 ceriel Exp $ */

#include <interface.h>
#include "unixproc.h"

/* in Solaris 2, vfprintf is only Async_Safe */

void m_print(FILE *f, char *s, int len)
{
#ifndef SOLARIS2
  pthread_mutex_lock(&malloc_lock);
#endif
  fwrite(s, 1, len, f);
#ifndef SOLARIS2
  pthread_mutex_unlock(&malloc_lock);
#endif
}

int m_scan(FILE *f, char *s, void *p)
{
  int r;
#ifndef SOLARIS2
  pthread_mutex_lock(&malloc_lock);
#endif
  r = fscanf(f, s, p);
#ifndef SOLARIS2
  pthread_mutex_unlock(&malloc_lock);
#endif
  return r;
}
