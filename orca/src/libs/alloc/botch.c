/* $Id: botch.c,v 1.9 1994/06/24 11:06:39 ceriel Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	botch - write garbage over a chunk of memory, useful if you want
		to check if freed memory is used inappopriately.
*/

#include "alloc.h"

void
botch(ptr, n)
	register char *ptr;
	register unsigned int n;
{
	while (n >= sizeof (long))	{	
			/* high-speed botch loop */
		*(long *)ptr = 025252525252L;
		ptr += sizeof (long), n -= sizeof (long);
	}
	while (n--) *ptr++ = '\252';
}
