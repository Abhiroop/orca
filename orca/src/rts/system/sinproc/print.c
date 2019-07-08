/* $Id: print.c,v 1.5 1996/07/04 08:53:52 ceriel Exp $ */

#include <interface.h>

void m_print(FILE *f, char *s, int len)
{
  fwrite(s, 1, len, f);
}

int m_scan(FILE *f, char *s, void *p)
{
  return fscanf(f, s, p);
}
