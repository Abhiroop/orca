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

  @(#)pthread_sched.c	1.17 7/22/93

*/


/*
 * Pseudo-code for pthread_sched.S -- does not compile!
 */

#include <errno.h>
#include <sun4/trap.h>
#define KERNEL
#include <sun4/asm_linkage.h>
#undef KERNEL
#include "pthread_asm.h"
#include "pthread_offsets.h"
#define SIG_SETMASK 4

/*------------------------------------------------------------*/
/* 
 * pthread_sched - dispatcher
 */
void pthread_sched()
{
  /*
   * temporary stack for dispatcher, handle_many_process_signals,
   * N nested function calls by handle_many_process_signals,
   * a possible sigtramp and universal handler, plus 1 window spare
   */
#ifdef DEBUG
  char pthread_tempstack[54*MINFRAME+WINDOWSIZE];
#else
  char pthread_tempstack[104*MINFRAME+WINDOWSIZE];
#endif
  int oldsp, context_saved;
  pthread_t old;

#ifdef NO_INLINE
  is_in_kernel = 0;
  if (state_change)
    goto state_changed;
  retl;
 state_changed:
#endif
  SET_KERNEL_FLAG;
  save;
 pthread_sched_no_save:
  old = pthread_self();

  if (old->state & T_RETURNED) {
    ST_FLUSH_WINDOWS();
    if (old->state & T_DETACHED) {
 dealloc:
      free(old->stack_base);
      free(old);
    }
  }
  else {
    new = ready.head;
    if (old == new && !new_signals)
      goto no_switch;

    ST_FLUSH_WINDOWS();
    old->errno = errno;
    old->context[SP_OFFSET] = sp;
    old->context[PC_OFFSET] = pc;
  }

  new = ready.head;
  if (new == old)
    goto handle_pending_signals;
  if (!new)
    goto no_threads;

 restore_new:
  errno = new->errno;
  sp = new->context[SP_OFFSET];
  pc = new->context[PC_OFFSET];
  pthread_self() = new;

 no_switch:

#ifdef DEF_RR
  if (new->attr.sched == SCHED_RR)
    timed_sigwait(new, NULL, RR_TIME);
#endif

  if (pc == called_from_sighandler)
    sigsetmask(S_DISABLE);

  restore;

  is_in_kernel = FALSE;
  state_change = FALSE;

  if (!new_signals)
    retl;
  else {
    save;

    SET_KERNEL_FLAG;
    old = pthread_self();

    ST_FLUSH_WINDOWS();
    old->errno = errno;
    old->context[SP_OFFSET] = sp;
    old->context[PC_OFFSET] = pc;

    if (pc == called_from_sighandler)
      sigsetmask(S_ENABLE);
    
  no_threads:
    oldsp = sp;
    sp = &pthread_tempstack;
    new = handle_many_process_signals();
    old = new;
    goto restore_stack;

  handle_pending_signals:
    oldsp = sp;
    sp = &pthread_tempstack;
    new = handle_many_process_signals();
  restore_stack:
    sp = oldsp;
    
    if (old != new && old->state & T_RETURNED && old->state & T_DETACHED)
      goto dealloc;
    else
      goto restore_new;
  }
}

/*------------------------------------------------------------*/
/*
 * pthread_sched_wrapper - 
 */
void pthread_sched_wrapper(sig, code)
int sig, code;
{
  save;

 called_from_sighandler:
  pthread_signal_sched(sig, code);

  return;
}

/*------------------------------------------------------------*/
/*
 * fake_call_wrapper - 
 */
void fake_call_wrapper(user_handler, sigmask, sig, infop, scp, restore_context,
		       oscp, cond)
void (*user_handler)();
sigset_t sigmask;
int sig;
struct siginfo *infop;
struct sigconext *scp, *oscp;
int restore_context;
pthread_cond_t *cond;
{
  pthread_t new, p;
  sigset_t omask = scp->sc_mask;

  int saved_errno = errno;

  if (*cond)
    pthread_cond_wait_terminate();

  (*user_handler)(sig, infop, scp);

  SET_KERNEL_FLAG;
  errno = saved_errno;

  if (restore_context) {
    sigmask = scp->sc_mask;
    sp = scp->sc_sp;
    pc = scp->sc_pc - RETURN_OFFSET;
    goto context_restored;
  }
  else if (omask != scp->sc_mask) {
    sigmask = scp->sc_mask;
    scp->sc_mask = omask;
  }

  p = pthread_self();
  p->mask = sigmask;

  if ((p->pending | pending_signals) & ~sigmask)
    handle_pending_signals_wrapper(FALSE); /* never returns from call */

  p->nscp = oscp;
  new = ready.head;
  goto pthread_sched_no_save;
}
          
/*------------------------------------------------------------*/
/*
 * handle_pending_signals_wrapper - 
 */
void handle_pending_signals_wrapper(initial_save)
int initial_save;
{
  pthread_t new, old = pthread_self();
  int oldsp;

  if (initial_save)
    save;

  ST_FLUSH_WINDOWS();
  old->errno = errno;
  old->context[SP_OFFSET] = sp;
  old->context[PC_OFFSET] = pc;

  if (!initial_save)
    save;

  oldsp = &pthread_tempstack;
  handle_pending_signals();

  new = ready.head;
  goto restore_stack;
}

/*------------------------------------------------------------*/
/*
 * pthread_signal_sched - 
 */
void pthread_signal_sched(sig, code)
int sig, code;
{
  pthread_t new, old = pthread_self();
  int oldsp;

  save;

  ST_FLUSH_WINDOWS();
  old->errno = errno;
  old->context[SP_OFFSET] = sp;
  old->context[PC_OFFSET] = pc;

  oldsp = &pthread_tempstack;
  handle_one_process_signal(sig, code);

  new = ready.head;
  goto restore_stack;
}
  
/*------------------------------------------------------------*/
/*
 * sighandler - 
 */
void sighandler(sig, code, scp, addr)
     sigset_t sig;
     int code;
     struct sigcontext *scp;
     char *addr;
{
  register sigset_t sigset = sigmask(sig);

  if (!(new_signals & sigset))
    if (!is_in_kernel && scp) {
      is_in_kernel = TRUE;
      sigsetmask(S_ENABLE);
      mac_pthread_self()->scp = mac_pthread_self()->nscp;
      mac_pthread_self()->nscp = scp;
      pthread_sched_wrapper(sig, code);
    }
    else {
      new_signals |= sigmask(sig);
      new_code[sig] = code;
    }
}

/*------------------------------------------------------------*/
/*
 * setjmp - 
 */
int setjmp(env)
struct jmp_buf *env;
{
  env->sp = sp;
  env->pc = pc;
  env->mask = mac_pthread_self()->mask;
#ifndef C_INTERFACE
  SAVE_WINDOW(SA(env->regs));
#endif
  return(0);
}

/*------------------------------------------------------------*/
/*
 * longjmp - 
 */
void longjmp(env, val)
struct jmp_buf *env;
int val;
{
  int new_sp;

  pthread_change_signal_mask(SIG_SETMASK, env->mask, NULL);
  ST_FLUSH_WINDOWS();
#ifndef C_INTERFACE
  new_sp = env->sp;
  fp = SA(env->regs);
#else
  fp = env->sp;
#endif
  pc = env->pc;
  if (val == 0)
    val = 1;
#ifndef C_INTERFACE
  restore;
  sp = new_sp;
  retl;
#else
  return;
#endif
}

/*------------------------------------------------------------*/
/*
 * sigsetjmp - 
 */
int sigsetjmp(env, savemask)
struct jmp_buf *env;
int savemask;
{
  env->sp = sp;
  env->pc = pc;
  env->mask = (savemask ? mac_pthread_self()->mask : -1);
#ifndef C_INTERFACE
  SAVE_WINDOW(SA(env->regs));
#endif
  return(0);
}

/*------------------------------------------------------------*/
/*
 * siglongjmp - 
 */
void siglongjmp(env, val)
struct jmp_buf *env;
int val;
{
  int new_sp;

  if (env->mask != -1)
    pthread_change_signal_mask(SIG_SETMASK, env->mask, NULL);
  ST_FLUSH_WINDOWS();
#ifndef C_INTERFACE
  new_sp = env->sp;
  fp = SA(env->regs);
#else
  fp = env->sp;
#endif
  pc = env->pc;
  if (val == 0)
    val = 1;
#ifndef C_INTERFACE
  restore;
  sp = new_sp;
  retl;
#else
  return;
#endif
}

/*------------------------------------------------------------*/
/*
 * pthread_mutex_lock - 
 */
#ifdef NOERR_CHECK
int pthread_mutex_lock(mutex)
pthread_mutex_t *mutex;
{
#ifdef _POSIX_THREADS_PRIO_PROTECT
  if (mutex->protocol == PRIO_PROTECT)
    goto slow_lock;
#endif
  if (test_and_set(&mutex->lock)) {
    mutex->owner = mac_pthread_self();
    return(0);
  }
  /*
   * if the queue is not empty or if someone holds the mutex,
   * we need to enter the kernel to queue up.
   */
#ifdef _POSIX_THREADS_PRIO_PROTECT
slow_lock:
#endif
  return(slow_mutex_lock(mutex));
}

/*------------------------------------------------------------*/
/*
 * pthread_mutex_trylock - 
 */
int pthread_mutex_trylock(mutex)
pthread_mutex_t *mutex;
{
#ifdef _POSIX_THREADS_PRIO_PROTECT
  if (mutex->protocol == PRIO_PROTECT)
    return slow_mutex_trylock(mutex);
#endif
  if (test_and_set(&mutex->lock)) {
    mutex->owner = mac_pthread_self();
    set_errno(EBUSY);
    return(-1);
  }
  return(0);
}

/*------------------------------------------------------------*/
/*
 * pthread_mutex_unlock - 
 */
int pthread_mutex_unlock(mutex);
pthread_mutex_t *mutex;
{

#ifdef _POSIX_THREADS_PRIO_PROTECT
  if (mutex->protocol == PRIO_PROTECT)
    goto slow_unlock;
#endif

  mutex->owner = NO_PTHREAD;
  if (mutex->queue.head == NULL) {
    mutex->lock = FALSE;
    /*
     * We have to test the queue again since there is a window
     * between the previous test and the unlocking of the mutex
     * where someone could have queued up.
     */
    if (mutex->queue.head == NULL)
      return(0);
    if (test_and_set(&mutex->lock))
      /*
       * if the test & set is not successful, someone else must
       * have acquired the mutex and will handle proper queueing,
       * so we're done.
       */
      return(0);
  }
  /*
   * if the queue is not empty, we need to enter the kernel to unqueue.
   */
#ifdef _POSIX_THREADS_PRIO_PROTECT
slow_unlock:
#endif
  return(slow_mutex_unlock(mutex));
}
#endif

/*------------------------------------------------------------*/
/*
 * pthread_cleanup_push - 
 */
#ifndef CLEANUP_HEAP
int pthread_cleanup_push(func, arg)
{
  cleanup_t *new;

#ifndef C_INTERFACE
  sp -= SA(sizeof(*new)+SA(MINFRAME)-WINDOWSIZE);
#else
  sp -= SA(sizeof(*new));
#endif
  new = sp + SA(MINFRAME);
  pthread_cleanup_push_body(func, arg, new);
}

/*------------------------------------------------------------*/
/*
 * pthread_cleanup_pop - 
 */
int pthread_cleanup_pop(execute)
{
  pthread_cleanup_pop_body(execute);
#ifndef C_INTERFACE
  sp += SA(sizeof(*new)+SA(MINFRAME)-WINDOWSIZE);
#else
  sp += SA(sizeof(*new));
#endif
}
#endif

void start_float()
{
  pthread_init();
}
