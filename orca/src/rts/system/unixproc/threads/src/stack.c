/* Copyright (C) 1992, the Florida State University
   Distributed by the Florida State University under the terms of the
   GNU Library General Public License.

This file is part of Pthreads.

Pthreads is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation (version 2).

Pthreads is distributed "AS IS" in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with Pthreads; see the file COPYING.  If not, write
to the Free Software Foundation, 675 Mass Ave, Cambridge,
MA 02139, USA.

Report problems and direct all questions to:

  pthreads-bugs@ada.cs.fsu.edu

  @(#)stack.c	1.20 10/1/93

*/

/*
 * Thread stack allocation.
 * This handles all stack allocation including defined policies.
 */

#include "pthread_internals.h"
#include <sun4/asm_linkage.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <frame.h>

extern char *pthread_get_sp();
extern int *pthread_get_fp();
void pthread_set_sp();
void pthread_set_fp();

#ifdef STACK_CHECK
int pthread_page_size;
#ifdef SIGNAL_STACK
extern char pthread_tempstack;
static struct frame f;
static int fp_offset = (int)&f.fr_savfp - (int)&f;
static int fp_index = ((int)&f.fr_savfp - (int)&f) / sizeof(int);
#endif
#endif

/*------------------------------------------------------------*/
/*
 * pthread_stack_init - initialize the main thread's stack
 */
void pthread_stack_init(p)
pthread_t p;
{
  struct rlimit rlim;
  
#ifdef STACK_CHECK
  pthread_page_size = getpagesize();
#endif

  /*
   * Stack size of the main thread is the stack size of the process
   */
  if (getrlimit(RLIMIT_STACK, &rlim) != 0) {
    perror("getrlimit");
    pthread_process_exit(1);
  }
  p->attr.stacksize = rlim.rlim_cur;

  /*
   * dummy stack base which can be freed
   */
  p->stack_base = (char *) malloc(sizeof(int));
  process_stack_base = pthread_get_sp();
}

#ifdef STACK_CHECK
/*------------------------------------------------------------*/
/*
 * pthread_lock_stack - lock memory page on stack
 * lock page at lower bound of stack to cause segmentation violation
 * when stack overflows
 */
int pthread_lock_stack(p)
pthread_t p;
{
#ifdef SIGNAL_STACK
  if (!mprotect(PA(p->stack_base), 3 * pthread_page_size, PROT_NONE)) {
    p->state |= T_LOCKED;
    return (0);
  }
  return (-1);
#else
  return mprotect(PA(p->stack_base), pthread_page_size, PROT_NONE);
#endif
}

/*------------------------------------------------------------*/
/*
 * pthread_unlock_all_stack - unlock all pages of stack, called at thread
 *                    termination.
 */
int pthread_unlock_all_stack(p)
pthread_t p;
{
  return mprotect(PA(p->stack_base), 
#ifdef SIGNAL_STACK
                  3 * pthread_page_size,
#else
                  pthread_page_size,
#endif
                  PROT_READ | PROT_WRITE);
}
#ifdef SIGNAL_STACK
/*------------------------------------------------------------*/
/*
 * pthread_unlock_stack - unlock memory pages on stack if overflow occurs
 */
int pthread_unlock_stack(p)
pthread_t p;
{
  if (!mprotect(PA(p->stack_base)+pthread_page_size, 2 * pthread_page_size, 
			                PROT_READ | PROT_WRITE)) {
    p->state &= ~T_LOCKED;
    return (0);
  }
  return (-1);
}
#endif
#endif

/*------------------------------------------------------------*/
/*
 * pthread_alloc_stack - allocate stack space on heap for a thread
 * Stacks are deallocated when the thread has returned and is detached.
 */
int pthread_alloc_stack(p)
pthread_t p;
{
#ifndef _POSIX_THREAD_ATTR_STACKSIZE
  p->attr.stacksize = DEFAULT_STACKSIZE;
#endif
#ifdef STACK_CHECK
  p->stack_base = (char *) malloc(p->attr.stacksize + PTHREAD_BODY_OFFSET
#ifdef SIGNAL_STACK
				  + pthread_page_size * 4);
#else
                                  + pthread_page_size * 2);
#endif
#else
  p->stack_base = malloc(p->attr.stacksize + PTHREAD_BODY_OFFSET);
#endif
  
  if ((int) p->stack_base == NULL) {
#ifdef DEBUG
    fprintf(stderr, "\n*** Out of space for thread stacks. ***\n");
    fflush(stderr);
#endif
    return(FALSE);
  }
  
#ifdef STACK_CHECK
  if (pthread_lock_stack(p))
    return(FALSE);
#endif

  return(TRUE);
}

#if STACK_CHECK && SIGNAL_STACK
/*------------------------------------------------------------*/
/*
 * switch_stacks - Flushes the windows and copies the stack frames from signal 
 *   stack to thread's stack and then sets the frame pointer links correctly for
 *   thread's stack and then finally switches to thread's stack.
 */

void switch_stacks(oldsp)
int oldsp;
{
  char *sp = pthread_get_sp();
  int *fp = pthread_get_fp();
  int size = &pthread_tempstack - sp;
  int *target = (int *)(oldsp - size);
  int *user_fp = target;

  pthread_ST_FLUSH_WINDOWS();
  memcpy(target, sp, size);

  do {
    user_fp[fp_index] = (int)user_fp + (int)((char *)fp - sp);
    user_fp = (int *)user_fp[fp_index];
    sp = (char *)fp;
    fp = (int *)fp[fp_index];
  } while (fp && ((char *)fp < &pthread_tempstack));

  user_fp[fp_index] = oldsp;
  pthread_set_fp(target[fp_index]);
  pthread_set_sp(target);
}
/*------------------------------------------------------------*/
#endif
