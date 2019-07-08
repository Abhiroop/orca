/* $Id: No_Mem.c,v 1.8 1994/06/24 11:06:23 ceriel Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include	<system.h>
#include	"alloc.h"

void
No_Mem()
{
	sys_write(STDERR, "Out of memory\n", 14);
	sys_stop(S_EXIT);
}
