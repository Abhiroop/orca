/*
  (c) copyright 1989 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id: ucmp.c,v 1.5 1994/06/24 11:16:19 ceriel Exp $ */

#include "flt_misc.h"

int
ucmp(l1,l2)
	long l1,l2;
{
	if (l1 == l2) return 0;
	if (l2 >= 0) {
		if (l1 > l2 || l1 < 0) return 1;
		return -1;
	}
	if (l1 >= 0 || l1 < l2) return -1;
	return 1;
}
