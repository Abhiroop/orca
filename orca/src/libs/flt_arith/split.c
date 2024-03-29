/* $Id: split.c,v 1.5 1994/06/24 11:16:16 ceriel Exp $ */

#include "flt_misc.h"

void
flt_split(e, p)
	register flt_arith *e;
	register unsigned short *p;
{
	/*	Split mantissa of e into the array p
	*/

	p[0] = (int)(e->m1 >> 16) & 0xFFFF;
	p[1] = (int)(e->m1) & 0xFFFF;
	p[2] = (int)(e->m2 >> 16) & 0xFFFF;
	p[3] = (int)(e->m2) & 0xFFFF;
}
