/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id: flt_umin.c,v 1.5 1994/06/24 11:16:10 ceriel Exp $ */

#include "flt_misc.h"

void
flt_umin(e)
	flt_arith *e;
{
	/*	Unary minus
	*/
	flt_status = 0;
	e->flt_sign = ! e->flt_sign;
}
