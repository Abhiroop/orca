/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: rename.c,v 1.3 1994/06/24 11:24:49 ceriel Exp $ */

#include <system.h>

int
sys_rename(path1, path2)
	char *path1, *path2;
{
	unlink(path2);
	return	link(path1, path2) == 0 &&
		unlink(path1) == 0;
}

