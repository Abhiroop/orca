/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: access.c,v 1.3 1994/06/24 11:24:07 ceriel Exp $ */

#include <system.h>

int
sys_access(path, mode)
	char *path;
	int mode;
{
	return access(path, mode) == 0;
}
