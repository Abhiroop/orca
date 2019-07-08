/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"		/* Provides a system interface */
				/* RECEIVE */
static void
comm_daemon(void *arg)
{
}

				/* SEND */
static void
unicast(int pid, char *data, int size)
{
}


static void
broadcast(char *data, int size)
{
}


void
pan_sys_comm_start(void)
{
}


void
pan_sys_comm_wakeup(void)
{
}


void
pan_sys_comm_end(void)
{
}


void
pan_comm_unicast_fragment(int dest, pan_fragment_p fragment)
{
}

void
pan_comm_multicast_fragment(pan_pset_p pset, pan_fragment_p fragment)
{
}

void
pan_comm_unicast_small(int dest, pan_nsap_p nsap, void *data)
{
}

void
pan_comm_multicast_small(pan_pset_p pset, pan_nsap_p nsap, void *data)
{
}
