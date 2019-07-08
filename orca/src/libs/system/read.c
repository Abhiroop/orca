/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: read.c,v 1.4 1994/06/24 11:24:43 ceriel Exp $ */

#include <system.h>

int
sys_read(fp, bufptr, bufsiz, pnbytes)
	File *fp;
	char *bufptr;
	int bufsiz, *pnbytes;
{
	if (! fp) return 0;
	return (*pnbytes = read(fp->o_fd, bufptr, bufsiz)) >= 0;
}
