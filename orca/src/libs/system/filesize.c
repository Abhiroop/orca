/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: filesize.c,v 1.4 1994/06/24 11:24:24 ceriel Exp $ */

#include <sys/types.h>
#include <sys/stat.h>
#include <system.h>

long
sys_filesize(path)
	char *path;
{
	struct stat st_buf;

	if (stat(path, &st_buf) != 0)
		return -1L;
	return (long) st_buf.st_size;
}
