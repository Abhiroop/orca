/*
 * Read or write a word exclusively.
 * If this system has undivisible word reads and writes, the lock
 * operation can be skipped.
 */


#ifndef __SYS_SOLARIS_X_WORD_H__
#define __SYS_SOLARIS_X_WORD_H__


#include "pan_sync.h"

#define WORD_INDIVISIBLE		/* for this architecture */


#ifdef WORD_INDIVISIBLE
#  define READ_WORD_INDIVISIBLE
#  define WRITE_WORD_INDIVISIBLE
#endif


				/* Protect "x_from" */
/* void pan_x_read_word(word *to, word *x_from, mutex_p lock); */

#ifdef READ_WORD_INDIVISIBLE

#  define pan_x_read_word(to, x_from, lock) \
	(*(to) = *(x_from))

#else

#  define pan_x_read_word(to, x_from, lock) \
	do { \
	    pan_mutex_lock(lock); \
	    *(to) = *(x_from); \
	    pan_mutex_lock(lock); \
	} while (0);

#endif

				/* Protect "x_to" */
/* void pan_x_write_word(word *x_to, word from, mutex_p lock); */

#ifdef WRITE_WORD_INDIVISIBLE

#  define pan_x_write_word(x_to, from, lock) \
	(*(x_to) = (from))

#else

#  define pan_x_write_word(x_to, from, lock) \
	do { \
	    pan_mutex_lock(lock); \
	    *(x_to) = (from); \
	    pan_mutex_lock(lock); \
	} while (0);

#endif


#endif
