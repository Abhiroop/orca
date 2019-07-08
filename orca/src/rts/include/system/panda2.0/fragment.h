/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __FRAGMENT_H__
#define __FRAGMENT_H__

#include "orca_types.h"
#include "rts_types.h"

/* Defines the basic operations on an object fragment: locking,
 * reading, and writing. Locking is not always necessary and is
 * left to the user.
 */

/* A fragment is the local representation of an object. Fragments
 * can be part of a replicated object. Fragments need not contain
 * object data; they might contain a reference to a remote fragment
 * that holds the data. The actual status is determined by the fragment's
 * flags field, as defined in rts_object.h.
 */


/* Abbreviations; used internally only */
#define fr_fields            o_fields
#define fr_rts               o_rtsdep

#define fr_type              o_rtsdep->type
#define fr_rtsobj            o_rtsdep->obj

#define fr_oid               o_rtsdep->obj.oid
#ifdef TRACING
#define fr_home              o_rtsdep->obj.home
#define fr_home_obj          o_rtsdep->obj.home_obj
#endif
#define fr_frag              o_rtsdep->obj.frag
#define fr_flags             o_rtsdep->obj.flags
#define fr_lock              o_rtsdep->obj.lock
#define fr_wlist             o_rtsdep->obj.wlist
#define fr_info              o_rtsdep->obj.info
#define fr_account           o_rtsdep->obj.account
#define fr_owner             o_rtsdep->obj.owner
#define fr_total_refs        o_rtsdep->obj.total_refs
#define fr_manager           o_rtsdep->obj.manager
#define fr_name              o_rtsdep->obj.name


/* Extracting fields.
 */
#define f_get_status(f)              ( (f_status_t)((f)->fr_flags & RO_MASK) )

#define f_get_fields(f)  ( (f)->fr_fields   )        /* object data */
#define f_get_type(f)    ( (f)->fr_type     )        /* object type */
#define f_get_queue(f)   ( &((f)->fr_wlist) )        /* blocked invocations */
#define f_get_owner(f)   ( (f)->fr_owner    )        /* owner platform id */
#define f_get_lock(f)    ( (f)->fr_lock     )        /* object lock */


/* Lock operations.
 */
#define f_lock(f)        pan_mutex_lock(f_get_lock(f))
#define f_try_lock(f)    pan_mutex_trylock(f_get_lock(f))
#define f_unlock(f)      pan_mutex_unlock(f_get_lock(f))

#define f_shallow_copy(src, dest)    *(dest) = *(src)

#define f_end_read(f)      f_unlock(f)



extern void f_init(fragment_p f, struct type_descr *obj_type, char *name);

extern void f_trc_create(fragment_p f, char *trc_name);

extern void f_clear(fragment_p f);

extern int f_read(fragment_p f, int *op_flags,
		  struct type_descr *optype, int opindex,
		  void **args, source_t src);

extern int f_read_write(fragment_p f, int *op_flags,
			struct type_descr *optype,
			int opindex, void **args, int *modified, source_t src);

extern int f_write_read(fragment_p f, int *op_flags,
			struct type_descr *optype, int opindex,
			void **args, int *modified, source_t src);

extern int f_start_read(fragment_p f);

extern int f_start_write(fragment_p f);

extern void f_end_write(fragment_p f);

#endif


