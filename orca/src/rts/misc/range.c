/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: range.c,v 1.1 1995/08/23 09:52:56 ceriel Exp $ */

#include <interface.h>

t_integer
m_check(t_integer c, t_integer ub, char *fn, int ln)
{
	if ((unsigned) c >= ub) {
		m_trap(RANGE, fn, ln);
	}
	return c;
}
