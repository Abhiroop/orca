/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* B I T   S E T   M A N I P U L A T I O N */

/* $Id: sets.c,v 1.5 1997/05/15 12:02:56 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

#include	"sets.h"

int set_assign(result, src, sz)
	p_set	result, src;
	int	sz;
{
	int	i = sz - 1;
	p_set	p = &src[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = *p--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

int set_union(result, s1, s2, sz)
	p_set	result, s1, s2;
	int	sz;
{
	int	i = sz - 1;
	p_set	p1 = &s1[i],
		p2 = &s2[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = *p1-- | *p2--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

int set_intersect(result, s1, s2, sz)
	p_set	result, s1, s2;
	int	sz;
{
	int	i = sz - 1;
	p_set	p1 = &s1[i],
		p2 = &s2[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = *p1-- & *p2--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

int set_minus(result, s1, s2, sz)
	p_set	result, s1, s2;
	int	sz;
{
	int	i = sz - 1;
	p_set	p1 = &s1[i],
		p2 = &s2[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = *p1-- & ~ *p2--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

void set_init(s, full, sz)
	p_set	s;
	int	full, sz;
{
	int	i = sz - 1;
	p_set	p = &s[i];

	if (full) full = 0377;
	for (; i >= 0; i--) {
		*p-- = full;
	}
}

int set_Xunion(result, s1, s2, sz)
	p_set	result, s1, s2;
	int	sz;
{
	int	i = sz - 1;
	p_set	p1 = &s1[i],
		p2 = &s2[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = (~*p1--) | *p2--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

int set_Xintersect(result, s1, s2, sz)
	p_set	result, s1, s2;
	int	sz;
{
	int	i = sz - 1;
	p_set	p1 = &s1[i],
		p2 = &s2[i],
		r = &result[i];
	int	retval = 0;

	for (; i >= 0; i--) {
		unsigned char c = (~*p1--) & *p2--;

		if (*r != c) retval = 1;
		*r-- = c;
	}
	return retval;
}

int set_isempty(s, sz)
	p_set	s;
	int	sz;
{
	int	i = sz - 1;
	p_set	p = &s[i];

	for (; i >= 0; i--) {
		if (*p--) return 0;
	}
	return 1;
}
