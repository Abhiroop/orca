/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_SYSTEM_DEBUG__
#define __PANDA_SYSTEM_DEBUG__

int pan_malloc_dump(int dump);
/*
 Sets a switch to dump every call to malloc, calloc, realloc, and
 free. Returns the old position. Only available if compiled with
 -DMALLOC_DUMP.
 */

void pan_malloc_statistics(void);
/*
 Dump the malloc statistics. Only available if not compiled with -DNDEBUG.
 */

#endif /* __PANDA_SYSTEM_DEBUG__ */
