/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include <string.h>
#include <stdio.h>

#include "pan_sys.h"
#include "continuation.h"
#include "obj_tab.h"
#include "fragment.h"	/* break include cycle! */
#include "rts_object.h"
#include "rts_trace.h"

#define NAMELEN       19      /* 8-fold - 4 - 1 to avoid padding */

void
ro_init(fragment_p obj, char *name)
{
    rts_object_p ro = &obj->fr_rtsobj;
    int len;

    ro->oid = OBJ_UNKNOWN;
#ifdef TRACING
    ro->home = rts_my_pid;
    ro->home_obj = obj;
#endif
    ro->flags = 0;
    ro->lock = pan_mutex_create();
    cont_init(&ro->wlist, ro->lock);

    ro->info.access_sum  = 0;
    ro->info.nr_accesses = 0;
    ro->info.delta_reads = 0;
    ro->owner      = RO_NO_OWNER;
    ro->total_refs = 1;
    ro->manager    = (manager_p)0;

    ro->name = (char *)m_malloc( NAMELEN+1);
    if (name) {
	len = strlen(name);
	strcpy( ro->name, (len <= NAMELEN ? name : name + len-NAMELEN));
    } else {
	strcpy( ro->name, "OBJ_UNKNOWN");
    }
}

void
ro_clear(fragment_p obj)
{
    rts_object_p ro = &obj->fr_rtsobj;

#ifdef TRACING
    struct {int cpu; fragment_p obj;} info;

    info.cpu = ro->home;
    info.obj = ro->home_obj;
    trc_event(obj_destroy, &info);
#endif

    if (ro->manager) {
	printf("%d) Don't know how to clear manager\n", rts_my_pid);
    }
    cont_clear(&ro->wlist);
    pan_mutex_clear(ro->lock);
    m_free(obj->fr_name);
    obj->fr_name = 0;
}
