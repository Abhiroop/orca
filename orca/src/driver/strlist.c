/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: strlist.c,v 1.5 1995/07/31 08:57:15 ceriel Exp $ */

/*   C O M M A - S E P A R A T E D   L I S T   H A N D L I N G   */

#include "ansi.h"

#include <stdio.h>
#include <alloc.h>
#include <assert.h>

#include "strlist.h"
#include "main.h"

typedef struct strlist {
	char	*sstr;	/* string. */
	char	*strp;	/* position in string. */
} t_list, *p_list;

void *
set_strlist(str)
	char *str;
{
	p_list	p = (p_list) Malloc(sizeof(t_list));
	p->sstr = str;
	p->strp = str;
	return p;
}

int
get_strlist(aaa, bbb, vs)
	char	**aaa,
		**bbb;
	void	*vs;
{
	p_list	s = vs;
	register char *p = s->strp;

	if (! p) return 0;
	if (! *p) return 0;
	while (*p && *p != '(') {
		p++;
	}
	*aaa = Malloc(p - s->strp + 1);
	strncpy(*aaa, s->strp, p-s->strp);
	(*aaa)[p-s->strp] = 0;
	if (! *p) {
		*bbb = 0;
		s->strp = p;
		return 1;
	}
	assert(*p == '(');
	p++;
	s->strp = p;
	while (*p && *p != ')') p++;
	*bbb = Malloc(p-s->strp+1);
	strncpy(*bbb, s->strp, p-s->strp);
	(*bbb)[p-s->strp] = 0;
	if (*p != ')') {
		error("error in database string %s", s->sstr);
		s->strp = p;
	}
	else {
		p++;
		if (*p == ',') s->strp = p+1; else s->strp = p;
	}
	return 1;
}
