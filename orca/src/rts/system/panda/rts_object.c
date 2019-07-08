#include "panda/panda.h"
#include "continuation.h"
#include "obj_tab.h"
#include "fragment.h"	/* break include cycle! */
#include "rts_object.h"
#include "rts_trace.h"

void
ro_init(rts_object_p ro, char *name)
{
    int len;

    oid_init(ro);          	/* get unique id for this RTS object frag. */
    ro->flags = 0;
    sys_mutex_init(&ro->lock);
    cont_init(&ro->wlist, &ro->lock);

    ro->info.access_sum  = 0;
    ro->info.nr_accesses = 0;
    ro->info.delta_reads = 0;
    ro->owner      = RO_NO_OWNER;
    ro->total_refs = 1;
    ro->manager    = (manager_p)0;

    if (name == NULL) {
	name = oid_ascii( &ro->oid);
    }
    ro->name = (char *)sys_malloc( 20+1);
    len = strlen( name);
    strcpy( ro->name, (len <= 20 ? name : name + len-20));
}


void
ro_clear(rts_object_p ro)
{
#ifdef TRACING
    struct { int cpu; void *obj; } info;

    info.cpu = ro->oid.cpu;
    info.obj = ro->oid.rts;
    trc_event( obj_destroy, &info);
#endif

    if (ro->manager) {
	printf("%d) Don't know how to clear manager\n", sys_my_pid);
    }
    cont_clear(&ro->wlist);
    sys_mutex_clear(&ro->lock);
    oid_clear(&ro->oid);
}
