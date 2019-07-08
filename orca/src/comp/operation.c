/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: operation.c,v 1.32 1997/05/15 12:02:36 ceriel Exp $ */

/* Routines dealing with operations. */

#include <stdio.h>

#include "debug.h"
#include "ansi.h"

#include <assert.h>
#include <alloc.h>

#include "operation.h"
#include "scope.h"
#include "node.h"
#include "type.h"
#include "visit.h"
#include "error.h"
#include "oc_stds.h"
#include "gen_code.h"

static t_def	*bdf;

static int blocks;
_PROTOTYPE(static int chk_blockcall, (p_node));

void
chk_blocking(df)
	t_def	*df;
{
	/* Blocking analysis. For every operation, check whether they
	   themselves may block or call a function/operation that does.
	*/
	t_def	*save_bdf = bdf;

	if (df->df_flags & D_BLOCKINGDONE) return;
	if (! (df->df_flags & D_DEFINED)) {
#ifdef DEBUG
		t_def *mdf = df->df_scope->sc_definedby;
		assert(bdf != 0);
		debug("blocking_info of %s depends on blocking-info of %s(%s%s%s)",
			bdf->df_idf->id_text,
			df->df_idf->id_text,
			mdf->mod_dir,
			*(mdf->mod_dir) ? "/" : "",
			mdf->df_idf->id_text);
#endif
		if (! (df->df_flags & (D_NONBLOCKING|D_BLOCKING))) {
#ifdef DEBUG
			debug("assuming %s does not block",
				df->df_idf->id_text);
#endif
			df->df_flags |= D_NONBLOCKING;
		}
		if (df->df_kind == D_OPERATION) {
			if (!(bdf->df_flags & D_CALLS_OP)) {
				bdf->df_flags |= D_CALLS_OP;
				if (bdf->df_kind == D_FUNCTION &&
				    could_be_called_from_operation(bdf)) {
					bdf->df_flags |= D_EXTRAPARAM;
				}
			}
		}
		else if (df->df_kind == D_FUNCTION) {
			if (df->df_flags & D_EXTRAPARAM) {
				bdf->df_flags |= (df->df_flags & D_CALLS_OP);
			}
		}
		def_enlist(&(bdf->prc_blockdep), df);
		df->df_flags |= D_BLOCKINGDONE;
		return;
	}

	df->df_flags |= D_BLOCKINGDONE;
	df->df_flags &= ~ (D_BLOCKING|D_NONBLOCKING);
	bdf = df;
	visit_ndlist(df->bod_statlist1, Call|Stat, chk_blockcall, 0);
#ifdef DEBUG
	debug("%s is %sblocking", df->df_idf->id_text,
		(df->df_flags & D_BLOCKING) ? "" : "non-");
#endif
	if (! (df->df_flags & D_BLOCKING)) df->df_flags |= D_NONBLOCKING;
	bdf = save_bdf;
	def_endlist(&(df->prc_blockdep));
	if (df->df_kind == D_FUNCTION && (df->df_flags & D_CALLS_OP)) {
		if (could_be_called_from_operation(df)) {
			df->df_flags |= D_EXTRAPARAM;
		}
	}
}

static int
chk_blockcall(nd)
	p_node	nd;
{
	assert(nd->nd_class & (Call|Stat));

	if (nd->nd_class == Stat) {
		if (nd->nd_symb == GUARD) {
			int sv_blocks = blocks;
			blocks = 0;
			visit_node(nd->nd_expr, Call, chk_blockcall, 0);
			if (blocks) {
				nd->nd_expr->nd_flags |= ND_BLOCKS;
				blocks = 0;
			}
			visit_ndlist(nd->nd_list1, Call, chk_blockcall, 0);
			if (blocks) {
				nd->nd_flags |= ND_BLOCKS;
			}
			blocks = sv_blocks;
		}
		return 0;
	}
	if (nd->nd_symb == '(') {
		/* Function call. */
		if (nd->nd_callee->nd_class == Def) {
			t_def	*df = nd->nd_callee->nd_def;

			if (df->df_type == std_type) return 0;
			if (df->df_kind == D_FUNCTION) {
				chk_blocking(df);
				if (df->df_flags & D_EXTRAPARAM) {
					bdf->df_flags |= D_CALLS_OP;
				}
				if (! (df->df_flags & D_BLOCKING)) {
					return 0;
				}
				else blocks = 1;
			}
		}
		/* Here, the call is through a function variable,
		   in which case we have no idea, so we assume worst case:
		   blocking, or the function is blocking.
		   We continue to find all dependencies.
		*/
		bdf->df_flags |= D_BLOCKING|D_CALLS_OP;
		blocks = 1;
		return 0;
	}

	/* Operation call. */
	if (nd->nd_symb == '$') {
		bdf->df_flags |= D_CALLS_OP;
		chk_blocking(nd->nd_callee->nd_def);
		if (nd->nd_callee->nd_def->df_flags & D_BLOCKING) {
			blocks = 1;
			bdf->df_flags |= D_BLOCKING;
		}
	}
	return 0;
}

static int	guard_writes;
_PROTOTYPE(static int chk_ofieldwrt, (p_node));

void
chk_writes(df)
	t_def	*df;
{
	/* Check an operation for its effect on the object, and set
	   flags according to the result.
	*/
	p_node	nd;
	t_def	*save_bdf = bdf;

	assert(df->df_kind == D_OPERATION);

	if (df->df_flags & D_RWDONE) return;

	if (! (df->df_flags & D_DEFINED)) {
#ifdef DEBUG
		t_def *mdf = df->df_scope->sc_definedby;
		assert(bdf != 0);
		debug("read/write behavior of %s depends on that of %s(%s%s%s)",
		      bdf->df_idf->id_text,
		      df->df_idf->id_text,
		      mdf->mod_dir,
		      *(mdf->mod_dir) ? "/" : "",
		      mdf->df_idf->id_text);
#endif
		if (df->df_flags & D_HASWRITES) {
			guard_writes = 1;
		}
		if (! (df->df_flags & (D_HASREADS|D_HASWRITES))) {
#ifdef DEBUG
			debug("assuming %s only reads", df->df_idf->id_text);
#endif
			df->df_flags |= D_HASREADS;
		}
		df->df_flags |= D_RWDONE;
		def_enlist(&(bdf->bod_transdep), df);
		return;
	}
	df->df_flags |= D_RWDONE;
	df->df_flags &= ~(D_HASWRITES|D_HASREADS);
	nd = node_getlistel(df->bod_statlist1);
	bdf = df;
	if (nd && nd->nd_symb == GUARD) {
		p_node	l;
		p_node	ll = df->bod_statlist1;

		node_initlist(&df->bod_statlist1);
		node_initlist(&df->bod_statlist2);
		node_walklist(ll, l, nd) {
			int save_writes = 0;
			assert(nd->nd_symb == GUARD);

			guard_writes = 0;
			visit_node(nd->nd_expr, Call, chk_ofieldwrt, 0);
			if (guard_writes) {
				nd->nd_expr->nd_flags |= ND_WRITES;
				df->df_flags |= D_OBJ_COPY;
				save_writes = 1;
				guard_writes = 0;
			}
			visit_ndlist(nd->nd_list1, Stat|Call, chk_ofieldwrt, 0);
			node_fromlist(&ll, nd);
			if (guard_writes || save_writes) {
				nd->nd_flags |= ND_WRITES;
				df->df_flags |= D_HASWRITES;
				if ((nd->nd_flags & ND_BLOCKS)
				    /* || ! node_emptylist(df->bod_statlist2) */) {
					df->df_flags |= D_OBJ_COPY;
				}
				node_enlist(&df->bod_statlist2, nd);
			}
			else {
				df->df_flags |= D_HASREADS;
				node_enlist(&df->bod_statlist1, nd);
			}
		}
		node_killlist(&ll);
		node_walklist(df->bod_statlist1, ll, nd) {
			if (node_emptylist(ll)) {
				nd->nd_flags |= ND_LASTGUARD;
			}
		}
		node_walklist(df->bod_statlist2, ll, nd) {
			if (node_emptylist(ll)) {
				nd->nd_flags |= ND_LASTGUARD;
			}
		}
	}
	else {
		guard_writes = 0;
		visit_ndlist(df->bod_statlist1, Stat|Call, chk_ofieldwrt, 0);
		if (guard_writes) {
			df->bod_statlist2 = df->bod_statlist1;
			df->bod_statlist1 = 0;
			df->df_flags |= D_HASWRITES;
		}
		else {
			df->df_flags |= D_HASREADS;
		}
	}
#ifdef DEBUG
	debug("%s%s%s", df->df_idf->id_text,
		(df->df_flags & D_HASREADS) ? " reads" : "",
		(df->df_flags & D_HASWRITES) ? " writes" : "");
#endif
	bdf = save_bdf;
	guard_writes = 0;
}

static int
chk_ofieldwrt(nd)
	p_node	nd;
{
	/* Check the tree indicated by 'nd' for writes to an object
	   field, and set 'guard_writes' to 1 if it does.
	*/
	t_def	*df;

	switch(nd->nd_class) {
	case Call: {
		p_node	actuals;
		t_dflst	formals;

		actuals = nd->nd_parlist;
		if (nd->nd_symb == DOLDOL) {
			break;
		}
		formals = nd->nd_callee->nd_type->prc_params;
		if (nd->nd_symb == '(') {
			/* Function call. First check the function designator.
			   It might be an expression with side effects.
			*/
			visit_node(nd->nd_callee, Call, chk_ofieldwrt, 0);
			if (nd->nd_callee->nd_type == std_type) {
				/* Some standard functions change an
				   argument. Other arguments could have
				   side effects.
				*/
				df = nd->nd_callee->nd_def;
				switch(df->df_stdname) {
				case S_DELETENODE:
					visit_node(node_getlistel(node_nextlistel(actuals)), Call, chk_ofieldwrt, 0);
					/* fall through */
				case S_ADDNODE:
					nd = node_getlistel(actuals);
					break;
				case S_INSERT:
				case S_DELETE:
					visit_node(node_getlistel(actuals), Call, chk_ofieldwrt, 0);
					nd = node_getlistel(node_nextlistel(actuals));
					break;
				case S_READ:
					visit_ndlist(actuals,
					   Call|Arrsel|Oper|Uoper|Def|Select,
					   chk_ofieldwrt, 0);
					return 1;
				default:
					return 0;
					/* So that visit_node will continue
					   to visit the arguments.
					*/
				}
				visit_node(nd,
					   Call|Arrsel|Oper|Uoper|Def|Select,
					   chk_ofieldwrt, 0);
				return 1;
			}
		}
		else {
			/* Check operation: does it write, and if so,
			   is the object a nested one or SELF?
			*/
			assert(nd->nd_symb == '$');
			visit_node(nd->nd_obj, Call, chk_ofieldwrt, 0);
			if (! guard_writes) {
				visit_node(nd->nd_obj,
					   Call|Arrsel|Oper|Uoper|Def|Select,
					   chk_ofieldwrt, 0);
				if (guard_writes) {
					chk_writes(nd->nd_callee->nd_def);
					if (nd->nd_callee->nd_def->df_flags & D_HASWRITES) {
						guard_writes = 1;
					}
					else {
						guard_writes = 0;
					}
				}
			}
		}

		/* Check parameters. */

		while (! def_emptylist(formals)) {
			t_def *f = def_getlistel(formals);

			visit_node(node_getlistel(actuals),
				   is_in_param(f) ?
					Call :
					(Call|Arrsel|Oper|Uoper|Def|Select),
				   chk_ofieldwrt, 0);
			formals = def_nextlistel(formals);
			actuals = node_nextlistel(actuals);
		}
		return 1;
	}
	case Def:
		if (nd->nd_def->df_kind == D_OFIELD) {
			guard_writes = 1;
		}
		if (nd->nd_def->df_kind == D_VARIABLE
		    && (nd->nd_def->df_flags & D_SELF)) {
			guard_writes = 1;
		}
		break;
	case Stat:
		switch(nd->nd_symb) {
		case BECOMES:
		case PLUSBECOMES:
		case MINBECOMES:
		case TIMESBECOMES:
		case DIVBECOMES:
		case MODBECOMES:
		case ORBECOMES:
		case ANDBECOMES:
		case B_ORBECOMES:
		case B_ANDBECOMES:
		case B_XORBECOMES:
		case LSHBECOMES:
		case RSHBECOMES:
			visit_node(nd->nd_desig,
				   Call|Arrsel|Oper|Uoper|Def|Select,
				   chk_ofieldwrt, 0);
		}
		break;
	case Oper:
	case Uoper:
		visit_node(nd->nd_left, Call, chk_ofieldwrt, 0);
		visit_node(nd->nd_right, Call, chk_ofieldwrt, 0);
		return 1;
	case Arrsel:
		visit_node(nd->nd_right, Call, chk_ofieldwrt, 0);
		/* Fall through */
	case Select:
		visit_node(nd->nd_left,
			   Arrsel|Call|Select|Def,
			   chk_ofieldwrt, 0);
		return 1;
	}
	return 0;
}
