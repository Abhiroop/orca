/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: divide.c,v 1.6 1995/08/23 09:53:03 ceriel Exp $ */

#include <interface.h>

t_integer
m_div(t_integer i, t_integer j)
{
	t_integer r;

	if (i == 0) return 0;
	r = i % j;
	if (r && (i > 0) != (r > 0)) {
		return i / j + 1;
	}
	return i/j;
}

t_longint
m_divl(t_longint i, t_longint j)
{
	t_longint r;

	if (i == 0) return 0;
	r = i % j;
	if (r && (i > 0) != (r > 0)) {
		return i / j + 1;
	}
	return i/j;
}

t_integer
m_mod(t_integer i, t_integer j)
{
	t_integer r;

	if (i == 0) return 0;
	r = i % j;
	if (r && (i > 0) != (r > 0)) {
		r -= j;
	}
	return r;
}

t_longint
m_modl(t_longint i, t_longint j)
{
	t_longint r;

	if (i == 0) return 0;
	r = i % j;
	if (r && (i > 0) != (r > 0)) {
		r -= j;
	}
	return r;
}
