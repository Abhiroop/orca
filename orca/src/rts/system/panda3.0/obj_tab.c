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

/*
 * The otab should be persistent, since we hand out pointers into it.
 * Previously, otab was realloc'ed when it becomes too small.
 * This is changed to an implementation with an array of chunks,
 * and lookup first finds the entry into the array of chunks, then the
 * offset in the chunk.
 * A slight performance penalty at lookup time, but since we carry around
 * pointers this penalty is only rarely incurred.
 *						RFHH */

#define LOGOTAB_CHUNK	10
#define OTAB_CHUNK	(1 << LOGOTAB_CHUNK)
#define OTABMOD(i)	(i & (OTAB_CHUNK - 1))	/* '&' is fast for '%' */
#define OTABDIV(i)	(i >> LOGOTAB_CHUNK)	/* '>>' is fast for '/' */

static fragment_p *otab_dir;    /* lookup table for local object table chunks */
static unsigned max_chunks;	/* length of the object indirection table */
static unsigned max_entries;	/* length of the object table */
static unsigned g_next;		/* next (global) location in object table */

void
otab_start(void)
{
    max_entries = OTAB_CHUNK;
    max_chunks = 1;
    otab_dir = m_malloc(max_chunks * sizeof(fragment_p));
    otab_dir[0] = m_malloc(OTAB_CHUNK * sizeof(fragment_t));
    memset(otab_dir[0], 0, OTAB_CHUNK * sizeof(fragment_t));
}

void
otab_end(void)
{
    int j;
    int i, not_empty = 0;
    int n;
    
    /* Check if all entries are empty
     */
    for (j = 0; j < max_chunks; j++) {
	if (j == max_chunks) {
	    n = OTABMOD(g_next);
	} else {
	    n = OTAB_CHUNK;
	}
	for(i = 0; i < n; i++) {
	    if (otab_dir[j][i].fr_rts) {   
		not_empty++;
	    }
	}
    }

    if (not_empty > 0) {
	fprintf(stderr, "%d) warning: object table has %d nonempty entries\n",
		rts_my_pid, not_empty);
    }
    
    for (j = 0; j < max_chunks; j++) {
	m_free(otab_dir[j]);
    }
    m_free(otab_dir);
    otab_dir = 0;
}


static fragment_p
otab_locate(int x)
{
    if (x < OTAB_CHUNK) {		/* avoid '/' and '%' in common case */
	return &otab_dir[0][x];
    }

    assert(OTABDIV(x) < max_chunks);
    return &otab_dir[OTABDIV(x)][OTABMOD(x)];
}

int
otab_enter(fragment_p obj, int home)
{
    int old_chunks;
    int j;
    fragment_p otab_ptr;

    /* Is there room for another entry in the object table? */
    if (g_next == max_entries) {
	old_chunks = max_chunks;
	max_chunks *= 2;
	max_entries *= 2;
	otab_dir = m_realloc(otab_dir, max_chunks * sizeof(fragment_p));
	for (j = old_chunks; j < max_chunks; j++) {
	    otab_dir[j] = m_malloc(OTAB_CHUNK * sizeof(fragment_t));
	    memset(otab_dir[j], 0, OTAB_CHUNK * sizeof(fragment_t));
	}
    }

    otab_ptr = otab_locate(g_next);

    if (home == rts_my_pid) {
	*otab_ptr = *obj;        /* copy local object pointers into table */
	obj->fr_oid  = g_next;    /* assign unique id to object */

#ifdef RTS_VERBOSE
	printf("%d) entered object %s: offset = %d, obj->fields = %p\n",
	       rts_my_pid, obj->fr_name, g_next, obj->fr_fields);
#endif
    } else {
	otab_ptr->fr_rts = 0;        /* no local copy available */
	otab_ptr->fr_fields = 0;

#ifdef RTS_VERBOSE
	printf("%d) new globid at offset %d\n", rts_my_pid, g_next);
#endif
    }

    return g_next++; 
}

void
otab_install(fragment_p obj)
{
    fragment_p p;

    assert(obj->fr_oid != OBJ_UNKNOWN);
    p = otab_locate(obj->fr_oid);
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

    obj = otab_locate(oid);
    return (obj->fr_rts ? obj : 0);
}

void
otab_remove(int oid)
{
    fragment_p obj;

    if (oid == OBJ_UNKNOWN) {
	return;
    }

    obj = otab_locate(oid);
    if (obj->fr_rts) {
	obj->fr_rts = 0;
    }
}

