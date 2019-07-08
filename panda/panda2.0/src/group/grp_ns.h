/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Name server tailored towards group communication.
 * Provides one service:
 *     pan_ns_lookup_or_register("group name", &group_id, &sequencer_platform);
 * The server is conceptually single-threaded;
 * The first client who does a lookup_or_register of a group_name, becomes
 * the sequencer. The server binds group_name to (1) a new, unique group_id
 * and (2) the platform number of the sequencer. Later clients for the
 * same group receive the group_id and the sequencer address, they become
 * ordinary group members.
 * This server runs on one platform only, and its address is known to
 * all platforms in the world (i.e. all platforms returned in pan_pset_fill()).
 *
 * THIS IMPLEMENTATION IS A HACK!!!!
 * The algorithm to find out whether this is the server's platform:
 * all processes start enumerating all legal platform numbers, and the first
 * one that exists in pan_pset_fill is the server platform.
 *
 *     Anyway, since the name server only services group communication, these
 *     routines are called from the group demuxer daemon, and the nameserver
 *     message header format is merged with the group header format.
 */

#ifndef _GROUP_PAN_GRP_NAMESERVER_H
#define _GROUP_PAN_GRP_NAMESERVER_H

#include "pan_sys.h"

#include "grp_types.h"


void   pan_ns_register_or_lookup(char *name, int *gid, int *sequencer);

void   pan_ns_unregister_group(int gid);

void   pan_ns_init(void);

void   pan_ns_await_size(int n);

void   pan_ns_clear(boolean await_unregisters);

void   pan_ns_statistics(void);

#endif
