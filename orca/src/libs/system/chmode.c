/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: chmode.c,v 1.4 1994/06/24 11:24:13 ceriel Exp $ */

#include <system.h>

int
sys_chmode(path, mode)
	char *path;
	int mode;
{
	return chmod(path, mode) == 0;
}
