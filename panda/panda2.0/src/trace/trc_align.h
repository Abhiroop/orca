/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Very simplest definition of alignment: an object is aligned on a
 * multiple of its size. This should work fine at least for non-structured
 * objects.
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */

#ifndef _TRACE_TRC_ALIGN_H
#define _TRACE_TRC_ALIGN_H

#include <stddef.h>



 /* void *ptr_upround(void *, size_t); */
#ifndef ptr_upround
#define ptr_upround(p, size) \
	((void *)((size) * (((long unsigned int)(p) + (size) - 1) / (size))))
#endif

 /* char *ptr_align(obj_type *, type); */
#define ptr_align(p, obj_size) \
	    ((char *)(ptr_upround((p), obj_size)))


/* Run-time measurement of alignments that should always work */

size_t      f_align(void);	/* double */

size_t      d_align(void);	/* int */

size_t      p_align(void);	/* void* */

size_t      h_align(void);	/* short int */

size_t      l_align(void);	/* long int */

size_t      c_align(void);	/* char */


 /* int upround(int i, int base); */
#define upround(i, base)	(((i) + (base) - 1) / (base))

#endif
