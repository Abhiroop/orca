/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: time.c,v 1.1 1999/10/11 14:23:45 ceriel Exp $ */

#include <system.h>

long time();

long
sys_time()
{
	return time((long *) 0);
}
