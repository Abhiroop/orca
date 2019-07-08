#ifndef __OBJ_TAB_H__
#define __OBJ_TAB_H__

/* Hash table that contains fragment descriptors. Each fragment descriptor
   is identified by an object id. The hash table cannot contain duplicate
   entries (i.e., entries with the same object id).

   A fragment descriptor is a shallow copy of an object fragment,
   containing pointers to the fragment's fields, type, and RTS part.

   This module also generates unique object identifiers. Ids generated
   are guaranteed to be unique over all platforms.
*/


#include <string.h>
#include "orca_types.h"


#define oid_clear(oid)

#define oid_copy(src, dest)          *(dest) = *(src)

#define oid_equal(oid1, oid2)        ( (oid1) == (oid2) )

#define oid_marshall(oid, buf)       (void)memcpy((buf), (oid), sizeof(oid_t))

#define oid_unmarshall(oid, buf)     (void)memcpy((oid), (buf), sizeof(oid_t))


extern void otab_start(void);

extern void otab_end(void);

extern otab_entry_p otab_enter(fragment_p f);

extern void otab_remove(otab_entry_p entry);

extern otab_entry_p otab_find_enter(oid_p oid);

extern otab_entry_p otab_lookup(oid_p oid);

extern void oid_init(rts_object_p rts);

extern char *oid_ascii(oid_p oid);

#endif
