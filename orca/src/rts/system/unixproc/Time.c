/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: Time.c,v 1.12 1998/12/09 18:15:05 ceriel Exp $ */

#include <interface.h>
#include "Time.h"

#include <sys/time.h>

t_integer f_Time__GetTime(void)
{
  struct timeval buffer;

  gettimeofday(&buffer, (void *) 0);
  return buffer.tv_sec*10+buffer.tv_usec/100000; /* time in deci-seconds */
}

t_integer f_Time__SysMilli(void)
{
  struct timeval buffer;

  gettimeofday(&buffer, (void *) 0);
  return buffer.tv_sec*1000+buffer.tv_usec/1000; /* time in milli-seconds */
}

t_real f_Time__SysMicro(void)
{
  struct timeval buffer;

  gettimeofday(&buffer, (void *) 0);
  return buffer.tv_sec*1000000.0+buffer.tv_usec; /* time in micro-seconds */
}

#ifdef BSDI
#include <unistd.h>
void f_Time__Sleep(t_integer sec, t_integer nanosec)
{
  unsigned int nusec = nanosec/1000 + sec * 1000000;

  usleep(nusec);
}

#else
#include <time.h>
#include <errno.h>

void f_Time__Sleep(t_integer sec, t_integer nanosec)
{
  struct timespec v;

  v.tv_sec = sec;
  v.tv_nsec = nanosec;

  while (nanosec < 0 || nanosec >= 1000000000) {
	sec++;
	nanosec -= 1000000000;
  }
  while (nanosleep(&v, &v) != 0) {
	if (errno == ENOSYS) break;
  }
}
#endif

void
(ini_Time__Time)()
{
    static int done = 0;
    if (! done) {
	done = 1;
    	ini_InOut__InOut();
    }
}
