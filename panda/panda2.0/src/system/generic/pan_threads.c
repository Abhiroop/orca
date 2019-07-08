/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
#include "pan_threads.h"

#define DEFAULT_STACKSIZE 65536
#define DEFAULT_PRIORITY      0

/* 
  Should be called before any other routine in this module. 
  Initialises some data structures.
*/
void pan_sys_thread_start(void)
{
}


/* 
  Should be called to end thread usage
*/
void
pan_sys_thread_end(void)
{
}


pan_thread_p
pan_thread_create(void (*func)(void *arg), void *arg, long stacksize, 
		  int priority, int detach)
{
}


void
pan_thread_clear(pan_thread_p thread)
{
}


void
pan_thread_exit()
{
}


void
pan_thread_join(pan_thread_p thread)
{
}


void
pan_thread_yield(void)
{
}


pan_thread_p
pan_thread_self(void)
{
}


int
pan_thread_getprio(void)
{
}


int
pan_thread_setprio(int priority)
{
}

      
int
pan_thread_minprio(void)
{
}


int
pan_thread_maxprio(void)
{
}
