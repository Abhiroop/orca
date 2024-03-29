/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: seek.c,v 1.3 1994/06/24 11:24:53 ceriel Exp $ */

#include <system.h>

long lseek();

int
sys_seek(fp, off, whence, poff)
	File *fp;
	long off;
	long *poff;
{
	if (! fp) return 0;
	return (*poff = lseek(fp->o_fd, off, whence)) >= 0;
}
