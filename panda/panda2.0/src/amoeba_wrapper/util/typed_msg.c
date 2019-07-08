/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <string.h>

#include "pan_util.h"

/* This module implements marshalling functions for basic
   C types and Panda types.
*/


#ifdef NO
void
tm_push_bytes(pan_msg_p msg, byte_p bytes, int len)
{
    byte_p p;

    p = pan_msg_push(msg, len, alignof(byte_t));
    memcpy(p, bytes, len);
    tm_push_int(msg, len);
}


void
tm_pop_bytes(pan_msg_p msg, byte_p buf, int *len)
{
    tm_pop_int(msg, len);
    memcpy(buf, pan_msg_pop(msg, *len, alignof(byte_t)), *len);
}


void
tm_look_bytes(pan_msg_p msg, byte_p buf, int *len)
{
    tm_pop_int(msg, len);
    memcpy(buf, pan_msg_look(msg, *len, alignof(byte_t)), *len);
    tm_push_int(msg, *len);
}
#endif



/* Raoul: maybe turn the common ones into macros to speed things up.
 */


void
tm_push_int(pan_msg_p msg, int n)
{
    int *p;

    p = pan_msg_push(msg, sizeof(int), alignof(int));
    *p = n;
}


void
tm_pop_int(pan_msg_p msg, int *n)
{
    *n = *(int *)pan_msg_pop(msg, sizeof(int), alignof(int));
}


void
tm_look_int(pan_msg_p msg, int *n)
{
    *n = *(int *)pan_msg_look(msg, sizeof(int), alignof(byte_t));
}


void
tm_push_ulong(pan_msg_p msg, unsigned long n)
{
    unsigned long *p;

    p = pan_msg_push(msg, sizeof(unsigned long), alignof(unsigned long));
    *p = n;
}


void
tm_pop_ulong(pan_msg_p msg, unsigned long *n)
{
    *n = *(unsigned long *)pan_msg_pop(msg, sizeof(unsigned long),
					alignof(unsigned long));
}


void
tm_push_double(pan_msg_p msg, double n)
{
    double *p;

    p = pan_msg_push(msg, sizeof(double), alignof(double));
    *p = n;
}


void
tm_pop_double(pan_msg_p msg, double *n)
{
    *n = *(double *)pan_msg_pop(msg, sizeof(double), alignof(byte_t));
}


void
tm_look_double(pan_msg_p msg, double *n)
{
    *n = *(double *)pan_msg_look(msg, sizeof(double), alignof(byte_t));
}


void
tm_push_pset(pan_msg_p msg, pan_pset_p pset)
{
    void       *p;
    int         size;

    size = pan_pset_size();
    p = pan_msg_push(msg, size, alignof(byte_t));
    pan_pset_marshall(pset, p);
    tm_push_int(msg, size);
}


void
tm_pop_pset(pan_msg_p msg, pan_pset_p pset)
{
    void       *p;
    int         size;

    tm_pop_int(msg, &size);
    p = pan_msg_pop(msg, size, alignof(byte_t));
    pan_pset_unmarshall(pset, p);
}


void
tm_look_pset(pan_msg_p msg, pan_pset_p pset)
{
    void       *p;
    int         size;

    tm_pop_int(msg, &size);
    p = pan_msg_look(msg, size, alignof(byte_t));
    pan_pset_unmarshall(pset, p);
    tm_push_int(msg, size);
}


void
tm_push_time(pan_msg_p msg, pan_time_p time)
{
    pan_time_marshall(time,
		      pan_msg_push(msg, pan_time_size(), alignof(byte_t)));
}


void
tm_pop_time(pan_msg_p msg, pan_time_p time)
{
    pan_time_unmarshall(time,
			pan_msg_pop(msg, pan_time_size(), alignof(byte_t)));
}


void
tm_look_time(pan_msg_p msg, pan_time_p time)
{
    pan_time_unmarshall(time,
			pan_msg_look(msg, pan_time_size(), alignof(byte_t)));
}


void
tm_push_ptr(pan_msg_p msg, void *ptr)
{
    void **p;

    p = pan_msg_push(msg, sizeof(void *), alignof(void *));
    *p = ptr;
}


void
tm_pop_ptr(pan_msg_p msg, void **ptr)
{
    *ptr = *(void **)pan_msg_pop(msg, sizeof(void *), alignof(void *));
}


void
tm_look_ptr(pan_msg_p msg, void **ptr)
{
    *ptr = *(void **)pan_msg_look(msg, sizeof(void *), alignof(void *));
}

/* #ifdef THREAD_VISIBLE */

void
tm_push_tid(pan_msg_p msg, pan_thread_p tid)
{
    pan_thread_p *p;

    p = pan_msg_push(msg, sizeof(pan_thread_p), alignof(pan_thread_p));
    *p = tid;
}


void
tm_pop_tid(pan_msg_p msg, pan_thread_p *tid)
{
    *tid = *(pan_thread_p *)pan_msg_pop(msg, sizeof(pan_thread_p),
					alignof(pan_thread_p));
}


void
tm_look_tid(pan_msg_p msg, pan_thread_p *tid)
{
    *tid = *(pan_thread_p *)pan_msg_look(msg, sizeof(pan_thread_p),
					 alignof(pan_thread_p));
}

/* #endif */
