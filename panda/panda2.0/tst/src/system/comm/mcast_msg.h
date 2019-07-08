#ifndef __MCAST_MSG_H__
#define __MCAST_MSG_H__

#include "pan_sys.h"

/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 */



void	mcast_msg(pan_msg_p msg);

void	mcast_msg_start(pan_pset_p set, void (*upcall)(pan_msg_p), int snc,
			int poll, int v);

void	mcast_msg_end(int statistics);

#endif
