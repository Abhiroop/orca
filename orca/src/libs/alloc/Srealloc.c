/* $Id: Srealloc.c,v 1.4 1994/06/24 11:06:33 ceriel Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	M E M O R Y  A L L O C A T I O N  R O U T I N E S	*/

/*	The memory allocation routines offered in this file are:

	char *Srealloc(ptr, n)	: reallocate buffer to n bytes
*/

#include	"alloc.h"

char *
Srealloc(str, sz)
	char str[];
	unsigned int sz;
{
	return Realloc(str, sz);
}
