/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: list.c,v 1.3 1995/07/31 08:53:58 ceriel Exp $ */

#include "ansi.h"
#include "list.h"

int
listmember(l, e)
	p_list	l;
	void	*e;
{
	p_list	le = l;

	if (! le) return 0;
	for (;;) {
		if (e == le->l_elem) return 1;
		le = le->l_next;
		if (! le || le == l) break;
	}
	return 0;
}

int
dbllistmember(l, e)
	p_dbl_list
		l;
	void	*e;
{
	p_dbl_list
		le = l;

	if (! le) return 0;
	for (;;) {
		if (e == le->dbl_elem) return 1;
		le = le->dbl_next;
		if (! le || le == l) break;
	}
	return 0;
}
