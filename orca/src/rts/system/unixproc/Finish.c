/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: Finish.c,v 1.6 1996/07/04 08:54:05 ceriel Exp $ */

#include <interface.h>
#include <stdlib.h>
#include "Finish.h"
#include "unixproc.h"
void f_Finish__Finish(void) { end_procs(); exit(0); }
