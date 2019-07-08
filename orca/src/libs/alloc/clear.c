/* $Id: clear.c,v 1.10 1994/06/24 11:06:41 ceriel Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	clear - clear a block of memory, and try to do it fast.
*/

#include "alloc.h"

/* instead of Calloc: */

void
clear(ptr, n)
	register char *ptr;
	register unsigned int n;
{
	register long *q = (long *) ptr;

	while (n >= 8*sizeof (long))	{
			/* high-speed clear loop */
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		n -= 8*sizeof (long);
	}
	while (n >= sizeof (long))	{
			/* high-speed clear loop */
		*q++ = 0;
		n -= sizeof (long);
	}
	ptr = (char *) q;
	while (n--) *ptr++ = '\0';
}
