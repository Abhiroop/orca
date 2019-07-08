/* New type of object ids to avoid hashing and locking when finding a
 * fragment through its identifier.
 *
 * Object ids consist of two parts: a global identifier and a local identifier
 * Initially an object is assigned a system wide unique local identifier 
 * (fragment pointer + cpu-id) and its global id is set to unknown.
 * When an object is passed as a shared parameter to a new process, this
 * module assigns a global id to it (as part of handling the FORK message).
 * The global identifier is just an offset in the object_table, so future
 * references can be looked up very fast.
 * The RTS takes care that all platforms process the arguments of FORK
 * messages in the same order such that the global identifiers can be handed
 * out by increasing a local counter.
 *
 * FORK messages are send asynchronously, which implies that the same object can
 * be handed out multiple times before its global identifier is installed.
 * Therefore we have to check for duplicates whenever an "unknown" id is being
 * looked up.
 */

#include <limits.h>
#include <stdio.h>
#include "panda/panda.h"
#include "obj_tab.h"
#include "policy.h"



static otab_entry_p otab;	/* mapping between oid_t and fragment_t */
static unsigned max_entries;	/* length of the object table */
static unsigned g_next;		/* next (global) location in the object table */

#define UNKNOWN	 (-1)		/* any illegal offset in obj table will do*/

void
otab_start(void)
{
    g_next = 0;
    max_entries = 1000;
    otab = (otab_entry_p)sys_malloc(max_entries*sizeof(otab_entry_t));
}


void
otab_end(void)
{
    register int i;
    int not_empty = 0;

    /* Check if all entries are empty
     */
    for(i = 0; i < g_next; i++) {
	if (otab[i].frag.fr_rts) {   
	    not_empty++;
	}
    }
    if (not_empty > 0) {
	fprintf(stderr, "%d) warning: object table has %d nonempty buckets\n",
		sys_my_pid, not_empty);
    }
}


char *
oid_ascii(oid_p oid)
{
    static char buf[128];

    if (oid->g_offset == UNKNOWN) {
	(void)sprintf(buf, "UNKNOWN(0x%x@%d)", (unsigned) oid->rts,
		      (int)oid->cpu);
    } else {
	(void)sprintf(buf, "%d(0x%x@%d)", oid->g_offset/sizeof(otab_entry_t),
		      (unsigned) oid->rts, (int) oid->cpu);
    }
    return buf;
}


void
oid_init(rts_object_p rts)
{
    rts->oid.g_offset = UNKNOWN;
    rts->oid.cpu      = sys_my_pid;
    rts->oid.rts      = rts;
}


otab_entry_p
otab_add_entry( oid_p oid, fragment_p frag, int install)
{
    otab_entry_p p;

    /* Seen this object before? (new objects at the end,
     * so search backwards)
     */
    for (p = &otab[g_next]; p-- > otab;) {
        if (p->oid.rts == oid->rts && p->oid.cpu == oid->cpu) {
	    /* This is a known object */

	    if (p->frag.fr_rts) {
		return p;
	    }
	    
	    /* a valid oid? */
	    if (frag) {
		p->frag = *frag;
		p->frag.fr_oid.g_offset = p->oid.g_offset;	/* lock frag? */

/* printf("%d) installing fragment: offset = %d, size = %d, oid->rts = %p\n",
       sys_my_pid, p - otab, td_objrec(f_get_type(&p->frag))->td_size, oid->rts); */
		return(p);
	    }
	    return 0;
	}
    }

    if (!install) {
	return 0;
    }

    /* Is there room for another entry in the object table? */
    p = &otab[g_next++];
    if (g_next == max_entries) {
	max_entries *= 2;
	otab = (otab_entry_p)sys_realloc(otab, max_entries*sizeof(otab_entry_t));
	p = &otab[g_next-1];
    }

    /* Fill in new entry */
    p->oid.g_offset = (char *)p - (char *)otab;
    p->oid.cpu      = oid->cpu;
    p->oid.rts      = oid->rts;
    
    if (frag) {
    	p->frag = *(fragment_p)frag;
	p->frag.fr_oid.g_offset = p->oid.g_offset;    /* lock frag? */

/* printf("%d) entered object: offset = %d, field size = %d, oid->rts = %p\n",
       sys_my_pid, g_next - 1, td_objrec(f_get_type(&p->frag))->td_size, oid->rts); */

    	return p;
    } else {
	/* just a new global identifier? */
    	p->frag.fr_rts = 0;

/* printf("%d) new globid (%d, %p) at offset %d\n", sys_my_pid, oid->cpu, oid->rts, g_next - 1); */

    	return 0;
    }
}


otab_entry_p
otab_enter(fragment_p f)
{
    return otab_add_entry(&f->fr_oid, f, 1);
}


void
otab_remove(otab_entry_p entry)
{
    if (!entry) abort();
/*  fprintf(stderr, "%d) removing object at offset %d\n",
	    sys_my_pid, entry - otab); */
    entry->frag.fr_rts = 0;
}


otab_entry_p
otab_find_enter(oid_p oid)
{
    otab_entry_p p;

    /* If the object has a global id, then return
     * whatever is found in its table entry.
     */
    if (oid->g_offset != UNKNOWN) {
	p = (otab_entry_p) ((char *)otab + oid->g_offset);
	return (p->frag.fr_rts == NULL ? NULL : p);
    }

    /* If the object does not yet have a global idea, but was created
     * on this cpu, then use its local id (cpu, rts pointer) to find it.
     * In this case, we _must_ be able to locate the object.
     */
    if (oid->cpu == sys_my_pid) {
	return otab_add_entry(oid, oid->rts->frag, 1);
    }

    return otab_add_entry(oid, NULL, 1);
}


otab_entry_p
otab_lookup(oid_p oid)
{
    otab_entry_p p;

    /* If the object has a global id, then return
     * whatever is found in its table entry.
     */
    if (oid->g_offset != UNKNOWN) {
	p = (otab_entry_p) ((char *)otab + oid->g_offset);
	return (p->frag.fr_rts ? p : 0);
    }

    /* If the object does not yet have a global idea, but was created
     * on this cpu, then use its local id (cpu, rts pointer) to find it.
     * In this case, we _must_ be able to locate the object.
     */
    if (oid->cpu == sys_my_pid) {
	return otab_add_entry(oid, oid->rts->frag, 0);
    }

    return otab_add_entry(oid, 0, 0);
}
