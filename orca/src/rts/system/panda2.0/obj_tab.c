/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* New type of object ids to avoid hashing and locking when finding a
 * fragment through its identifier.
 *
 * An object identifier is a system-wide unique integer. Objects
 * are assigned an object identifier when they are first passed
 * as a shared parameter in a FORK. Until then, their object id
 * equals OBJ_UNKNOWN.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "pan_sys.h"
#include "obj_tab.h"

static fragment_p otab;         /* local object table. */
static unsigned max_entries;	/* length of the object table */
static unsigned g_next;		/* next (global) location in object table */

void
otab_start(void)
{
    max_entries = 1000;
    otab = m_malloc(max_entries * sizeof(fragment_t));
}

void
otab_end(void)
{
    int i, not_empty = 0;
    
    /* Check if all entries are empty
     */
    for(i = 0; i < g_next; i++) {
	if (otab[i].fr_rts) {   
	    not_empty++;
	}
    }
    
    if (not_empty > 0) {
	fprintf(stderr, "%d) warning: object table has %d nonempty entries\n",
		rts_my_pid, not_empty);
    }
    
    m_free(otab);
    otab = 0;
}

int
otab_enter(fragment_p obj, int home)
{
    int newid = g_next * sizeof(fragment_t);

    /* Is there room for another entry in the object table? */
    if (g_next == max_entries) {
	max_entries *= 2;
	otab = m_realloc(otab, max_entries * sizeof(fragment_t));
    }
    
    if (home == rts_my_pid) {
	otab[g_next] = *obj;     /* copy local object pointers into table */
	obj->fr_oid  = newid;    /* assign unique id to object */
	g_next++;

#ifdef RTS_VERBOSE
	printf("%d) entered object %s: offset = %d, obj->fields = %p\n",
	       rts_my_pid, obj->fr_name, g_next, obj->fr_fields);
#endif
    } else {
	otab[g_next].fr_rts = 0;        /* no local copy available */
	otab[g_next].fr_fields = 0;
	g_next++;

#ifdef RTS_VERBOSE
	printf("%d) new globid at offset %d\n", rts_my_pid, g_next);
#endif
    }

    return newid; 
}

void
otab_install(fragment_p obj)
{
    fragment_p p;

    assert(obj->fr_oid != OBJ_UNKNOWN);
    p = (fragment_p)((char *)otab + obj->fr_oid);
    assert(!p->fr_rts && !p->fr_fields);
    *p = *obj;
    assert(p->fr_rts && p->fr_fields);
}

fragment_p
otab_lookup(int oid)
{
    fragment_p obj;

    /* Objects should not be looked up before they have been given
     * a global object id (I think...)
     */
    assert(oid != OBJ_UNKNOWN);

    obj = (fragment_p) ((char *)otab + oid);
    return (obj->fr_rts ? obj : 0);
}

void
otab_remove(int oid)
{
    fragment_p obj;

    if (oid == OBJ_UNKNOWN) {
	return;
    }

    obj = (fragment_p) ((char *)otab + oid);
    if (obj->fr_rts) {
	obj->fr_rts = 0;
    }
}

