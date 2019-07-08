/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __PROT_STACK_H__
#define __PROT_STACK_H__



#define alignof(type)	(sizeof(struct{char i; type c;}) - sizeof(type))
#define do_align(s,a)	(((s)+(a)-1) & ~((a) - 1))
#define ps_align(s)	do_align(s,sizeof(double))


static void *
rts_ps_push( void *msg, int *len, int data_size)
{
    void *p;

    p = (char *)msg + *len;
    *len += ps_align(data_size);

    return p;
}

static void *
rts_ps_pop( void *msg, int *len, int data_size)
{
    void *buf;

    *len -= ps_align(data_size);
    buf = (char *)msg + *len;

    return buf;
}

#endif
