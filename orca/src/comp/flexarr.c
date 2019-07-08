/*
 * (c) copyright 1997 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: flexarr.c,v 1.1 1997/05/15 12:02:00 ceriel Exp $ */

#include <alloc.h>

#include "flexarr.h"

#define INITSZ	4

p_flex flex_init(elsz, initsz)
    size_t  elsz;
    int	    initsz;
{
    p_flex  f = (p_flex) Malloc(sizeof(t_flex));

    f->elsz = elsz;
    f->maxused = -1;
    f->cursz = initsz ? initsz : INITSZ;
    f->ptr = Malloc(f->elsz * f->cursz);
    return f;
}

void *flex_finish(f, psz)
    p_flex  f;
    uint    *psz;
{
    void    *p;

    if (f->cursz > f->maxused+1) {
	f->cursz = f->maxused+1;
	f->ptr = Realloc(f->ptr, f->elsz * f->cursz);
    }
    p = f->ptr;
    if (psz) *psz = f->cursz;
    free(f);
    return p;
}

void *flex_index(f, ind)
    p_flex  f;
    uint    ind;
{
    if (ind >= f->cursz) {
	while (ind >= f->cursz) {
	    f->cursz += f->cursz;
	}
	/* Using the fact that Realloc is Malloc when the first arg == 0. */
	f->ptr = Realloc(f->ptr, f->cursz * f->elsz);
    }
    if ((int) ind > f->maxused) {
	f->maxused = ind;
    }
    return (char *)(f->ptr) + ind * f->elsz;
}
