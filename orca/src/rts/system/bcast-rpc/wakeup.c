/* $Id: wakeup.c,v 1.3 1996/07/04 08:53:04 ceriel Exp $ */

#include "interface.h"

/* remote wakeup */
WakeUp(cpu, process)
	int cpu;
	void *process;
{
	if (cpu == this_cpu) {
		local_wakeup(process);
	}
}
