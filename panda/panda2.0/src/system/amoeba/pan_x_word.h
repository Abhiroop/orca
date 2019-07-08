/*
 * Read or write a word exclusively.
 * If this system has undivisible word reads and writes, the lock
 * operation can be skipped.
 */


#ifndef __SYS_AMOEBA_X_WORD_H__
#define __SYS_AMOEBA_X_WORD_H__


#include "sync.h"

#define WORD_INDIVISIBLE		/* for this architecture */


#ifdef WORD_INDIVISIBLE
#  define READ_WORD_INDIVISIBLE
#  define WRITE_WORD_INDIVISIBLE
#endif


				/* Protect "x_from" */
/* void sys_x_read_word(word *to, word *x_from, mutex_p lock); */

#ifdef READ_WORD_INDIVISIBLE

#  define sys_x_read_word(to, x_from, lock) \
	(*(to) = *(x_from))

#else

#  define sys_x_read_word(to, x_from, lock) \
	do { \
	    sys_mutex_lock(lock); \
	    *(to) = *(x_from); \
	    sys_mutex_lock(lock); \
	} while (0);

#endif

				/* Protect "x_to" */
/* void sys_x_write_word(word *x_to, word from, mutex_p lock); */

#ifdef WRITE_WORD_INDIVISIBLE

#  define sys_x_write_word(x_to, from, lock) \
	(*(x_to) = (from))

#else

#  define sys_x_write_word(x_to, from, lock) \
	do { \
	    sys_mutex_lock(lock); \
	    *(x_to) = (from); \
	    sys_mutex_lock(lock); \
	} while (0);

#endif


#endif
