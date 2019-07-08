/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

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

#define OBJ_UNKNOWN (-1)

extern void otab_start(void);

extern void otab_end(void);

extern int otab_enter(fragment_p obj, int home);

extern void otab_install(fragment_p obj);

extern fragment_p otab_lookup(int oid);

extern void otab_remove(int oid);

#endif
