/* $Id: time.c,v 1.10 1995/07/25 13:51:38 ceriel Exp $ */

#include <orca_types.h>
#include "module/syscall.h"
#include "module/mutex.h"

static unsigned long	base;

t_integer
f_Time__GetTime()
{
	return (sys_milli() - base)/100;
}

t_integer
f_Time__SysMilli()
{
	return sys_milli() - base;
}

t_real
f_Time__SysMicro()
{
	return (sys_milli() - base) * 1000.0;
}

void f_Time__Sleep(t_integer n_sec, t_integer n_nanosec)
{
	interval	n = n_sec * 1000 + n_nanosec / 1000000;
	mutex		mu;

	if (n > 0) {
		mu_init(&mu);
		mu_lock(&mu);
		(void) mu_trylock(&mu, n);
	}
}

void
(ini_Time__Time)()
{
	base = sys_milli();
}
