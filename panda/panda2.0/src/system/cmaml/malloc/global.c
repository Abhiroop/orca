/* $Header: /usr/proj/panda/Repositories/gnucvs/panda2.0/src/system/cmaml/malloc/global.c,v 1.1 1996/03/08 10:59:20 tim Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include	"param.h"
#include	"impl.h"

/*	The only global data item:
*/
mallink *ml_last;		/* link to the world */
