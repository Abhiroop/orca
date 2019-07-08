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

  @(#)signal.c	1.20 10/4/93

*/

/* 
 * Functions for the handling of signals and timers.
 */

/*
 * The DEBUG flag causes a message to be printed out during signal handling.
 * The IO flag handles I/O requests asynchronously such that a signal is
 * delivered to the process upon completion of the operation.
 * If both flags are set at the same time, the signal handler would issue
 * an I/O request for each invocation which in turns causes another signal
 * to be delivered to yet another instance of the signal handler.
 * To avoid this, messages are only printed if DEBUG is defined but not IO.
 */

#define PTHREAD_KERNEL
#include "signal_internals.h"
#include "pthread_internals.h"
#include <sys/signal.h>
#ifdef NOERR_CHECK
#undef NOERR_CHECK
#include "mutex.h"
#define NOERR_CHECK
#else
#include "mutex.h"
#endif
#ifdef RAND_SWITCH 
extern int pthread_n_ready;
#endif
#ifdef TIMER_DEBUG
static struct timeval last_alarm;
#endif
#if STACK_CHECK && SIGNAL_STACK
extern int pthread_page_size;
extern char pthread_tempstack;
#endif

pthread_timer_q pthread_timer;                /* timer queue                 */
static struct sigaction user_handler[NNSIG];  /* user signal handlers        */
static int new_code[NNSIG];                   /* UNIX signal code (new sigs) */
static int pending_code[NNSIG];               /* UNIX signal code (pending)  */
static sigset_t synchronous =                 /* set of synchronous signals  */
  sigmask(SIGILL) | sigmask (SIGABRT) | sigmask(SIGEMT) | sigmask(SIGFPE) |
  sigmask(SIGBUS) | sigmask(SIGSEGV) | sigmask(SIGPIPE);
static sigset_t sig_handling = 0;             /* set of signals being handled*/

/*------------------------------------------------------------*/
/*
 * handle_thread_signal - try to handle one signal on a thread and
 * return TRUE if it was handled, otherwise return FALSE
 */
static int handle_thread_signal(p, sig, sigset, code)
pthread_t p;
int sig;
sigset_t sigset;
int code;
{
  register struct sigcontext *scp;

  /*
   * handle timer signals
   */
  
  if (sig == TIMER_SIG) {
#ifdef TIMER_DEBUG
    fprintf(stderr, "handle_thread_signal: timer signal\n");
#endif
#ifdef DEF_RR
    if (pthread_timer.head->mode == RR_TIME && p->queue == &ready) {
      pthread_cancel_timed_sigwait(p, TRUE, ANY_TIME, FALSE);
      pthread_q_deq(&ready, p, PRIMARY_QUEUE);
      pthread_q_primary_enq(&ready, p);
    } else
#endif
      pthread_cancel_timed_sigwait(p, TRUE, ANY_TIME, p->queue != &ready);
    return(TRUE);
  }
  
  /*
   * handle signals for sigwait
   */
  if (p->state & T_SIGWAIT && p->sigwaitset & sigset) {
    pthread_q_wakeup_thread(NO_QUEUE, p, NO_QUEUE_INDEX);
    p->state &= ~T_SIGWAIT;
    p->mask |= p->sigwaitset & ~cantmask;
    p->sigwaitset &= ~sigset;
    return(TRUE);
  }
  
  /*
   * handle signals for sigsuspend and user handlers
   */
  if (handlerset & sigset) {
    /*
     * handler set to ignore
     */
    if (user_handler[sig].sa_handler == SIG_IGN)
      return(TRUE);
    
    /*
     * handler not set to default but to some user handler
     */
    if (user_handler[sig].sa_handler != SIG_DFL) {
      if (p->state & T_BLOCKED)
	p->context.errno = EINTR;
      
      if (!(p->state & T_RUNNING)) {
	if (p->state & T_SYNCTIMER)
	  pthread_cancel_timed_sigwait(p, FALSE, SYNC_TIME, TRUE);
	else {
	  pthread_q_wakeup_thread(p->queue, p, PRIMARY_QUEUE);
	  if (p->state & (T_SIGWAIT | T_SIGSUSPEND)) {
	    p->state &= ~(T_SIGWAIT | T_SIGSUSPEND);
	    p->sigwaitset &= cantmask;
	  }
	}
      }
      
      p->sig_info[sig].si_signo = sig;
      p->sig_info[sig].si_code = code;

      if (pthread_not_called_from_sighandler(p->context.pc))
	scp = DIRECTED_AT_THREAD;
      else
	scp = p->nscp;

      pthread_push_fake_call(p, user_handler[sig].sa_handler, sig, scp,
		     sigset | user_handler[sig].sa_mask);

      return(TRUE);
    }
  }

  /*
   * handle cancel signal
   */
  if (sig == SIGCANCEL) {
    if (p->state & T_SYNCTIMER)
      pthread_cancel_timed_sigwait(p, FALSE, ALL_TIME, TRUE);
    else if (p->state & (T_SIGWAIT | T_SIGSUSPEND)) {
      p->state &= ~(T_SIGWAIT | T_SIGSUSPEND);
      p->sigwaitset &= cantmask;
    }

    if (p->queue && !(p->state & T_RUNNING))
      pthread_q_deq(p->queue, p, PRIMARY_QUEUE);

    pthread_q_deq(&all, p, ALL_QUEUE);
    
    /*
     * no more signals for this thread, not even cancellation signal
     */
    p->mask = S_DISABLE | sigmask(SIGCANCEL);
    pthread_push_fake_call(p, pthread_exit, -1, DIRECTED_AT_THREAD, 0);
    if (!(p->state & T_RUNNING))
      pthread_q_wakeup_thread(NO_QUEUE, p, NO_QUEUE_INDEX);

    return(TRUE);
  }

  return (FALSE);
}

/*------------------------------------------------------------*/
/*
 * pthread_clear_sighandler  - get rid of universal signal handler for all
 * signals except for those which cannot be masked;
 * also invalidate the timer if still active
 */
void pthread_clear_sighandler()
{
  struct sigvec vec;
  register int sig;
  struct itimerval it;

  vec.sv_handler = SIG_DFL;
  vec.sv_mask = S_DISABLE;
  vec.sv_flags = 0;

  for (sig = 1; sig < NSIG; sig++)
    if (!(sigmask(sig) & cantmask))
      if (sigvec(sig, &vec, (struct sigvec *) NULL))
	fprintf(stderr,
		"Pthreads: Could not install handler for signal %d\n", sig);

  if (!getitimer(ITIMER_REAL, &it) && timerisset(&it.it_value)) {
    it.it_value.tv_sec = it.it_value.tv_usec = 0;
    it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &it, (struct itermval *) NULL);
  }
}

/*------------------------------------------------------------*/
/*
 * default_action - take default action on process
 * Notice: SIGSTOP and SIGKILL can never be received (unmaskable)
 * but they are included anyway.
 */
static void default_action(sig)
int sig;
{
  switch (sig) {
  case SIGHUP:
  case SIGINT:
  case SIGQUIT:
  case SIGILL:
  case SIGTRAP:
  case SIGABRT:
  case SIGEMT:
  case SIGFPE:
  case SIGKILL:
  case SIGBUS:
  case SIGSEGV:
  case SIGSYS:
  case SIGPIPE:
  case SIGALRM:
  case SIGTERM:
  case SIGXCPU:
  case SIGXFSZ:
  case SIGVTALRM:
  case SIGPROF:
  case SIGLOST:
  case SIGUSR1:
  case SIGUSR2:
#ifdef TIMER_DEBUG
      fprintf(stderr, "last alarm   is %d.%d\n", last_alarm.tv_sec,
	                                         last_alarm.tv_usec);
    if (pthread_timer.head != NO_TIMER) {
      fprintf(stderr, "wakeup  time is %d.%d\n", pthread_timer.head->tp.tv_sec,
	                                         pthread_timer.head->tp.tv_usec);
      gettimeofday(&pthread_timer.head->tp, (struct timezone *) NULL);
      fprintf(stderr, "current time is %d.%d\n", pthread_timer.head->tp.tv_sec,
	                                         pthread_timer.head->tp.tv_usec);
    }

#endif
    pthread_clear_sighandler();
    kill(getpid(), sig); /* reissue signal to terminate process: UNIX default */
    sigsetmask(S_ENABLE);
#ifdef DEBUG
#ifndef IO
    fprintf(stderr, "RUNAWAY: should produce core dump now\n");
    pthread_process_exit(-1); /* abnormal termination, but no core dump */
#endif
#endif
  case SIGURG:
  case SIGCONT:
  case SIGCHLD:
  case SIGIO:
  case SIGWINCH:
    break; /* ignore or continue */
  case SIGSTOP:
  case SIGTSTP:
  case SIGTTIN:
  case SIGTTOU:
    kill(getpid(), SIGSTOP);  /* stop process -> UNIX may generate SIGCHLD */
    break;
  }
}

/*------------------------------------------------------------*/
/*
 * handle_one_signal - handle one signal on the process level
 * assumes SET_KERNEL_FLAG
 */
static void handle_one_signal(sig, sigset, code)
int sig;
sigset_t sigset;
int code;
{
  register pthread_t p;
  struct itimerval it;
  extern pthread_t pthread_q_all_find_receiver();
  static int aio_handle();

  /*
   * Determine who needs to get the signal (in the following order):
   * (1) signal directed at specific thread: take this thread
   * (2) signal at process level:
   * (2a) synchronous signal: direct at current thread
   * (2b) SIGALRM, timer queue not empty, timer expired: take head off timer q
   * (2c) SIGIO, asynchronous I/O requested: determine receiver and make ready
   * (2c) handler defined: take first thread in all queue with signal unmasked
   * (3) no handler defined:
   * (3a) SIG_IGN requested: ignore the signal
   * (3b) SIG_DFL requested: perform default action on process
   * (3c) handler installed: pend signal on process till queue non-empty
   *      if signal already pending, it's lost
   */
  if ((mac_pthread_self()->nscp == DIRECTED_AT_THREAD ||
       pthread_not_called_from_sighandler(
                                 mac_pthread_self()->context.pc)) &&
      (p = (pthread_t) code))
    code = 0;
  else if (synchronous & sigset)
    p = mac_pthread_self();
  else if (sig == SIGALRM && 
#ifdef DEF_RR
	   pthread_timer.head != NO_TIMER && (p = pthread_timer.head->thread) &&
#else
	   (p = pthread_timer.head) &&
#endif
	   !getitimer(ITIMER_REAL, &it) && !timerisset(&it.it_value)) {
    sig = TIMER_SIG;
  }
#ifdef IO
  else if (sig == SIGIO && aio_handle())
    return;
#endif
  else if (!(p = pthread_q_all_find_receiver(&all, sigset)))
    switch ((int) user_handler[sig].sa_handler) {
    case (int)SIG_DFL:
      default_action(sig);
      return;
    case (int)SIG_IGN:
      return;
    default:
      if (!(pending_signals & sigset)) {
	pending_signals |= sigset;
	pending_code[sig] = code;
      }
      return;
    }
  
  if (p->state & T_RETURNED)
    return;
  
  /*
   * Pend signal on thread if it's masked out OR
   * if the signal is SIGCANCEL, the interrupt state CONTROLLED, and
   * we are not at an interruption point.
   */
  if (p->mask & sigset || sig == SIGCANCEL &&
                          p->state & T_CONTROLLED &&
                          !(p->state & T_INTR_POINT)) {
    p->pending |= sigset;
    /* p->sig_info[sig].si_signo = sig;*/
    p->sig_info[sig].si_code = code;
    return;
  }

  if (handle_thread_signal(p, sig, sigset, code))
    return;
  
  default_action(sig);
}
  
/*------------------------------------------------------------*/
/*
 * pthread_handle_many_process_signals - determine pending signal(s).
 * if no thread ready, suspend process;
 * returns the head of the ready queue.
 * assumes SET_KERNEL_FLAG
 */
pthread_t pthread_handle_many_process_signals()
{
  register int sig;
  register sigset_t sigset;

  do {
    while (sig_handling = new_signals) {
      for (sig = 1, sigset = sigmask(1); sig < NNSIG; sig++, sigset <<= 1)
	if (sig_handling & sigset)
	  handle_one_signal(sig, sigset, new_code[sig]);
      
      sigsetmask(S_DISABLE);
      /*
       * start critical section
       */
      
      new_signals &= ~sig_handling;
      
      sigsetmask(S_ENABLE);
      /*
       * end of critical section
       */
    };

    /*
     * No thread, no action: suspend waiting for signal at process level
     */
    if (ready.head == NO_PTHREAD) {
      sigsetmask(S_DISABLE);
      if (!new_signals) {
#ifdef DEBUG
#ifndef IO
	fprintf(stderr, "suspending process waiting for signal\n");
#endif
#endif
	sigpause(S_ENABLE);
      }
      sigsetmask(S_ENABLE);
    }

  } while (ready.head == NO_PTHREAD);

  return(ready.head);
}

/*------------------------------------------------------------*/
/*
 * pthread_handle_one_process_signal - handle latest signal caught by 
 * universal handler while not in kernel
 * returns the head of the ready queue.
 * assumes SET_KERNEL_FLAG
 */
pthread_t pthread_handle_one_process_signal(sig, code)
int sig;
int code;
{
  handle_one_signal(sig, sigmask(sig), code);

  if (new_signals || ready.head == NO_PTHREAD)
    pthread_handle_many_process_signals();
}

/*------------------------------------------------------------*/
/*
 * pthread_handle_pending_signals - handle unmasked pending signals of 
 * current thread assumes SET_KERNEL_FLAG
 */
void pthread_handle_pending_signals()
{
  pthread_t p = mac_pthread_self();
  int sig;
  sigset_t sigset;

  /*
   * handle signals pending on threads if they are unmasked and
   * SIGCANCEL only on an interruption point.
   */
  if (~p->mask & p->pending)
    for (sig = 1, sigset = sigmask(1); sig < NNSIG; sig++, sigset <<= 1)
      if (p->pending & sigset && !(p->mask & sigset) && 
	  (sig != SIGCANCEL || p->state & T_INTR_POINT)) {
	p->pending &= ~sigset;
	
	handle_thread_signal(p, sig, sigset, p->sig_info[sig].si_code);
      }

  /*
   * handle signals pending on process
   */
  if (~p->mask & pending_signals)
    for (sig = 1, sigset = sigmask(1); sig < NNSIG; sig++, sigset <<= 1)
      if (pending_signals & sigset && !(p->mask & sigset)) {
	pending_signals &= ~sigset;
	
	handle_thread_signal(p, sig, sigset, pending_code[sig]);
      }
}

/*------------------------------------------------------------*/
/*
 * sighandler - wrapper for all signals, defers signals for later processing
 * Notice: All maskable signals are caught and re-multiplexed by Pthreads.
 */
static void sighandler(sig, code, scp, addr)
sigset_t sig;
int code;
struct sigcontext *scp;
char *addr;
{
  register sigset_t sigset = sigmask(sig);
  register pthread_t p = mac_pthread_self();
#if STACK_CHECK && SIGNAL_STACK
  void pthread_io_end();
  struct sigstack ss;

  if (((sig == SIGILL && code == ILL_STACK) || 
       (sig == SIGSEGV && FC_CODE(code) == FC_PROT) ||
       (sig == SIGBUS && FC_CODE(code) == FC_OBJERR)) &&
      ((scp->sc_sp < PA(p->stack_base+3*pthread_page_size))
#ifdef IO
        || (scp->sc_pc > (int)read && scp->sc_pc < (int)pthread_io_end)
#endif
                                                                       ))
    if (p->state & T_LOCKED)
      pthread_unlock_stack(p);
    else
      default_action(SIGILL);

  if (sig == SIGILL || sig == SIGBUS || sig == SIGSEGV) {
    switch_stacks(scp->sc_sp);
    if (scp->sc_onstack)
      scp->sc_onstack = FALSE;
    else {
      ss.ss_sp = (char *) SA((int)&pthread_tempstack);
      ss.ss_onstack = FALSE;
      if (sigstack(&ss, (struct sigstack *) NULL))
#ifdef DEBUG
        fprintf(stderr,
            "Pthreads: Could not specify signal stack, errno %d\n", errno)
#endif
		;
    }
  }

#endif

  /*
   * add signal to queue of pending signals
   */
#ifdef DEBUG
#ifndef IO
#ifdef DEF_RR
  if (sig != SIGALRM)
#endif
    fprintf(stderr, "signal %d caught\n", sig);
#endif
#endif

  if (!(new_signals & sigset))
    if (!is_in_kernel && scp) {
      SET_KERNEL_FLAG;
      sigsetmask(S_ENABLE);

      /*
       * Associate UNIX context with current thread
       */
      p->scp = p->nscp;
      p->nscp = scp;

      /*
       * A signal is pending so that the siganl dispatcher calls
       * pthread_handle_one_process_signals()
       * and then switches to highest priority thread. Thus, we act as if no
       * context switch is initiated, i.e. as if we switched back to thread
       * which was running when the signal came in.
       */
#ifndef C_CONTEXT_SWITCH
      pthread_sched_wrapper(sig, code);
#else
      pthread_sched_wrapper(sig, code, p);
      CLEAR_KERNEL_FLAG;
#endif
    }
    else {
      new_signals |= sigset;
      new_code[sig] = code;
      state_change = TRUE;
    }
  else printf("weetikveel: %d\n", sig);
}

/*------------------------------------------------------------*/
/*
 * pthread_init_signals - initialize signal package
 */
void pthread_init_signals()
{
  int sig;
  struct sigvec vec;
#if STACK_CHECK && SIGNAL_STACK
  struct sigstack ss;

  ss.ss_sp = (char *) SA((int)&pthread_tempstack);
  ss.ss_onstack = FALSE;
  if (sigstack(&ss, (struct sigstack *) NULL))
    fprintf(stderr,
	    "Pthreads: Could not specify signal stack, errno %d\n", errno);
#endif

  /*
   * initialize kernel structure
   */
  is_in_kernel = FALSE;
  new_signals = pending_signals = handlerset = 0;
  S_ENABLE = cantmask;
  S_DISABLE = ~cantmask;
  pthread_queue_init(&ready);
  pthread_queue_init(&all);
  pthread_timer_queue_init(&pthread_timer);
  pthread_kern.k_stderr = (FILE *) stderr;
  set_warning = "CAUTION: entering kernel again\n";
  clear_warning = "CAUTION: leaving kernel again\n";
#ifdef RAND_SWITCH
  srandom(1);
  pthread_n_ready = 0;
#endif

  /*
   * no signal requests
   */
  for (sig = 0; sig <= NNSIG; sig++) {
    user_handler[sig].sa_handler = SIG_DFL;
    user_handler[sig].sa_mask = 0;
    user_handler[sig].sa_flags = 0;
    new_code[sig] = 0;
  }

  /*
   * install universal signal handler for all signals
   * except for those which cannot be masked
   */
  vec.sv_handler = sighandler;
  vec.sv_mask = S_DISABLE;
  vec.sv_flags = 0;
    
  for (sig = 1; sig < NSIG; sig++)
    if (!(sigmask(sig) & cantmask)) {
#if STACK_CHECK && SIGNAL_STACK
      if (sig == SIGBUS || sig == SIGILL || sig == SIGSEGV)
        vec.sv_flags |= SV_ONSTACK;
#endif
      if (sigvec(sig, &vec, (struct sigvec *) NULL))
        fprintf(stderr, "Pthreads (signal): \
          Could not install handler for signal %d\n", sig);
#if STACK_CHECK && SIGNAL_STACK
      if (sig == SIGBUS || sig == SIGILL || sig == SIGSEGV)
        vec.sv_flags &= ~SV_ONSTACK;
#endif
    }
}

/*------------------------------------------------------------*/
/*
 * sigwait - suspend thread until signal becomes pending
 * Return: signal number if o.k., otherwise -1
 * Notice: cannot mask SIGKILL, SIGSTOP, SIGCANCEL
 */
int sigwait(set)
sigset_t *set;
{
  register int sig;
  register sigset_t sigset, more;
  register pthread_t p = mac_pthread_self();

  more = *set = *set & ~cantmask;
  SET_KERNEL_FLAG;

  /*
   * Are the signals in set blocked by the current thread?
   */
  if ((p->mask & more) != more) {
    set_errno(EINVAL);
    CLEAR_KERNEL_FLAG;
    return(-1);
  }

  /*
   * Any of the signals in set pending on thread?
   */
  if (p->pending & more)
    for (sig = 1, sigset = sigmask(1); sig < NNSIG; sig++, sigset <<= 1)
      if (p->pending & sigset && more & sigset) {
	p->pending &= ~sigset;
	CLEAR_KERNEL_FLAG;
	return(sig);
      }
    
  /*
   * suspend thread and wait for any of the signals
   */
  p->sigwaitset |= *set;
  p->mask &= ~*set;
  p->state &= ~T_RUNNING;
  p->state |= T_SIGWAIT | T_BLOCKED | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }

  if (get_errno() == EINTR)
    return(-1);

  /*
   * determine the received signal
   */
  for (sig = 1, sigset = sigmask(1); sig < NNSIG; sig++, sigset <<= 1)
    if (*set & sigset && !(p->sigwaitset & sigset))
      break;

  /*
   * Clear signal mask
   */
  SET_KERNEL_FLAG;
  p->sigwaitset &= cantmask;
  CLEAR_KERNEL_FLAG;

  /*
   * If no signal woke us up directly, a user handler (hence interrupt) must
   * have been activated via a different signal.
   */
  if (sig < 0) {
    set_errno(EINVAL);
    return(-1);
  }

  return(sig);
}

/*------------------------------------------------------------*/
/*
 * pthread_change_signal_mask - change signal mask of thread (for internal use)
 */
int pthread_change_signal_mask(how, new, oset)
int how;
sigset_t new, *oset;
{
  register sigset_t old;
  register pthread_t p = mac_pthread_self();

  SET_KERNEL_FLAG;

  old = p->mask;
  if (oset)
    *oset = old;

  switch (how) {
  case SIG_BLOCK:
    p->mask |= new;
    break;
  case SIG_UNBLOCK:
    p->mask &= ~new;
    break;
  case SIG_SETMASK:
    p->mask = new;
    break;
  default:
    set_errno(EINVAL);
    CLEAR_KERNEL_FLAG;
    return(-1);
  }

  if ((p->pending | pending_signals) & (old & ~p->mask))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }
  return(0);
}

/*------------------------------------------------------------*/
/*
 * sigprocmask - change or exeamine signal mask of thread
 * return old mask or -1 if error
 * cannot mask SIGKILL, SIGSTOP, SIGCANCEL
 */
#ifndef __GNUC_MINOR__
int sigprocmask(how, set, oset)
int how;
sigset_t *set, *oset;
#else /* match <sys/signal.h> */
int     sigprocmask(int how, const sigset_t *set, sigset_t *oset)
#endif
{
  if (!set) {
    if (oset)
      *oset = mac_pthread_self()->mask;
    return(0);
  }

  if (how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
    set_errno(EINVAL);
    return(-1);
  }

  return(pthread_change_signal_mask(how, *set & ~cantmask, oset));
}

/*------------------------------------------------------------*/
/*
 * sigpending - inquire about pending signals which are blocked, i.e. applies
 * to only those signals which are explicitly pending on the current thread
 * return 0 if o.k., -1 otherwise
 */
int sigpending(set)
sigset_t *set;
{
  if (!set) {
    set_errno(EINVAL);
    return(-1);
  }

  SIM_SYSCALL(TRUE);
  *set = (mac_pthread_self()->pending | pending_signals) & ~cantmask;
  return(0);
}

/*------------------------------------------------------------*/
/*
 * sigsuspend - suspend thread,
 * set replaces the masked signals for the thread temporarily,
 * suspends thread, and resumes execution when a user handler is invoked
 * Return: -1 and EINTR if interrupted or EINVAL if wrong parameters
 * Notice: cannot mask SIGKILL, SIGSTOP
 */
int sigsuspend(set)
sigset_t *set;
{
  register int sig;
  register sigset_t sigset, oset;
  register pthread_t p = mac_pthread_self();

  if (!set) {
    set_errno(EINVAL);
    return(-1);
  }

  SET_KERNEL_FLAG;
  oset = p->mask;
  p->sigwaitset |= p->mask = *set & ~cantmask;

  p->state &= ~T_RUNNING;
  p->state |= T_SIGSUSPEND | T_BLOCKED | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }

  /*
   * restore the initial signal mask (swap omask, p->mask)
   */
  SET_KERNEL_FLAG;
  p->mask = oset;

  if ((p->pending | pending_signals) & ((*set & ~cantmask) & ~p->mask))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else
    CLEAR_KERNEL_FLAG;

  return(-1);
}

/*------------------------------------------------------------*/
/*
 * pause - suspend thread until any signal is caught,
 * same as sigsuspend except that the signal mask doesn't change
 */
int pause()
{
  register int sig;
  register sigset_t sigset;
  register pthread_t p = mac_pthread_self();

  SET_KERNEL_FLAG;
  p->sigwaitset |= p->mask;
  
  p->state &= ~T_RUNNING;
  p->state |= T_SIGSUSPEND | T_BLOCKED | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }
  
  return(-1);
}

/*------------------------------------------------------------*/
/*
 * pthread_timed_sigwait - suspend running thread until specified time
 * Return -1 if error, 0 otherwise
 * assumes SET_KERNEL_FLAG
 */
int pthread_timed_sigwait(p, timeout, mode)
     pthread_t p;
     struct timespec *timeout;
     int mode;
{
  struct itimerval it;
  struct timeval now, in;
  register timer_ent_t phead;

#ifdef TIMER_DEBUG
    fprintf(stderr, "timed_sigwait: enter\n");
#endif

#ifdef DEF_RR
  if (p->num_timers >= TIMER_MAX) {
    set_errno(EAGAIN);
    return(-1);
  }

  if (mode != RR_TIME) {
#endif
    if (!timeout || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000 ||
	gettimeofday(&now, (struct timezone *) NULL)) {
      set_errno(EINVAL);
      return(-1);
    }
    P2U_TIME(in, (*timeout));
#ifdef DEF_RR
  }
#endif

  it.it_interval.tv_sec = it.it_interval.tv_usec = 0;

  if (mode == ABS_TIME) {
    /*
     * time has already passed
     */
    if (GTEQ_TIME(now, in)) {
      set_errno(EAGAIN);
      return(-1);
    }

    MINUS_TIME(it.it_value, in, now);
  }
  else if (mode == REL_TIME) {
    /*
     * time has already passed
     */
    if (LE0_TIME(in)) {
      set_errno(EAGAIN);
      return(-1);
    }

    it.it_value.tv_sec = in.tv_sec;
    it.it_value.tv_usec = in.tv_usec;
    PLUS_TIME(in, in, now);
  }
#ifdef DEF_RR
  else if (mode == RR_TIME) {
    p->state |= T_ASYNCTIMER;
    if ((p->interval.tv_sec == 0) && (p->interval.tv_usec == 0)) {
      in.tv_sec = it.it_value.tv_sec = 0;
      in.tv_usec = it.it_value.tv_usec = TIME_SLICE;
    }
    else {
      in.tv_sec = it.it_value.tv_sec = p->interval.tv_sec; 
      in.tv_usec = it.it_value.tv_usec = p->interval.tv_usec;
    }
  }
#endif
  else {
    set_errno(EINVAL);
    return(-1);
  }

#ifdef DEF_RR
  p->num_timers++;
#endif

  /*
   * if no timer set, set timer to current request; otherwise
   * overwrite timer if current request needs to be served next
   */
  if (!(phead = pthread_timer.head) || GT_TIME(phead->tp, in)) {
    if (setitimer(ITIMER_REAL, &it, (struct itimerval *) NULL)) {
      fprintf(stderr, "ERROR: setitimer in timed_sigwait\n");
      set_errno(EINVAL);
      return(-1);
    }
#ifdef TIMER_DEBUG
    fprintf(stderr, "timed_sigwait: setitimer %d.%d sec.usec\n",
	    it.it_value.tv_sec, it.it_value.tv_usec);
#endif
  }
#ifdef TIMER_DEBUG
  else
    fprintf(stderr, "timed_sigwait: timer not set up, pthread_timer.head=%x\n", phead);
#endif
    
  /*
   * queue up current thread on the timer queue
   */
  pthread_q_timed_enq(&pthread_timer, in, mode, p);
  return(0);
}

/*------------------------------------------------------------*/
/*
 * pthread_cancel_timed_sigwait - dequeue thread waiting for alarm only
 * "signaled" indicates if the thread "p" received a SIGALRM
 * Notice: set error in both thread structure and global UNIX errno
 *         since this may be called from pthread_handle_many_process_signals
 * assumes SET_KERNEL_FLAG
 */
int pthread_cancel_timed_sigwait(first_p, signaled, mode, activate)
     pthread_t first_p;
     int signaled, mode, activate;
{
  pthread_t p = first_p;
  timer_ent_t tmr;
  timer_ent_t old_tmr_head = pthread_timer.head;
  struct itimerval it;
  struct timeval now;
  int time_read = FALSE;

  /*
   * find the first instance of this thread in timer queue
   */
#ifdef DEF_RR
  for (tmr = pthread_timer.head; tmr; tmr = tmr->next[TIMER_QUEUE])
    if (tmr->thread == p && (tmr->mode & mode))
      break;
#else
  tmr = p;
#endif

  if (!tmr) {
#ifdef TIMER_DEBUG
    fprintf(stderr, "pthread_cancel_timed_sigwait: exit0\n");
#endif
    return(0);
  }

  /*
   * for each occurrence, remove the timer entry
   */
  do {
    if (p->state & T_CONDTIMER) {
      p->state &= ~(T_CONDTIMER | T_SYNCTIMER);
      pthread_q_deq(p->queue, p, PRIMARY_QUEUE);
      pthread_q_timed_wakeup_thread(&pthread_timer, p, 
			(p == first_p ? activate : TRUE));
      mac_mutex_lock(p->cond->mutex, p);
      if (!--p->cond->waiters)
	p->cond->mutex = NO_MUTEX;
      p->cond = NO_COND;
      if (p != first_p || signaled) {
	p->context.errno = EAGAIN;

	if (p == mac_pthread_self())
	  set_errno(EAGAIN);
      }
    }  
    else {
#ifdef DEF_RR
      p->state &= ~((tmr->mode & RR_TIME) ? T_ASYNCTIMER : T_SYNCTIMER);
#else
      p->state &= ~T_SYNCTIMER;
#endif
      pthread_q_timed_wakeup_thread(&pthread_timer, p,
			 (p == first_p ? activate : TRUE));
    }

#ifdef DEF_RR
    /*
     * find next instance of this thread in timer queue
     */
    if (mode == ALL_TIME) {
      tmr = pthread_timer.head;
      while (tmr && tmr->thread != p)
	tmr = tmr->next[TIMER_QUEUE];
    }
    else
      tmr = NO_TIMER;

    /*
     * check if head of timer queue can be woken up, i.e. now > tmr->tp
     */
    if (tmr == NO_TIMER && !time_read)
#else
    if (!time_read)
#endif
      if (gettimeofday(&now, (struct timezone *) NULL)) {
#ifdef TIMER_DEBUG
	fprintf(stderr, "pthread_cancel_timed_sigwait: exit1\n");
#endif
	set_errno(EINVAL);
	return(-1);
      }
      else
	time_read = TRUE;

    if (time_read) {
      tmr = pthread_timer.head;
#ifdef DEF_RR
      if (tmr)
	p = tmr->thread;
#else
      p = tmr;
#endif
    }
      
  } while (tmr && (!time_read || GTEQ_TIME(now, tmr->tp)));

  /*
   * if head of timer queue hasn't changed, no action
   */
  if (tmr == old_tmr_head) {
#ifdef TIMER_DEBUG
    fprintf(stderr, "pthread_cancel_timed_sigwait: exit2\n");
#endif
    return(0);
  }

  /*
   * overwrite timer if current request needs to be served next or invalidate
   */
  if (tmr != NO_TIMER)
    MINUS_TIME(it.it_value, tmr->tp, now);
  else
    it.it_value.tv_sec = it.it_value.tv_usec = 0;

  it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &it, (struct itimerval *) NULL)) {
    fprintf(stderr, "ERROR: setitimer in pthread_cancel_timed_sigwait\n");
    set_errno(EINVAL);
    return(-1);
  }

  /*
   * timer signal received while cancelling => ignore it
   *
   * This assumes that between the above setitimer() and the first sigsetmask()
   * below, no SIGALRM can come in. This assumption seems reasonable since
   * the shortest time interval supported by SunOS is 10ms, enough time
   * to execute the code in between the two system calls - provided that
   * no process context switch takes place! In the latter case, a SIGALRM
   * may be lost.
   */
  if (!signaled && (new_signals | sig_handling) & sigmask(SIGALRM)) {
    sig_handling &= ~sigmask(SIGALRM);
    sigsetmask(S_DISABLE);
    new_signals &= ~sigmask(SIGALRM);
    sigsetmask(S_ENABLE);
  }
#ifdef TIMER_DEBUG
  last_alarm.tv_sec = it.it_value.tv_sec;
  last_alarm.tv_usec = it.it_value.tv_usec;
#endif

#ifdef TIMER_DEBUG
  fprintf(stderr, "pthread_cancel_timed_sigwait: setitimer %d.%d sec.usec\n",
	  it.it_value.tv_sec, it.it_value.tv_usec);
#endif
  
  return(0);
}

/*------------------------------------------------------------*/
/*
 * raise - send a signal to the current process;
 * NOT directed to any particular thread,
 * any thread waiting for the signal may pick it up.
 */
int raise(sig)
int sig;
{
  SET_KERNEL_FLAG;
  SIM_SYSCALL(TRUE);
#ifdef C_CONTEXT_SWITCH
  if (!sigsetjmp(&mac_pthread_self()->context, FALSE))
#endif
    pthread_signal_sched(sig, (int) NO_PTHREAD);
#ifdef C_CONTEXT_SWITCH
  CLEAR_KERNEL_FLAG;
#endif

  return(0);
}

/*------------------------------------------------------------*/
/*
 * pthread_kill - send signal to thread
 */
int pthread_kill(thread, sig)
pthread_t thread;
int sig;
{
  if (thread == NO_PTHREAD || thread->state & T_RETURNED ||
      sigmask(sig) & cantmask) {
    set_errno(EINVAL);
    return(-1);
  }

  /*
   * queue up signal associated with thread
   */
  SET_KERNEL_FLAG;
  SIM_SYSCALL(TRUE);
#ifdef C_CONTEXT_SWITCH
  if (!sigsetjmp(&mac_pthread_self()->context, FALSE))
#endif
    pthread_signal_sched(sig, (int) thread);
#ifdef C_CONTEXT_SWITCH
  CLEAR_KERNEL_FLAG;
#endif

  return(0);
}

/*------------------------------------------------------------*/
/*
 * pthread_cancel - cancel thread
 * Open question: is this an interruption point if a thread cancels itself?
 * As of now, it isn't!
 */
int pthread_cancel(thread)
pthread_t thread;
{
  if (thread == NO_PTHREAD || thread->state & T_RETURNED) {
    set_errno(EINVAL);
    return(-1);
  }

  /*
   * queue up signal associated with thread
   */
  SET_KERNEL_FLAG;
  SIM_SYSCALL(TRUE);
#ifdef C_CONTEXT_SWITCH
  if (!sigsetjmp(&mac_pthread_self()->context, FALSE))
#endif
    pthread_signal_sched(SIGCANCEL, (int) thread);
#ifdef C_CONTEXT_SWITCH
  CLEAR_KERNEL_FLAG;
#endif

  return(0);
}

/*------------------------------------------------------------*/
/*
 * pthread_setintr - set interruptablility state for thread cancellation
 */
int pthread_setintr(state)
     int state;
{
  int old;

  if (state != PTHREAD_INTR_ENABLE && state != PTHREAD_INTR_DISABLE) {
    set_errno(EINVAL);
    return(-1);
  }
  
  SIM_SYSCALL(TRUE);
  old = (mac_pthread_self()->mask & sigmask(SIGCANCEL) ?
	 PTHREAD_INTR_DISABLE : PTHREAD_INTR_ENABLE);
  if (pthread_change_signal_mask(state, sigmask(SIGCANCEL), 
					(sigset_t *) NULL) > 0)
    return(old);
  else
    return(-1);
}

/*------------------------------------------------------------*/
/*
 * pthread_setintrtype - set interruptablility type for thread cancellation
 */
int pthread_setintrtype(type)
     int type;
{
  register pthread_t p = mac_pthread_self();
  int old;
  
  old = (p->state & T_CONTROLLED ?
	 PTHREAD_INTR_CONTROLLED : PTHREAD_INTR_ASYNCHRONOUS);
  switch (type) {
  case PTHREAD_INTR_CONTROLLED:
    SET_KERNEL_FLAG;
    p->state |= T_CONTROLLED;
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
    return(old);
  case PTHREAD_INTR_ASYNCHRONOUS:
    SET_KERNEL_FLAG;
    p->state &= ~T_CONTROLLED;
    if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL))) {
      p->state |= T_INTR_POINT;
      SIG_CLEAR_KERNEL_FLAG(TRUE);
    }
    else {
      SIM_SYSCALL(TRUE);
      CLEAR_KERNEL_FLAG;
    }
    return(old);
  default:
    set_errno(EINVAL);
    return(-1);
  }
}

/*------------------------------------------------------------*/
/*
 * pthread_testintr - act upon pending cancellation (creates interruption point)
 */
void pthread_testintr()
{
  register pthread_t p = mac_pthread_self();

  SET_KERNEL_FLAG;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL))) {
    p->state |= T_INTR_POINT;
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  }
  else {
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }
}

/*------------------------------------------------------------*/
/*
 * sigaction - install interrupt handler for a thread on a signal
 * return 0 if o.k., -1 otherwise
 * Notice: cannot mask SIGKILL, SIGSTOP, SIGCANCEL
 */
#ifndef __GNUC_MINOR__
int sigaction(sig, act, oact)
int sig;
struct sigaction *act, *oact;
#else /* match <sys/signal.h> */
int     sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
#endif
{
  register pthread_t p = mac_pthread_self();
  register sigset_t sigset = sigmask(sig);
  struct sigvec vec;

  if (sigset & cantmask) {
    set_errno(EINVAL);
    return(-1);
  }

  if (!act) {
    if (oact)
      *oact = user_handler[sig];
    return(0);
  }

  if (act->sa_mask & cantmask) {
    set_errno(EINVAL);
    return(-1);
  }

  SET_KERNEL_FLAG;
  if (oact)
    *oact = user_handler[sig];

  user_handler[sig] = *act;

  /*
   * queue up mac_pthread_self() in the signal queue indicated
   */
  if (!(handlerset & sigset))
    handlerset |= sigset;

  /*
   * dequeue pending signals on process and threads if to be ignored
   * or perform default action on process if default action chosen
   */
  if (act->sa_handler == SIG_IGN || act->sa_handler == SIG_DFL) {
    if (pending_signals & sigset) {
      pending_signals &= ~sigset;
      if (act->sa_handler == SIG_DFL)
	default_action(sig);
    }

    for (p = all.head; p; p = p->next[ALL_QUEUE])
      if (p->pending & sigset) {
	p->pending &= ~sigset;
	if (act->sa_handler == SIG_DFL)
	  default_action(sig);
      }
  }

  /*
   * let UNIX know about sa_flags by reinstalling process handler
   * for signals that have a defined sa_flags bit
   */
  if (sig == SIGCHLD) {
    vec.sv_handler = act->sa_handler;
    vec.sv_mask = act->sa_mask;
    vec.sv_flags = (act->sa_flags & SA_NOCLDSTOP ? SA_NOCLDSTOP : 0);
    sigvec(sig, &vec, (struct sigvec *) NULL);
  }

  SIM_SYSCALL(TRUE);
  CLEAR_KERNEL_FLAG;

  return(0);
}


/*------------------------------------------------------------*/
/*
 * nanosleep - suspend until time interval specified by "rqtp" has passed
 * or a user handler is invoked or the thread is canceled
 */
int nanosleep(rqtp, rmtp)
struct timespec *rqtp, *rmtp;
{
  register pthread_t p = mac_pthread_self();
  timer_ent_t tmr;
  struct timeval now;
 
  if (rmtp)
    rmtp->tv_sec = rmtp->tv_nsec = 0;

  SET_KERNEL_FLAG;

  if (pthread_timed_sigwait(p, rqtp, REL_TIME) == -1) {
    CLEAR_KERNEL_FLAG;
    return(-1);
  }

  /*
   * clear error number before suspending
   */
  set_errno(0);

  p->state &= ~T_RUNNING;
  p->state |= T_BLOCKED | T_SYNCTIMER | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }

  /*
   * Check if condition was signaled or time-out was exceeded.
   */
  if (get_errno() == EINTR) {
    if (gettimeofday(&now, (struct timezone *) NULL)) {
      set_errno(EINVAL);
      return(-1);
    }

#ifdef DEF_RR
    if (tmr = pthread_timer.head) {
      while (tmr->thread != p)
	tmr = tmr->next[TIMER_QUEUE];
#else
    if (tmr = p) {
#endif
    
      if (GT_TIME(tmr->tp, now)) {
	if (rmtp) {
	  MINUS_TIME(now, tmr->tp, now);
	  U2P_TIME((*rmtp), now);
	}
	return(-1);
      }
    }
  }

  return(0);
}

/*------------------------------------------------------------*/
/*
 * sleep - suspend thread for time interval (in seconds)
 */
unsigned int sleep(seconds)
unsigned int seconds;
{
  struct timespec rqtp, rmtp;

  if (rqtp.tv_sec = seconds) {
    rqtp.tv_nsec = 0;
    nanosleep(&rqtp, &rmtp);
    if (get_errno() == EINTR)
      return(rmtp.tv_sec + (rmtp.tv_nsec ? 1 : 0)); /* pessimistic round-up */
  }
  return(0);
}

/*------------------------------------------------------------*/
/*
 * clock_gettime - reads the clock
 */
int clock_gettime(clock_id, tp)
int clock_id;
struct timespec *tp;
{
  struct timeval now;

  if (clock_id != CLOCK_REALTIME || !tp ||
      gettimeofday(&now, (struct timezone *) NULL)) {
    set_errno(EINVAL);
    return(-1);
  }

  U2P_TIME((*tp), now);
  return(0);
}

#ifdef IO
/*-------------------------------------------------------------*/
/*
 * aio_handle - handler for asynchronous I/O on process level,
 * demultiplexes SIGIO for threads
 */
static int aio_handle()
{
  pthread_t p;
  int count = 0;

  for(p = all.head; p; p = p->next[ALL_QUEUE]) {
    if (p->state & T_IO_OVER) {
      p->state &= ~T_IO_OVER;
      count++;
      continue;
    }
    else if (p->sigwaitset & IO_MASK &&
	p->resultp.aio_return != AIO_INPROGRESS) {
      count++;
      pthread_q_wakeup_thread(NO_QUEUE, p, NO_QUEUE_INDEX);
      p->sigwaitset &= ~IO_MASK;
    }
  }

  if (count)
    return(TRUE);
  else
    return(FALSE);
}
#endif
/*-------------------------------------------------------------*/
