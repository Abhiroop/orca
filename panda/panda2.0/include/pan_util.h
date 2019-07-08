/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_UTIL_H__
#define __PAN_UTIL_H__

#include "pan_sys.h"

typedef unsigned char byte_t, *byte_p;

void pan_util_init(void);
void pan_util_end(void);

/* This module exports functions that allow one to pack typed
   data conveniently into a message. The exported functions
   cover the basic C and Panda types. 

   When the destination argument for a pop operation is a
   null pointer, the data is popped, but not copied to the
   destination.
*/

#ifdef NO
void tm_push_bytes(pan_msg_p msg, byte_p bytes, int len);

void tm_pop_bytes(pan_msg_p msg, byte_p buf, int *len);

void tm_look_bytes(pan_msg_p msg, byte_p bytes, int *len);
#endif

void tm_push_int(pan_msg_p msg, int n);

void tm_pop_int(pan_msg_p msg, int *n);

void tm_look_int(pan_msg_p msg, int *n);

void tm_push_ulong(pan_msg_p msg, unsigned long n);

void tm_pop_ulong(pan_msg_p msg, unsigned long *n);

void tm_push_double(pan_msg_p msg, double n);

void tm_pop_double(pan_msg_p msg, double *n);

void tm_look_double(pan_msg_p msg, double *n);

void tm_push_pset(pan_msg_p msg, pan_pset_p pset);

void tm_pop_pset(pan_msg_p msg, pan_pset_p pset);

void tm_look_pset(pan_msg_p msg, pan_pset_p pset);

void tm_push_time(pan_msg_p msg, pan_time_p time);

void tm_pop_time(pan_msg_p msg, pan_time_p time);

void tm_look_time(pan_msg_p msg, pan_time_p time);

void tm_push_ptr(pan_msg_p msg, void *ptr);

void tm_pop_ptr(pan_msg_p msg, void **ptr);

void tm_look_ptr(pan_msg_p msg, void **ptr);

void tm_push_tid(pan_msg_p msg, pan_thread_p tid);

void tm_pop_tid(pan_msg_p msg, pan_thread_p *tio);

void tm_look_tid(pan_msg_p msg, pan_thread_p *tid);



/* Endianness test */

int  pan_is_bigendian(void);



/* Clock sync module. */

/* Global sync. All platforms must call this function "at the same time" -----*/

int         pan_clock_sync(int n_syncs, pan_time_p shift, pan_time_p std);

/*-------------- Extra handles for operation/enquiry -------------------------*/

void        pan_clock_set_timeout(pan_time_p timeout, pan_time_p uc_delay);

void        pan_clock_get_timeout(pan_time_p timeout, pan_time_p uc_delay);

int         pan_clock_get_shift(pan_time_p timeout, pan_time_p d_timeout);



/* Fixed time representation. */

pan_time_fix_t pan_time_fix_infinity;
pan_time_fix_t pan_time_fix_zero;

#ifdef NO_FIX_TIME_MACROS

int    pan_time_fix_cmp(pan_time_fix_p t1, pan_time_fix_p t2);

void   pan_time_fix_add(pan_time_fix_p res, pan_time_fix_p delta);

void   pan_time_fix_sub(pan_time_fix_p res, pan_time_fix_p delta);

#else	/* NO_FIX_TIME_MACROS */


#define NANO	1000000000

#define pan_time_fix_cmp(t1, t2) \
		((t1)->t_sec == (t2)->t_sec ? \
		    ((t1)->t_nsec == (t2)->t_nsec ? 0 \
			: ((t1)->t_nsec > (t2)->t_nsec ? 1 \
			    : -1)) \
		    : ((t1)->t_sec > (t2)->t_sec ? 1 \
			: -1))

#define pan_time_fix_add(res, delta) \
		do { \
		    (res)->t_nsec += (delta)->t_nsec; \
		    (res)->t_sec += (delta)->t_sec; \
		    if ((res)->t_nsec > NANO) { \
			(res)->t_nsec -= NANO; \
			++(res)->t_sec; \
		    } \
		} while (0)

#define pan_time_fix_sub(res, delta) \
		do { \
		    (res)->t_nsec -= (delta)->t_nsec; \
		    (res)->t_sec -= (delta)->t_sec; \
		    if ((res)->t_nsec < 0) { \
			(res)->t_nsec += NANO; \
			--(res)->t_sec; \
		    } \
		} while (0)


#endif	/* NO_FIX_TIME_MACROS */


void   pan_time_fix_mul(pan_time_fix_p res, int nr);

void   pan_time_fix_div(pan_time_fix_p res, int nr);

void   pan_time_fix_mulf(pan_time_fix_p res, double nr);

void   pan_time_fix_d2t(pan_time_fix_p t, double d);

double pan_time_fix_t2d(pan_time_fix_p t);


/* strdup, which is not included in ansi C */

char  *strdup(const char *s1);

/* sleep in Panda speak. sleep() is not included in ansi C */

void   pan_sleep(pan_time_p t);

#endif
