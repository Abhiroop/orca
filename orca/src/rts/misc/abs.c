/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: abs.c,v 1.3 1995/07/31 09:01:28 ceriel Exp $ */

#include <interface.h>

t_integer
m_abs(t_integer i)
{
	return i < 0 ? -i : i;
}

t_longint
m_labs(t_longint i)
{
	return i < 0 ? -i : i;
}

t_real
m_rabs(t_real i)
{
	return i < 0 ? -i : i;
}

t_longreal
m_lrabs(t_longreal i)
{
	return i < 0 ? -i : i;
}
