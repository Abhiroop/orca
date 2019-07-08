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

  @(#)pthread_init.c	1.20 10/1/93

*/

#include "pthread_internals.h"
#include <sun4/asm_linkage.h>
#include <sun4/frame.h>

extern pthread_body();
#ifdef STACK_CHECK
extern int pthread_page_size;
#endif

/*
 * additional parameters passed on stack by fake call
 */
#ifdef C_CONTEXT_SWITCH
#define PARAM_OFFSET 8
#else
#define PARAM_OFFSET 0
#endif

/*
 * Stack layout:
 * high +-----------+ + +
 *      |           | | | WINDOWSIZE (prev. window for callee save area)
 *      +-----------+ | +
 *      |           | PTHREAD_BODY_OFFSET (space for pthread_body())
 *      |           | |
 *      +-----------+ + start of user stack space
 *      |           | |
 *      |           | |
 *      |           | |
 *      |           | |
 *      |           | attr.stacksize (space for user functions)
 *      |           | |
 *      |           | |
 *      |           | |
 *      |           | |
 *      +-----------+ + end of user stack space
 *      | --------- | | +
 *      |  locked   | | pthread_page_size: locked page, bus error on overflow
 *      | --------- | | + PA(stack_base): next smallest page aligned address
 *      |           | 2*pthread_page_size
 *      |           | |
 * low  +-----------+ + stack_base
 *
 * ifdef STACK_CHECK:
 * Lock the closest page to the end of the stack to cause a bus error
 * (or and illegal instruction if no signal handler cannot be pushed
 * because the stack limit is exceeded, causes bad signal stack message)
 * on stack overflow. We allocate space conservatively (two pages) to
 * ensure that the locked page does not cross into the allocated stack
 * stack due to page alignment. Notice that only whole pages can be
 * locked, i.e. smaller amounts will be extended to the next page
 * boundary.
 */

/*------------------------------------------------------------*/
/*
 * pthread_initialize - associate stack space with thread
 */
void pthread_initialize(t)
pthread_t t;
{
  struct frame *sp;

#ifdef STACK_CHECK
  sp = (struct frame *) (t->stack_base + 
#ifdef SIGNAL_STACK
     pthread_page_size * 4 +
#else
     pthread_page_size * 2 +
#endif
     SA(t->attr.stacksize + PTHREAD_BODY_OFFSET - WINDOWSIZE));

  sp->fr_savfp = (struct frame *) (t->stack_base + 
#ifdef SIGNAL_STACK
     pthread_page_size * 4 +
#else
     pthread_page_size * 2 +
#endif
     SA(t->attr.stacksize + PTHREAD_BODY_OFFSET));
#else  
  sp = (struct frame *)
    (t->stack_base + SA(t->attr.stacksize + PTHREAD_BODY_OFFSET - WINDOWSIZE));
  sp->fr_savfp = (struct frame *)
    (t->stack_base + SA(t->attr.stacksize + PTHREAD_BODY_OFFSET));
#endif
  /*
   * set up a jump buffer environment, then manipulate the pc and stack
   */
#ifdef C_CONTEXT_SWITCH
  sigsetjmp(&t->context, FALSE);
#endif
  t->context.sp = (int) sp;
  t->context.pc = (int) pthread_body - RETURN_OFFSET;
}

/*------------------------------------------------------------*/
/*
 * pthread_push_fake_call - push a user handler for a signal on some thread's 
 * stack. The user handler will be the first code executed when control
 * returns to the thread.
 */
void pthread_push_fake_call(p, handler_add, sig, scp, temp_mask)
     pthread_t p;
     void (*handler_add)();
     int sig;
     struct context_t *scp;
     sigset_t temp_mask;
{
#ifdef C_CONTEXT_SWITCH
  extern void pthread_fake_call_wrapper_wrapper();
#else
  extern void pthread_fake_call_wrapper();
#endif
  struct frame *framep;
  int new_context = scp == (struct context_t *) DIRECTED_AT_THREAD;

  /*
   * create a new frame for the wrapper, and bind sp of the new 
   * frame to the frame structure.
   */
  if (new_context) {                /* init context structure if neccessary */
                                    /* allocate space on stack */
    scp = (struct context_t *) (p->context.sp -	SA(sizeof(struct context_t)));
    /*
     * need space for new window and 2 params on stack
     */
    framep = (struct frame *) ((int) scp - SA(MINFRAME + PARAM_OFFSET));
    scp->sc_mask = p->mask;
    scp->sc_sp = p->context.sp;
    /*
     * The offset will be substracted in the wrapper to hide this issue
     * from the user (in case he changes the return address)
     */
    scp->sc_pc = p->context.pc + RETURN_OFFSET;
  }
  else
    framep = (struct frame *) (p->context.sp - SA(MINFRAME + PARAM_OFFSET));

  framep->fr_savfp = (struct frame *) p->context.sp;
                                    /* put fp in saved area of i6. */
  framep->fr_savpc = p->context.pc;
                                    /* put pc in saved area of i7. */
  framep->fr_arg[0] = (int) handler_add;
                                    /* save handler's address as i0. */
  framep->fr_arg[1] = p->mask;      /* save signal mask as i1. */
  framep->fr_arg[2] = sig;          /* arg0 to user handler in i2. */
  framep->fr_arg[3] = (int) &p->sig_info[sig == -1 ? 0 : sig];
				    /* arg1 to user handler in i3. */
  framep->fr_arg[4] = (int) scp;    /* arg2 to user handler in i4. */
  framep->fr_arg[5] = new_context;  /* used by wrapper to restore context */
#ifdef C_CONTEXT_SWITCH
  framep->fr_argx[0] = (int) p->scp;   /* old context pointer on stack */
  framep->fr_argx[1] = (int) &p->cond; /* addr. of inter. cond. wait on stack */
#else
  framep->fr_local[6] = (int) &p->cond; /* addr. of inter. cond. wait in l6 */
  framep->fr_local[7] = (int) p->scp;   /* old context pointer in l7 */
#endif
  p->context.sp = (int) framep;     /* store new sp */
#ifdef C_CONTEXT_SWITCH
  p->context.pc = 
    (int) pthread_fake_call_wrapper_wrapper - RETURN_OFFSET; /* store new pc */
#else
  p->context.pc = 
    (int) pthread_fake_call_wrapper - RETURN_OFFSET;         /* store new pc */
#endif
  p->mask |= temp_mask;     /* update the per-thread mask. */
}
/*------------------------------------------------------------*/
