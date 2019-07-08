/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S C O P E   M E C H A N I S M */

/* $Id: scope.c,v 1.14 1997/05/15 12:02:55 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"
#include	<stdio.h>

#include	<assert.h>
#include	<alloc.h>

#include	"scope.h"
#include	"idf.h"
#include	"def.h"
#include	"type.h"
#include	"error.h"
#include	"options.h"
#include	"specfile.h"

t_scope	*PervasiveScope,
	*CurrentScope,
	*GlobalScope,
	*ProcScope;

void
open_scope(scopetype)
	int	scopetype;
{
	/*	Open a scope that is either open (automatic imports) or closed.
	*/
	t_scope	*sc = new_scope();

	assert(scopetype == OPENSCOPE || scopetype == CLOSEDSCOPE);

	sc->sc_scopeclosed = scopetype == CLOSEDSCOPE;
	sc->sc_encl = CurrentScope;
	CurrentScope = sc;
}

t_scope *
open_and_close_scope(scopetype)
	int	scopetype;
{
	t_scope	*sc;

	open_scope(scopetype);
	sc = CurrentScope;
	close_scope();
	return sc;
}

void
init_scope()
{
	PervasiveScope = new_scope();
	CurrentScope = PervasiveScope;
}

void
chk_procs()
{
	/*	Called at scope closing. Check all definitions, and if one
		is either D_FUNCTION, D_PROCESS, or D_OPERATION,
		check that it is defined.
	*/
	t_def	*df = CurrentScope->sc_def;

	while (df) {
		if (! (df->df_flags & D_DEFINED)) switch (df->df_kind) {
		case D_FUNCTION:
			/* A not defined procedure
			*/
			if (! options['u']) {
				error("function \"%s\" not defined",
					df->df_idf->id_text);
			}
			break;
		case D_PROCESS:
			/* A not defined process
			*/
			error("process \"%s\" not defined",
				df->df_idf->id_text);
			break;
		case D_OPERATION:
			/* A not defined operation
			*/
			error("operation \"%s\" not defined",
				df->df_idf->id_text);
			break;
		}
		df = df->df_nextinscope;
	}
}

void
chk_forwards()
{
	/*	Look for all forward definitions (resulting from NODENAME
		type declarations).  If it still is a D_TYPE with df_type 0
		it is actually either imported from the enclosing scope(s)
		or not declared.
	*/
	t_def	*df,
		*df2 = 0,
		**pdf = &(CurrentScope->sc_def);

	while ((df = *pdf)) {
		if (! Specification && df->df_kind == D_OPAQUE) {
			error("opaque type \"%s\" not declared",
				df->df_idf->id_text);
		}
		else if (df->df_kind == D_TYPE) {
			struct forw *nd = df->tdf_forw_list;
			int must_continue = 0;
			t_idf *id = df->df_idf;

			if (! df->df_type) {
				*pdf = df->df_nextinscope;
				remove_from_id_list(df);
				free_def(df);
				df = lookfor(id, CurrentScope, 1);
				must_continue = 1;
			}
			if (nd && df->df_type != error_type) {
				if (! df->df_kind & (D_ERROR|D_TYPE)) {
pos_error(&(nd->f_pos), "\"%s\" is not a type", id->id_text);
				}
				else if (df->df_type->tp_fund != T_GRAPH) {
pos_error(&(nd->f_pos), "\"%s\" is not a graph type", id->id_text);
				}
				while (nd) {
					struct forw *next = nd->f_next;
					nd->f_nodename->tp_next = df->df_type;
					free_forw(nd);
					nd = next;
				}
				df->tdf_forw_list = 0;
			}
			if (must_continue) continue;
		}
		df2 = df;
		pdf = &(df->df_nextinscope);
	}
	if (df2) df2->df_scope->sc_end = df2;
}

void
close_scope()
{
	/*	Close a scope.
	*/

	DO_DEBUG(options['S'], dump_scope(CurrentScope));

	if (ProcScope == CurrentScope) {
		ProcScope = 0;
	}
	CurrentScope = enclosing(CurrentScope);
}

#ifdef DEBUG
int
dump_scope(sc)
	t_scope	*sc;
{
	t_def	*df = sc->sc_def;

	printf("List of definitions in scope 0x%lx:\n", (long) sc);
	while (df) {
		(void) dump_def(df);
		df = df->df_nextinscope;
	}
	return 0;
}
#endif
