/* $Header: /usr/proj/panda/Repositories/gnucvs/panda2.0/src/system/cmaml/malloc/getsize.c,v 1.1 1996/03/08 10:59:18 tim Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	find out if a pointer-sized integer, preferably unsigned,
	must be declared as an unsigned int or a long
*/

#include <stdio.h>

void
main(int argc, char **argv)
{
	if (sizeof(unsigned int) == sizeof(char *)) {
		puts("typedef unsigned int size_type;");
		exit(0);
	}
	if (sizeof(long) == sizeof(char *)) {
		puts("typedef long size_type;");
		exit(0);
	}
	fputs("funny pointer size\n", stderr);
	exit(1);
}
