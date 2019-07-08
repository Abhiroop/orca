/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: traps.c,v 1.10 1996/07/04 08:52:38 ceriel Exp $ */

#include <interface.h>

static void
prnt(FILE *f, char *s, ...)
{
  char	buf[512];
  va_list ap;

  va_start(ap, s);
  vsprintf(buf, s, ap);
  va_end(ap);
  m_print(stderr, buf, strlen(buf));
}

void m_trap(int n, char *fn, int ln)
{
  prnt(stderr, "Run-time error on CPU %d", m_mycpu());
  if (fn) {
  	prnt(stderr, ", file \"%s\", line %d",
		fn, ln);
  }
  prnt(stderr, ": ");
  switch(n) {
  case FROM_EMPTY_SET:
	prnt(stderr, "FROM on empty set or bag\n");
	break;
  case ARRAY_BOUND_ERROR:
	prnt(stderr, "array bound error\n");
	break;
  case LOCAL_DEADLOCK:
	prnt(stderr, "local deadlock\n");
	break;
  case BAD_NODENAME:
	prnt(stderr, "bad nodename\n");
	break;
  case UNION_ERROR:
	prnt(stderr, "union error\n");
	break;
  case BAD_CPU:
	prnt(stderr, "illegal cpu number\n");
	break;
  case CASE_ERROR:
	prnt(stderr, "case error\n");
	break;
  case ALIAS:
	prnt(stderr, "illegal alias\n");
	break;
  case FAILED_ASSERT:
	prnt(stderr, "assertion failed\n");
	break;
  case FALL_THROUGH:
	prnt(stderr, "no RETURN from function procedure/operation\n");
	break;
  case DIV_ZERO:
	prnt(stderr, "divide by 0\n");
	break;
  case RANGE:
	prnt(stderr, "range error in CHR or VAL\n");
	break;
  case ILL_MOD:
	prnt(stderr, "modulo by <= 0\n");
	break;
  default:
	prnt(stderr, "unknown error\n");
	break;
  }
  abort();
}

void m_syserr(char *s)
{
  m_liberr("System error", s);
}

void m_liberr(char *lib, char *s)
{
  prnt(stderr, "%s on CPU %d", lib, m_mycpu());
  prnt(stderr, ": %s\n", s);
  abort();
}
