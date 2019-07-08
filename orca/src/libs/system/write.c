/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: write.c,v 1.4 1994/06/24 11:25:11 ceriel Exp $ */

#include <system.h>

int
sys_write(fp, bufptr, nbytes)
	File *fp;
	char *bufptr;
	int nbytes;
{
	if (! fp) return 0;
	return write(fp->o_fd, bufptr, nbytes) == nbytes;
}
