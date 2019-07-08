/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __INVOCATION_H__
#define __INVOCATION_H__

#include "orca_types.h"
#include "rts_types.h"


/* This module describes the interface to the invocation data type.
   Once started, an invocation may succeed, block, or fail. Blocked
   invocations can be resumed.
*/

#undef DECL
#if defined(INVOCATION_SRC) || !defined(INLINE_FUNCTIONS)
#define DECL    extern
#define i_get_info_macro(i, op, frag, argv, status, upcall, op_flags) \
	       i_get_info(i, &op, &frag, &argv, &status, &upcall, &op_flags)

#else
#define DECL    static

/* gcc does not optimize the following code (which results from inline
 * expansion of i_get_info):
 *	*&opindex = inv->opindexl
 * gcc does not put the local variable 'opindex' into a register, but places it
 * on the stack and, thus, generates superfluous loads and stores :-(
 * Hack: avoid & operator by using a macro.
 */
#define i_get_info_macro(i, _op, _frag, _argv, status, _upcall, _op_flags) \
		do { \
		    _op     = i->op; \
		    _frag   = i->frag; \
		    _argv   = i->argv; \
		    status  = i->f_status; \
		    _upcall = i->upcall; \
		    _op_flags = i->op_flags; \
		} while (0)

#endif

DECL void i_clear(invocation_p inv);

DECL void i_init(invocation_p inv, fragment_p frag, f_status_t status,
		 op_dscr *op, int upcall, void **argv, int *op_flags);

DECL void i_get_info(invocation_p inv, op_dscr **op, fragment_p *frag,
		     void ***argv, f_status_p status, int *upcall,
		     int **op_flags);

DECL void i_wakeup(invocation_p inv, int res);

DECL i_status_t i_block(invocation_p inv);

void i_start(void);

void i_end(void);

#if !defined(INVOCATION_SRC) && defined(INLINE_FUNCTIONS)
#define INVOCATION_INLINE
#include "src/invocation.c"
#endif

#endif
