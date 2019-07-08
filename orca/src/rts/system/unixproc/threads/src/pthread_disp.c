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

  @(#)pthread_disp.c	1.20 10/1/93

*/


#include "pthread_internals.h"
#include <sun4/frame.h>

#ifdef C_CONTEXT_SWITCH
/*
 * Context switch (preemptive), now coded in C instead of assembly
 *
 * Do NOT compile with -O3 or more. This file contains several signal
 * handling routines which modify global data. Thus, the last optimization
 * which is safe is -O2!
 *
 * Notice that functions within ifdef _ASM are still not to be
 * compiled. The code of these functions simply serves as pseudo-
 * code to better understand the assembly code.
 *
 * Portability notes:
 * (1) System calls to BSD routines have to be changed to SVR4.
 *     Some system calls (e.g. sigprocmask) are redefined by Pthreads.
 *     Thus, uses of sigsetmask cannot be replaced by sigprocmask
 *     but rather have to be translated into direct syscall()'s.
 * (2) The context switch code below assumes at some places the
 *     presence of register windows. In particular, the calls to
 *     pthread_ST_FLUSH_WINDOWS() should be redundant on most other
 *     architectures. Some other register flushing may be necessary
 *     at these places though to ensure that the old stack pointer
 *     is safed in memory rather than the new one.
 * (3) The assembly code for invoking a fake call (wrapper_wrappers)
 *     will be different on other architectures. This can range from
 *     complete redundance to some other form of parameter passing.
 *     Critical are also the parameters passed in stack past the
 *     sixth parameter. Other architectures may require some new tricks.
 */

/*
 * temporary stack for dispatcher, pthread_handle_many_process_signals,
 * N nested function calls by pthread_handle_many_process_signals,
 * a possible sigtramp and universal handler, plus 1 window spare
 */
#ifdef DEBUG
char pthread_tempstack_body[104*MINFRAME+WINDOWSIZE];
#else
char pthread_tempstack_body[54*MINFRAME+WINDOWSIZE];
#endif
char pthread_tempstack[WINDOWSIZE];

/*------------------------------------------------------------*/
/* 
 * pthread_sched - dispatcher
 * assumes SET_KERNEL_FLAG
 */
void pthread_sched()
{
  register int change;
  register pthread_t old, new;
  pthread_t pthread_sched_new_signals();
  
  old = mac_pthread_self();

  do {
    if (old->state & T_RETURNED && old->state & T_DETACHED) {
#ifdef MALLOC
      pthread_free(old->stack_base);
      pthread_free(old);
#else !MALLOC
      free(old->stack_base);
      free(old);
#endif MALLOC
      old = NO_PTHREAD;
    }
    
    if (change = (new_signals || (new = ready.head) == NO_PTHREAD))
      old = pthread_sched_new_signals(old, FALSE);
  } while (change);

  mac_pthread_self() = new;
#ifdef DEF_RR
  if (new->attr.sched == SCHED_RR)
    pthread_timed_sigwait(new, (struct timespec *) NULL, RR_TIME);
#endif
  
  if (!pthread_not_called_from_sighandler(new->context.pc))
    sigsetmask(S_DISABLE);
  
  siglongjmp(&new->context, TRUE);
}

/*------------------------------------------------------------*/
/*
 * pthread_sched_new_signals - handle signals which came in while inside
 * the kernel by switching over to the temporary stack
 */
pthread_t pthread_sched_new_signals(p, masked)
pthread_t p;
int masked;
{
  register pthread_t old = p, new;
  extern pthread_t pthread_handle_many_process_signals();

  if (masked && old && !pthread_not_called_from_sighandler(old->context.pc))
    sigsetmask(S_ENABLE);
    
  /*
   * always flush windows before the stack is changed to preserve the proper
   * linking of frame pointers
   */
  pthread_ST_FLUSH_WINDOWS();
  pthread_set_sp(SA((int)pthread_tempstack) - SA(WINDOWSIZE));
  new = pthread_handle_many_process_signals();
  if (!old)
    old = new;
  pthread_set_sp(old->context.sp - SA(WINDOWSIZE));
  return(new);
}

#ifdef _ASM
/*------------------------------------------------------------*/
/*
 * pthread_sched_wrapper -
 */
#ifdef C_CONTEXT_SWITCH
void pthread_sched_wrapper(sig, code, p)
int sig, code;
pthread_t p;
#else
void pthread_sched_wrapper(sig, code)
int sig, code;
#endif
{
  save;

#ifdef C_CONTEXT_SWITCH
  if (!sigsetjmp(&p->context, FALSE))
#endif
    pthread_signal_sched(sig, code);

  return;
}

/*------------------------------------------------------------*/
/*
 * pthread_not_called_from_sighandler -
 */
int pthread_not_called_from_sighandler(addr)
int addr;
{
  return(addr - &called_from_sighandler);
}

/*------------------------------------------------------------*/
/*
 * pthread_test_and_set -
 */
int pthread_test_and_set(flag)
int *flag;
{
  return(ldstub(flag));
}

/*------------------------------------------------------------*/
/*
 * pthread_get_sp -
 */
char *pthread_get_sp()
{
  return(sp);
}

/*------------------------------------------------------------*/
/*
 * pthread_set_sp -
 */
void pthread_set_sp(new_sp)
char *new_sp;
{
  sp = new_sp;
}

/*------------------------------------------------------------*/
/*
 * pthread_get_fp -
 */
char *pthread_get_fp()
{
  return(fp);
}

/*------------------------------------------------------------*/
/*
 * pthread_set_fp -
 */
void pthread_set_fp(new_fp)
char *new_fp;
{
  fp = new_fp;
}

/*------------------------------------------------------------*/
/*
 * pthread_ST_FLUSH_WINDOWS -
 */
void pthread_ST_FLUSH_WINDOWS()
{
  ST_FLUSH_WINDOWS();
}

#ifdef C_CONTEXT_SWITCH
/*------------------------------------------------------------*/
/*
 * pthread_fake_call_wrapper_wrapper -
 */
void pthread_fake_call_wrapper_wrapper(user_handler, smask, sig, infop, scp,
                               restore_context, oscp, cond)
void (*user_handler)();
sigset_t smask;
int sig;
struct siginfo *infop;
struct sigcontext *scp;
int restore_context;
struct sigcontext *oscp;
pthread_cond_t **cond;
{
  extern void pthread_fake_call_wrapper();

  pthread_fake_call_wrapper(user_handler, smask, sig, infop, scp,
			    restore_context, oscp, cond);
}

/*------------------------------------------------------------*/
/*
 * pthread_clear_kernel_flag_wrapper_wrapper -
 */
void pthread_clear_kernel_flag_wrapper_wrapper()
{
  pthread_clear_kernel_flag_wrapper();
  restore; /* Delay:restore previous window */
}  
#endif C_CONTEXT_SWITCH
#endif _ASM

/*------------------------------------------------------------*/
/*
 * pthread_clear_kernel_flag_wrapper - after a fake call with modified pc in the
 * context structure, return though this wrapper which clear the kernel
 * flag before jumping into user code.
 */
void pthread_clear_kernel_flag_wrapper()
{
  CLEAR_KERNEL_FLAG;
}

/*------------------------------------------------------------*/
/*
 * pthread_fake_call_wrapper - invoke a fake call on a thread's stack
 * fake_call already puts the address of a user-defined handler
 * in %i0, the signal mask (to be restored) in %i1, and a flag
 * restore_context in %i5 which indicates if the context has to be
 * copied back by the wrapper (o.w. it is done by UNIX).
 * It calls the user handler with parameters sig, infop, scp.
 * Notice that the address of the condition variable is
 * passed on stack if the signal came in during a conditional wait.
 * In this case, the conditional wait terminates and the mutex is relocked
 * before the user handler is called. This is only done once for
 * nested handlers by the innermost handler (see check for zero-value
 * of the condition variable).
 * Notice that oscp is passed on stack and is restored as p->nscp
 * upon return from the wrapper.
 * The errno is saved across the user handler call.
 * assumes SET_KERNEL_FLAG still set from context switch after pushing fake call
 */
void pthread_fake_call_wrapper(user_handler, smask, sig, infop, scp,
                               restore_context, oscp, cond)
void (*user_handler)();
sigset_t smask;
int sig;
struct siginfo *infop;
struct sigcontext *scp;
int restore_context;
struct sigcontext *oscp;
pthread_cond_t **cond;
{
  register pthread_t p;
  register sigset_t omask = scp->sc_mask;
  register int saved_errno = errno;
  register struct frame *framep;
  register int old_pc = scp->sc_pc;
  void pthread_handle_pending_signals_wrapper();
  extern void pthread_clear_kernel_flag_wrapper_wrapper();

  if (*cond)
    pthread_cond_wait_terminate();

  CLEAR_KERNEL_FLAG;
  (*user_handler)(sig, infop, scp);
  SET_KERNEL_FLAG;

  errno = saved_errno;
  p = mac_pthread_self();

  if (restore_context) {
    if (old_pc != scp->sc_pc) {
      /*
       * always flush windows before the stack is changed to preserve the proper
       * linking of frame pointers.
       * Then insert a frame to clear kernel flag, assuming that the sp
       * provides enough space for an extra frame (which is guaranteed by
       * the wrappers). This stack modification is critical though since
       * it violates the stack discipline, i.e. does not happen on the top
       * of the stack.
       */
      pthread_ST_FLUSH_WINDOWS();
      framep = (struct frame *) (scp->sc_sp - SA(MINFRAME));
      framep->fr_savfp = (struct frame *) scp->sc_sp;
      framep->fr_savpc = scp->sc_pc - RETURN_OFFSET;

      p->context.sp = (int) framep;
      p->context.pc = 
	(int) pthread_clear_kernel_flag_wrapper_wrapper - RETURN_OFFSET;
    }
    else {
      p->context.sp = scp->sc_sp;
      p->context.pc = scp->sc_pc - RETURN_OFFSET;
    }
    smask = scp->sc_mask;
  }
  else if (omask != scp->sc_mask) {
    smask = scp->sc_mask;
    scp->sc_mask = omask;
  }

  p->mask = smask;

  if ((p->pending | pending_signals) & ~smask)
    if (restore_context || !sigsetjmp(&p->context, FALSE))
      pthread_handle_pending_signals_wrapper();
      /* never returns from call */

  p->nscp = oscp;

  if (restore_context)
    pthread_sched();
    /* never returns from call */
}
          
/*------------------------------------------------------------*/
/*
 * pthread_handle_pending_signals_wrapper - 
 * change to temp stack and call pthread_handle_pending_signals()
 * then jumps into regular scheduler
 * assumes SET_KERNEL_FLAG
 */
void pthread_handle_pending_signals_wrapper()
{
  register pthread_t old = mac_pthread_self();

  /*
   * always flush windows before the stack is changed to preserve the proper
   * linking of frame pointers
   */
  pthread_ST_FLUSH_WINDOWS();
  pthread_set_sp(SA((int)pthread_tempstack) - SA(WINDOWSIZE));
  pthread_handle_pending_signals();
  pthread_set_sp(old->context.sp - SA(WINDOWSIZE));

  pthread_sched();
  /* never returns from call */
}

/*------------------------------------------------------------*/
/*
 * pthread_signal_sched - 
 * change to temp stack and call pthread_handle_one_process_signal(sig)
 * then jumps into regular scheduler
 * This is called by the universal signal handler to minimize calls
 * to sigsetmask() which is an expensive UNIX system call.
 * assumes SET_KERNEL_FLAG
 */
void pthread_signal_sched(p_sig, p_code)
int p_sig, p_code;
{
  register pthread_t old = mac_pthread_self();
  register int sig = p_sig, code = p_code;

  /*
   * always flush windows before the stack is changed to preserve the proper
   * linking of frame pointers
   */
  pthread_ST_FLUSH_WINDOWS();
  pthread_set_sp(SA((int)pthread_tempstack) - SA(WINDOWSIZE));
  pthread_handle_one_process_signal(sig, code);
  pthread_set_sp(old->context.sp - SA(WINDOWSIZE));
  
  pthread_sched();
  /* never returns from call */
}
  
#ifdef _ASM
/*------------------------------------------------------------*/
/*
 * setjmp - 
 */
int setjmp(env)
struct jmp_buf *env;
{
  env->errno = errno;
  env->sp = sp;
  env->pc = pc;
  env->mask = mac_pthread_self()->mask;
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
  errno = env->errno;
  ST_FLUSH_WINDOWS();
  fp = env->sp;
  pc = env->pc;
  if (val == 0)
    val = 1;
  return;
}

/*------------------------------------------------------------*/
/*
 * sigsetjmp - 
 */
int sigsetjmp(env, savemask)
struct jmp_buf *env;
int savemask;
{
  env->errno = errno;
  env->sp = sp;
  env->pc = pc;
  env->mask = (savemask ? mac_pthread_self()->mask : -1);
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
  errno = env->errno;
  ST_FLUSH_WINDOWS();
  fp = env->sp;
  pc = env->pc;
  if (val == 0)
    val = 1;
  return;
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

/*------------------------------------------------------------*/
/*
 * start_float -
 */
void start_float()
{
  pthread_init();
}
#endif _ASM
#endif C_CONTEXT_SWITCH

/*------------------------------------------------------------*/
/*
 * pthread_process_exit - switches stacks to process stack and
 * calls UNIX exit with parameter status
 */
void pthread_process_exit(p_status)
int p_status;
{
  register int status = p_status;

  /*
   * always flush windows before the stack is changed to preserve the proper
   * linking of frame pointers
   */
  pthread_ST_FLUSH_WINDOWS();
  pthread_set_sp(process_stack_base);
  CLEAR_KERNEL_FLAG;
  exit(status);
}
