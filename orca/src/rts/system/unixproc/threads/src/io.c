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

  @(#)io.c	1.20 10/1/93
  
*/

#include "pthread_internals.h"
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>

#ifdef IO

#if STACK_CHECK && SIGNAL_STACK
/*
 * aioread/write may cause a stack overflow in the UNIX kernel which cannot
 * be caught by sighandler. This seems to be a bug in SunOS. We get
 * around this problem by deliberately trying to access a storage
 * location on stack about a page ahead of where we are. This will
 * cause a premature stack overflow (SIGBUS) which *can* be caught
 * by sighandler.
 */
  extern int pthread_page_size;
#define ACCESS_STACK \
  MACRO_BEGIN \
    if (*(int *) (pthread_get_sp() - pthread_page_size)) \
      ; \
  MACRO_END

#else
#define ACCESS_STACK
#endif

/*------------------------------------------------------------*/
/*
 * read - Same as POSIX.1 read except that it blocks only the current thread
 * rather than entire process.
 */
int read (fd, buf, nbytes)
     int fd, nbytes;
     char *buf;
{
  int mode;
  struct iovec iov[1];
  pthread_t p;

  /*
   * If the mode is FNDELAY perform a Non Blocking read and return immediately.
   */
  if ((mode = fcntl (fd, F_GETFL, 0)) & FNDELAY) {
    iov[0].iov_base = buf;
    iov[0].iov_len = nbytes;
    return (readv (fd, iov, 1));
  }

  ACCESS_STACK;

  /*
   * Else issue an asynchronous request for nbytes.
   */
  p = mac_pthread_self();
  SET_KERNEL_FLAG;
  p->resultp.aio_return = AIO_INPROGRESS;
  if (aioread (fd, buf, nbytes, 0l, SEEK_CUR, &p->resultp) < 0) {
    CLEAR_KERNEL_FLAG;
    return (-1);
  }
  if (p->resultp.aio_return != AIO_INPROGRESS) {
    if (p->resultp.aio_return != -1)
      lseek(fd, p->resultp.aio_return, SEEK_CUR);
    else
      set_errno (p->resultp.aio_errno);
    p->state |= T_IO_OVER;
    CLEAR_KERNEL_FLAG;
    return(p->resultp.aio_return);
  }
  p->sigwaitset |= IO_MASK;
  p->state &= ~T_RUNNING;
  p->state |= T_BLOCKED | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }

  /*
   * The case when the read() is interrupted (when the thread receives a
   * signal other than SIGIO), read() returns -1 and errno is set to EINTR
   * (this is done in signal.c when the thread is woken up).
   */
  switch (p->resultp.aio_return) {
  case AIO_INPROGRESS:
    aiocancel(&p->resultp);
    return (-1);
  case -1:
    set_errno (p->resultp.aio_errno);
    return (-1);
  default:
    lseek(fd, p->resultp.aio_return, SEEK_CUR);
    return (p->resultp.aio_return);
  }
}

/*------------------------------------------------------------*/
/*
 * write - Same as POSIX.1 write except that it blocks only the current
 * thread rather than entire process.
 */
int write (fd, buf, nbytes)
     int fd, nbytes;
     char *buf;
{

  int mode;
  struct iovec iov[1];
  pthread_t p;

  /*
   * If the mode is FNDELAY perform a Non Blocking write and return immediately.
   */
  if ((mode = fcntl (fd, F_GETFL, 0)) & FNDELAY) {
    iov[0].iov_base = buf;
    iov[0].iov_len = nbytes;
    return (writev (fd, iov, 1));
  }

  ACCESS_STACK;

  /*
   * Else issue an asynchronous request for nbytes.
   */
  p = mac_pthread_self();
  SET_KERNEL_FLAG;
  p->resultp.aio_return = AIO_INPROGRESS;
  if (aiowrite(fd, buf, nbytes, 0l, SEEK_CUR, &p->resultp) < 0) {
    CLEAR_KERNEL_FLAG;
    return (-1);
  }
  if (p->resultp.aio_return != AIO_INPROGRESS) {
    if (p->resultp.aio_return != -1)
      lseek(fd, p->resultp.aio_return, SEEK_CUR);
    else
      set_errno (p->resultp.aio_errno);
    p->state |= T_IO_OVER;
    CLEAR_KERNEL_FLAG;
    return(p->resultp.aio_return);
  }
  p->sigwaitset |= IO_MASK;
  p->state &= ~T_RUNNING;
  p->state |= T_BLOCKED | T_INTR_POINT;
  if (p->pending & sigmask(SIGCANCEL) && !(p->mask & sigmask(SIGCANCEL)))
    SIG_CLEAR_KERNEL_FLAG(TRUE);
  else {
    pthread_q_deq_head(&ready, PRIMARY_QUEUE);
    SIM_SYSCALL(TRUE);
    CLEAR_KERNEL_FLAG;
  }
  switch (p->resultp.aio_return) {
  case AIO_INPROGRESS:
    aiocancel(&p->resultp);
    return (-1);
  case -1:
    set_errno (p->resultp.aio_errno);
    return -1;
  default:
    lseek(fd, p->resultp.aio_return, SEEK_CUR);
    return (p->resultp.aio_return);
  }
}

/*------------------------------------------------------------*/
#if STACK_CHECK && SIGNAL_STACK
/*
 * pthread_io_end - dummy function used to do pc mapping to check
 *                  stack_overflow.
 */
void pthread_io_end()
{
  return;
}
/*------------------------------------------------------------*/
#endif
#endif
