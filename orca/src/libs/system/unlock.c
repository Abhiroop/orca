/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: unlock.c,v 1.4 1994/06/24 11:25:08 ceriel Exp $ */

#include <system.h>

int
sys_unlock(path)
	char *path;
{
	return unlink(path) == 0;
}
