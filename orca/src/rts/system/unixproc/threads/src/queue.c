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

  @(#)queue.c	1.20 10/1/93

*/

/*
 * Implementation of queues
 */

#include "pthread_internals.h"

extern pthread_timer_q pthread_timer;
#ifdef RAND_SWITCH
int pthread_n_ready;
#endif

/*------------------------------------------------------------*/
/*
 * pthread_q_all_enq - enqueing a pthread into an unsorted queue (linked list)
 */
void pthread_q_all_enq(q, t)
     pthread_queue_t q;
     pthread_t t;
{
  pthread_t prev = NO_PTHREAD;
  pthread_t next = q->head;

#ifdef DEBUG
  if (!t || t->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_all_enq on returned thread attempted\n");
#endif

  while (next != NO_PTHREAD && next->attr.prio >= t->attr.prio) {
    prev = next;
    next = next->next[ALL_QUEUE];
  }

  t->next[ALL_QUEUE] = next;
  if (prev == NO_PTHREAD)
    q->head = t;
  else
    prev->next[ALL_QUEUE] = t;

  if (next == NO_PTHREAD)
    q->tail = t;
}

/*------------------------------------------------------------*/
/*
 * pthread_q_primary_enq - enqueing a pthread into a queue sorted by priority
 */
void pthread_q_primary_enq(q, t)
     pthread_queue_t q;
     pthread_t t;
{
  pthread_t prev = NO_PTHREAD;
  pthread_t next = q->head;

#ifdef DEBUG
  if (!t || t->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_primary_enq on returned thread attempted\n");
#endif

  while (next != NO_PTHREAD && next->attr.prio >= t->attr.prio) {
    prev = next;
    next = next->next[PRIMARY_QUEUE];
  }

  t->next[PRIMARY_QUEUE] = next;
  if (prev == NO_PTHREAD) {
    q->head = t;
    if (q == &ready)
      state_change = TRUE;
  }
  else
    prev->next[PRIMARY_QUEUE] = t;

  if (next == NO_PTHREAD)
    q->tail = t;

  t->queue = q;
#ifdef RAND_SWITCH
  if (q == &ready)
    pthread_n_ready++;
#endif
}

/*------------------------------------------------------------*/
/*
 * pthread_q_timed_enq - enqueing a pthread into a list sorted by time
 */
void pthread_q_timed_enq(q, in, mode, p)
	pthread_timer_q_t q;
	struct timeval in;
	int mode;
	pthread_t p;
{
  timer_ent_t prev = NO_TIMER;
  timer_ent_t next = q->head;
  timer_ent_t timer_ent;

#ifdef DEBUG
  if (!p || p->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_timed_enq on returned thread attempted\n");
#endif

#ifdef DEF_RR
#ifdef MALLOC
  if ((timer_ent = (timer_ent_t) pthread_malloc(sizeof(struct timer_ent))) == NULL) {
#else !MALLOC
  if ((timer_ent = (timer_ent_t) malloc(sizeof(struct timer_ent))) == NULL) {
#endif MALLOC
    set_errno(EAGAIN);
    return;
  }

  timer_ent->mode = mode;
  timer_ent->thread = p;
#else
  timer_ent = p;
#endif
  timer_ent->tp.tv_sec = in.tv_sec;
  timer_ent->tp.tv_usec = in.tv_usec;

  while (next != NO_TIMER && GT_TIME(timer_ent->tp, next->tp)) {
    prev = next;
    next = next->next[TIMER_QUEUE];
  }
 
  timer_ent->next[TIMER_QUEUE] = next;
  if (prev == NO_TIMER)
    q->head = timer_ent;
  else
    prev->next[TIMER_QUEUE] = timer_ent;
 
  if (next == NO_TIMER)
    q->tail = timer_ent;
}

/*------------------------------------------------------------*/
/*
 * pthread_q_enq_head - enqueue head of queue
 */
void pthread_q_enq_head(q, t, index)
     pthread_queue_t q;
     pthread_t t;
     int index;
{
#ifdef DEBUG
  if (!t || t->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_enq_head on returned thread attempted\n");
#endif

  if ((t->next[index] = q->head) == NO_PTHREAD)
    q->tail = t;
  q->head = t;
  if (!(t->queue))
    t->queue = q;
  if (q == &ready) {
    state_change = TRUE;
#ifdef RAND_SWITCH 
    pthread_n_ready++; 
#endif
  }
}

#if defined(RR_SWITCH) || defined(RAND_SWITCH)
/*------------------------------------------------------------*/
/*
 * pthread_q_enq_tail - enqueing a pthread into a queue at the tail (RR)
 */
void pthread_q_enq_tail(q)
     pthread_queue_t q;
{
  pthread_t prev = NO_PTHREAD;
  pthread_t next = q->head;
  pthread_t t = mac_pthread_self();
 
#ifdef DEBUG
  if (!t || t->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_primary_enq on returned thread attempted\n");
#endif
 
  while (next != NO_PTHREAD ) {
    prev = next;
    next = next->next[PRIMARY_QUEUE];
  }
 
  t->next[PRIMARY_QUEUE] = NO_PTHREAD;
  if (prev == NO_PTHREAD) {
    q->head = t;
    if (q == &ready)
      state_change = TRUE;
  }
  else
    prev->next[PRIMARY_QUEUE] = t;
 
  if (!(t->queue))
    t->queue = q;
  q->tail = t;
#ifdef RAND_SWITCH 
  if (q == &ready)
    pthread_n_ready++; 
#endif
}
#endif

/*------------------------------------------------------------*/
/*
 * pthread_q_deq - dequeue a thread from a queue
 */
void pthread_q_deq(q, t, index)
     pthread_queue_t q;
     pthread_t t;
     int index;
{
  pthread_t prev = NO_PTHREAD;
  pthread_t next = q->head;
  timer_ent_t tmr;
  struct timeval now;

  while (next != NO_PTHREAD && next != t) {
    prev = next;
    next = next->next[index];
  }

  if (next == NO_PTHREAD)
    return;

  if ((next = next->next[index]) == NO_PTHREAD)
    q->tail = prev;
  
  if (prev == NO_PTHREAD) {
    q->head = next;
    if (q == &ready)
      state_change = TRUE;
  }
  else
    prev->next[index] = next;
  
  t->next[index] = NO_PTHREAD;
  
  if (index == PRIMARY_QUEUE) {
    t->queue = NULL;
#ifdef DEF_RR
    if (t == mac_pthread_self() && t->attr.sched == SCHED_RR &&
	t->state & T_ASYNCTIMER) {
      for (tmr = pthread_timer.head; tmr; tmr = tmr->next[TIMER_QUEUE])
	if (tmr->thread == t && tmr->mode == RR_TIME)
	  break;
      if (tmr && !gettimeofday(&now, (struct timezone *) NULL) &&
	  GT_TIME(tmr->tp, now))
	MINUS_TIME(t->interval, tmr->tp, now);
      else
	t->interval.tv_usec = t->interval.tv_sec = 0;

      pthread_cancel_timed_sigwait(t, FALSE, RR_TIME, FALSE);
    }
#endif
  }

#ifdef RAND_SWITCH 
  if (q == &ready)
    pthread_n_ready--; 
#endif
}

/*------------------------------------------------------------*/
/*
 * pthread_q_timed_deq - dequeue thread from (timer) queue
 */
void pthread_q_timed_deq(q, t)
	pthread_timer_q_t q;
	pthread_t t;
{
  timer_ent_t temp, prev = NO_TIMER;
  timer_ent_t next = q->head;
 
#ifdef DEF_RR
  while (next != NO_TIMER && next->thread != t) {
#else
  while (next != NO_TIMER && next != t) {
#endif
    prev = next;
    next = next->next[TIMER_QUEUE];
  }
 
  if (next == NO_TIMER)
    return;
 
  temp = next;
  if ((next = next->next[TIMER_QUEUE]) == NO_TIMER)
    q->tail = prev;
 
  if (prev == NO_TIMER)
    q->head = next;
  else
    prev->next[TIMER_QUEUE] = next;

#ifdef DEF_RR
#ifdef MALLOC
  pthread_free(temp);
#else !MALLOC
  free(temp);
#endif
  t->num_timers--;
#endif
}

/*------------------------------------------------------------*/
/*
 * pthread_q_deq_head - dequeue head of queue and return head
 */
pthread_t pthread_q_deq_head(q, index)
     pthread_queue_t q;
     int index;
{
  pthread_t t;

  if ((t = q->head) != NO_PTHREAD) {
    if (q == &ready)
      state_change = TRUE;
    if ((q->head = t->next[index]) == NO_PTHREAD)
      q->tail = NO_PTHREAD;
    else
      t->next[index] = NO_PTHREAD;

    if (index == PRIMARY_QUEUE) {
      t->queue = NULL;
#ifdef DEF_RR
      if (t == mac_pthread_self() && t->attr.sched == SCHED_RR &&
	  t->state & T_ASYNCTIMER)
        pthread_cancel_timed_sigwait(t, FALSE, RR_TIME, FALSE);
#endif
    }
  }

#ifdef RAND_SWITCH  
  if (q == &ready)
    pthread_n_ready--;  
#endif

  return(t);
}

#ifdef RAND_SWITCH
/*------------------------------------------------------------*/
/*
 * pthread_q_exchange_rand - Remove the current thread from the queue and
 *                   put that at the end of the queue and put the
 * thread #index in the queue at the head.
 */
void pthread_q_exchange_rand(q)
     pthread_queue_t q;
{
  pthread_t t = mac_pthread_self();
  int i, index = (int)random() % pthread_n_ready;

#ifdef DEBUG                        
  if (!t || t->state & T_RETURNED)
    fprintf(stderr, "ERROR: q_primary_enq on returned thread attempted\n");
#endif

  pthread_q_deq(q, t, PRIMARY_QUEUE);
  pthread_q_enq_tail(q);
  t = q->head;
  for (i = 0; i < index; i++)
    t = t->next[PRIMARY_QUEUE];

  pthread_q_deq(q, t, PRIMARY_QUEUE);
  pthread_q_enq_head(q, t, PRIMARY_QUEUE);
}
#endif

/*------------------------------------------------------------*/
/*
 * pthread_q_all_find_receiver - find thread in all queue s.t.
 * (a) either a handler is installed and the thread has the signal unmasked,
 * (b) or the signal is in the sigwaitset,
 * and return this thread (or NO_THREAD).
 */
pthread_t pthread_q_all_find_receiver(q, sigset)
     pthread_queue_t q;
     sigset_t sigset;
{
  pthread_t t, temp;
  int handler = handlerset & sigset;

  temp = NO_PTHREAD;

  for (t = q->head; t != NO_PTHREAD; t = t->next[ALL_QUEUE])
    if ((handler || t->state & (T_SIGWAIT | T_SIGSUSPEND) &&
	 t->sigwaitset & sigset) && !(t->mask & sigset))
      break;

  return(t);
}

/*------------------------------------------------------------*/
/*
 * pthread_q_sleep_thread - block current thread: move from anywhere in ready
 * queue to some other queue;
 * Assumes SET_KERNEL_FLAG
 */
void pthread_q_sleep_thread(q, p, index)
pthread_queue_t q;
pthread_t p;
int index;
{
  pthread_q_deq(&ready, p, PRIMARY_QUEUE);
  if (index == PRIMARY_QUEUE)
    pthread_q_primary_enq(q, p);
  p->state &= ~T_RUNNING;
  p->state |= T_BLOCKED;
}

/*------------------------------------------------------------*/
/*
 * pthread_q_sleep - block current thread: move from head of ready to some 
 * other queue; Assumes SET_KERNEL_FLAG
 */
void pthread_q_sleep(q, index)
pthread_queue_t q;
int index;
{
  pthread_t p;
  
  p = pthread_q_deq_head(&ready, PRIMARY_QUEUE);
  if (index == PRIMARY_QUEUE)
    pthread_q_primary_enq(q, p);
  p->state &= ~T_RUNNING;
  p->state |= T_BLOCKED;
}

/*------------------------------------------------------------*/
/* 
 * pthread_q_wakeup -  Wakeup head of the queue and return head.
 * If there one exists, deq him and put him on the run queue ready
 * to execute.  Note: Deq takes care of who runs when., so scheduling
 * goes on fine!
 * Assumes SET_KERNEL_FLAG
 */
void pthread_q_wakeup(q, index)
     pthread_queue_t q;
     int index;
{
  pthread_t p;

  p = pthread_q_deq_head(q, index);
  if (p != NO_PTHREAD && !(p->state & T_RUNNING)) {
    p->state &= ~(T_BLOCKED | T_INTR_POINT);
    p->state |= T_RUNNING;
    pthread_q_primary_enq(&ready, p);
  }
}

/*------------------------------------------------------------*/
/*
 * pthread_q_wakeup_thread -  same as pthread_q_wakeup but for specific thread
 * return pointer to thread if thread found in queue, NO_PTHREAD otherwise
 */
void pthread_q_wakeup_thread(q, p, index)
     pthread_queue_t q;
     pthread_t p;
{
  if (q != NO_QUEUE)
    pthread_q_deq(q, p, index);

  if (p != NO_PTHREAD && !(p->state & T_RUNNING)) {
    p->state &= ~(T_BLOCKED | T_INTR_POINT);
    p->state |= T_RUNNING;
    pthread_q_primary_enq(&ready, p);
  }
}
      
/*------------------------------------------------------------*/
/*
 * pthread_q_timed_wakeup_thread - same as pthread_q_wakeup_thread but 
 * for timer queue
 */
void pthread_q_timed_wakeup_thread(q, p, activate)
     pthread_timer_q_t q;
     pthread_t p;
     int activate;
{
  if (q != NO_TIMER_QUEUE)
    pthread_q_timed_deq(q, p);
 
  if (activate && p != NO_PTHREAD && !(p->state & T_RUNNING)) {
    p->state &= ~(T_BLOCKED | T_INTR_POINT);
    p->state |= T_RUNNING;
    pthread_q_primary_enq(&ready, p);
  }
}
 
/*------------------------------------------------------------*/
/*
 * pthread_q_wakeup_all - Wake up all the threads waiting on a condition   
 * change.  See pthread_q_wakeup.
 * Assumes SET_KERNEL_FLAG
 */
void pthread_q_wakeup_all(q, index)
     pthread_queue_t q;
{
  pthread_t p;
  
  while ((p = pthread_q_deq_head(q, index)) != NO_PTHREAD)
    if (!(p->state & T_RUNNING)) {
      p->state &= ~(T_BLOCKED | T_INTR_POINT);
      p->state |= T_RUNNING;
      pthread_q_primary_enq(&ready, p);
    }
}
/*------------------------------------------------------------*/
#ifdef _POSIX_THREADS_PRIO_PROTECT
/*
 * pthread_mutex_q_adjust - Removes self from ready queue and reinserts
 * itself in the ready queue in the first place among threads of same
 * priority. Unlocking a mutex should not take it to the end.
 * Assumes SET_KERNEL_FLAG
 */
void pthread_mutex_q_adjust()
{
  pthread_t prev = NO_PTHREAD;
  pthread_t next = ready.head;
  pthread_t p = mac_pthread_self();
#ifdef DEF_RR
  timer_ent_t tmr;
  struct timeval now;
#endif
 
  if (next == p &&
      p->attr.prio >= p->next[PRIMARY_QUEUE]->attr.prio)
    return;
 
  while (next != NO_PTHREAD && next != p) {
    prev = next;
    next = next->next[PRIMARY_QUEUE];
  }
 
  if (next == NO_PTHREAD)
    return;
 
  if ((next = next->next[PRIMARY_QUEUE]) == NO_PTHREAD)
    ready.tail = prev;
  
  if (prev == NO_PTHREAD) {
    ready.head = next;
    state_change = TRUE;
  }
  else
    prev->next[PRIMARY_QUEUE] = next;
  
#ifdef DEF_RR
    if (p->attr.sched == SCHED_RR && p->state & T_ASYNCTIMER) {
      for (tmr = pthread_timer.head; tmr; tmr = tmr->next[TIMER_QUEUE])
        if (tmr->thread == p && tmr->mode == RR_TIME)
          break;
      if (tmr && !gettimeofday(&now, (struct timezone *) NULL) &&
          GT_TIME(tmr->tp, now))
        MINUS_TIME(p->interval, tmr->tp, now);
      else
        p->interval.tv_usec = p->interval.tv_sec = 0;
 
      pthread_cancel_timed_sigwait(p, FALSE, RR_TIME, FALSE);
    }
#endif
     
  next = ready.head;
  prev = NO_PTHREAD;
  while (next != NO_PTHREAD && next->attr.prio > p->attr.prio) {
    prev = next;
    next = next->next[PRIMARY_QUEUE];
  }
  p->next[PRIMARY_QUEUE] = next;
  prev->next[PRIMARY_QUEUE] = p;
  if (next == NO_PTHREAD)
    ready.tail = p;
 
}
/*------------------------------------------------------------*/
#endif
