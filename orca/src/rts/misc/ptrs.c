/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: ptrs.c,v 1.5 1996/07/04 08:52:36 ceriel Exp $ */

#include <interface.h>

static void **ptrs;
static int nptrs;
static int maxptrs;

t_integer
m_ptrregister(void *p)
{
	if (nptrs >= maxptrs) {
		ptrs = m_realloc(ptrs, (maxptrs+50)*sizeof(void *));
		maxptrs += 50;
	}
	ptrs[nptrs] = p;
	return nptrs++;
}

void *
m_getptr(t_integer i)
{
	if (i < 0 || i >= nptrs) {
		m_syserr("illegal m_getptr");
	}
	return ptrs[i];
}
