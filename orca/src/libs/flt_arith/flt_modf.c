/*
  (c) copyright 1989 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id: flt_modf.c,v 1.6 1994/06/24 11:15:58 ceriel Exp $ */

#include "flt_misc.h"

void
flt_modf(e, ipart, fpart)
	register flt_arith *e, *ipart, *fpart;
{
	if (e->flt_exp < 0) {
		*fpart = *e;
		ipart->flt_sign = 0;
		ipart->flt_exp = 0;
		ipart->m1 = ipart->m2 = 0;
		return;
	}
	if (e->flt_exp >= 63) {
		fpart->flt_sign = 0;
		fpart->flt_exp = 0;
		fpart->m1 = fpart->m2 = 0;
		*ipart = *e;
		return;
	}
	*ipart = *e;
	/* "loose" low order bits */
	flt_b64_sft(&(ipart->flt_mantissa), 63 - e->flt_exp);
	flt_b64_sft(&(ipart->flt_mantissa), e->flt_exp - 63);
	flt_sub(e, ipart, fpart);
}
