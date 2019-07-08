/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: remove.c,v 1.4 1994/06/24 11:24:46 ceriel Exp $ */

#include <system.h>

int
sys_remove(path)
	char *path;
{
	return unlink(path) == 0;
}
