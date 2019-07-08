/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: conv.c,v 1.5 1996/07/04 08:51:29 ceriel Exp $ */

#include <interface.h>

#define BUFSZ 128

t_longreal f_conversions__StringToReal(t_string *s, t_integer *eaten)
{
  double	f = 0.0;

  *eaten = 0;
  if (s->a_sz > 0) {
  	char	buf[BUFSZ];
  	char	*sf, *p;
	sf = &((char *)(s->a_data))[s->a_offset];
	if (s->a_sz < BUFSZ) {
		strncpy(buf, sf,  s->a_sz);
		buf[s->a_sz] = 0;
		f = strtod(buf, &p);
		*eaten = p - buf;
	}
	else {
		p = m_malloc(s->a_sz+1);
		strncpy(p, sf,  s->a_sz);
		p[s->a_sz] = 0;
		f = strtod(p, &sf);
		*eaten = sf - p;
		m_free(p);
	}
  }
  return f;
}

t_longint f_conversions__StringToInt(t_string *s, t_integer *eaten)
{
  t_longint	l = 0;

  *eaten = 0;
  if (s->a_sz > 0) {
  	char	buf[BUFSZ];
  	char	*sf, *p;
	sf = &((char *)(s->a_data))[s->a_offset];
	if (s->a_sz < BUFSZ) {
		strncpy(buf, sf,  s->a_sz);
		buf[s->a_sz] = 0;
		l = strtol(buf, &p, 10);
		*eaten = p - buf;
	}
	else {
		p = m_malloc(s->a_sz+1);
		strncpy(p, sf,  s->a_sz);
		p[s->a_sz] = 0;
		l = strtol(p, &sf, 10);
		*eaten = sf - p;
		m_free(p);
	}
  }
  return l;
}

void f_conversions__RealToString(t_longreal r, t_integer p, t_string *s)
{
  char	buf[128];

  if (p <= 0) p = 1;
  if (p > 100) p = 100;

  sprintf(buf, "%.*g", p, (double) r);
  free_string(s);
  a_allocate(s, 1, 1, 1, strlen(buf));
  strncpy(&((char *)(s->a_data))[s->a_offset], buf, s->a_sz);
}

void f_conversions__IntToString(t_longint i, t_string *s)
{
  char	buf[128];

  sprintf(buf, "%ld", i);
  free_string(s);
  a_allocate(s, 1, 1, 1, strlen(buf));
  strncpy(&((char *)(s->a_data))[s->a_offset], buf, s->a_sz);
}
