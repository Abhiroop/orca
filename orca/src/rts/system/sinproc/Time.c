/* $Id: Time.c,v 1.10 1998/09/02 16:19:52 ceriel Exp $ */

#include <interface.h>
#include "Time.h"

#ifndef SOLARIS2

#include <sys/time.h>
#include <sys/resource.h>

t_integer f_Time__GetTime(void)
{
  struct rusage buffer;

  getrusage(RUSAGE_SELF, &buffer);
  return buffer.ru_utime.tv_sec*10+buffer.ru_utime.tv_usec/100000;	/* user time in deci-seconds */
}

t_integer f_Time__SysMilli(void)
{
  struct rusage buffer;

  getrusage(RUSAGE_SELF, &buffer);
  return buffer.ru_utime.tv_sec*1000+buffer.ru_utime.tv_usec/1000;	/* user time in milli-seconds */
}

t_real f_Time__SysMicro(void)
{
  struct rusage buffer;

  getrusage(RUSAGE_SELF, &buffer);
  return buffer.ru_utime.tv_sec*1000000.0+buffer.ru_utime.tv_usec;	/* user time in micro-seconds */
}

#else /* SOLARIS2 */

#include <sys/times.h>
#include <limits.h>

t_integer f_Time__GetTime(void)
{
  struct tms buffer;

  (void) times(&buffer);
  return (buffer.tms_utime * 10)/CLK_TCK;	/* user time in deci-seconds */
}

t_integer f_Time__SysMilli(void)
{
  struct tms buffer;

  (void) times(&buffer);
  if (1000 % CLK_TCK) {
  	return (buffer.tms_utime * 1000)/CLK_TCK;	/* user time in milli-seconds */
  }
  return buffer.tms_utime * (1000/CLK_TCK);	/* user time in milli-seconds */
}
t_real f_Time__SysMicro(void)
{
  struct tms buffer;

  (void) times(&buffer);
  return (buffer.tms_utime * 1000000.0)/CLK_TCK;	/* user time in micro-seconds */
}
#endif /* SOLARIS2 */

#include <time.h>
#include <errno.h>

void f_Time__Sleep(t_integer sec, t_integer nanosec)
{
#ifdef SOLARIS2
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
#else
  if (sec < 2000) {
	usleep(sec*1000000+nanosec/1000);
  }
  else {
  	sleep(sec);
  	usleep(nanosec/1000);
  }
#endif
}

void
(ini_Time__Time)()
{
    static int done = 0;

    if (! done) {
	done = 1;
	ini_InOut__InOut();
    }
}
