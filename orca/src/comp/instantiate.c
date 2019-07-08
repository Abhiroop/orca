/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* I N S T A N T I A T I O N   O F   G E N E R I C S */

/* $Id: instantiate.c,v 1.21 1997/05/15 12:02:21 ceriel Exp $ */

#include <stdio.h>
#include <alloc.h>

#include "ansi.h"
#include "debug.h"

#include "instantiate.h"
#include "scope.h"
#include "type.h"
#include "specfile.h"
#include "error.h"
#include "main.h"
#include "chk.h"
#include "idf.h"

_PROTOTYPE(static void chk_inst_param, (p_node, t_def *));

static p_node
	generic_actuals, p_actual;
static int
	instantiation_kind,
	instantiating;
static struct idf
	*instantiation_id,
	*generic_id;

void
start_instantiate(kind, inst_id, gen_id)
	int	kind;
	t_idf	*inst_id;
	t_idf	*gen_id;
{
	instantiation_kind = kind;
	instantiation_id = inst_id;
	generic_id = gen_id;
	node_initlist(&generic_actuals);
}

void
add_generic_actual(nd)
	t_node	*nd;
{
	if (nd->nd_class == Name) {
		t_def	*df = lookfor(nd->nd_idf, CurrentScope, 1);

		nd->nd_class = Def;

		df->df_flags |= D_USED;
		nd->nd_type = df->df_type;
		nd->nd_def = df;
		if (df->df_kind & (D_CONST|D_ENUM)) {
			nd = chk_designator(nd);
		}
		else if (!(df->df_kind & (D_ISTYPE|D_ERROR|D_FUNCTION))) {
			error("parameter of instantiation must be a type, function, or constant");
		}
	}
	node_enlist(&generic_actuals, nd);
}

void
start_generic_formals()
{
}

void
add_generic_formal(df, tp)
	t_def	*df;
	t_type	*tp;
{
	df->df_flags |= D_GENERICPAR|D_DEFINED;
	if (tp) df->df_type = tp;
	if (instantiating) {
		if (node_emptylist(p_actual)) {
			error("too few actual parameters in generic instantiation");
			instantiating = 0;
		}
		else	chk_inst_param(p_actual, df);
		p_actual = node_nextlistel(p_actual);
	}
}

void
end_generic_formals()
{
	if (instantiating && ! node_emptylist(p_actual)) {
		error("too many actual parameters in generic instantiation");
	}
	instantiating = 0;
}

void
finish_instantiate()
{
	t_def	*dfgen,
		*df;
	t_scope	*sc = open_and_close_scope(CLOSEDSCOPE);
	p_node	nd;
	p_node	actuals;
	t_idf	*inst_id = instantiation_id;

	actuals = generic_actuals;

	p_actual = generic_actuals;
	instantiating = 1;

	/* The instantiation itself is mostly handled in program.g,
	   which calls chk_inst_param() for each generic parameter.
	*/
	dfgen = get_specification(generic_id, 1, instantiation_kind,
				  D_GENERIC, inst_id);

	generic_actuals = actuals;
	/* because get_specification may cause another instantiation.  */

	dfgen->df_scope = sc;	/* just an anonymous scope, so that it is
				   read again when required */

	df = define(inst_id, CurrentScope, dfgen->df_kind);
	df->df_flags |= D_INSTANTIATION;
	df->mod_gendf = dfgen;

	if (dfgen->df_kind == D_OBJECT) {
		/* We need a new type struct, to get the names right.
		   Every instantiation must result in different names.
		*/
		df->df_type = new_type();
		*(df->df_type) = *(dfgen->df_type);
		df->df_type->tp_tpdef = 0;
		df->df_type->tp_descr = 0;
		df->df_type->tp_def = df;
		dfgen->df_type->tp_equal = df->df_type;
	}
	else df->df_type = dfgen->df_type;

	df->bod_scope = dfgen->bod_scope;
	node_walklist(generic_actuals, actuals, nd) {
		free_node(nd);
	}
	node_killlist(&generic_actuals);
}

static void
chk_inst_param(nd, df)
	p_node	nd;
	t_def	*df;
{
	/*	Check the actual generic instantiation parameter in "nd"
		against the formal parameter in "df".
	*/
	t_type	*tp;

	switch(df->df_kind) {
	case D_TYPE:
		/* initialize so that it is, when an error occurs. */
		generic_actual_of(df->df_type) = error_type;

		if (nd->nd_class != Def
		    || ! (nd->nd_def->df_kind & (D_ERROR|D_ISTYPE))) {
			pos_error(&nd->nd_pos,
				"actual parameter for \"%s\" is not a type",
				df->df_idf->id_text);
			break;
		}

		tp = nd->nd_def->df_type;

		if (tp == error_type || df->df_type == error_type) {
			break;
		}
		switch(df->df_type->tp_fund) {
		case T_GENERIC:
			break;
		case T_NUMERIC:
			if (!(tp->tp_fund & T_NUMERICARG)) {
				pos_error(&nd->nd_pos,
				  "actual parameter for \"%s\" is not numeric",
				  df->df_idf->id_text);
			}
			break;
		case T_SCALAR:
			if (!(tp->tp_fund & T_SCALARARG)) {
				pos_error(&nd->nd_pos,
				  "actual parameter for \"%s\" is not scalar",
				  df->df_idf->id_text);
			}
			break;
		default:
			crash("chk_inst_param.1");
		}

		/* If the instantiation parameter itself is a generic
		   parameter (so we have a nested instantiation),
		   use its actual, if there is one.
		*/
		if ((tp->tp_fund & T_GENPAR) && generic_actual_of(tp)) {
			tp = generic_actual_of(tp);
		}
		generic_actual_of(df->df_type) = tp;
		break;
	case D_CONST:
		if (! (nd->nd_flags & ND_CONST)) {
			pos_error(&nd->nd_pos,
				"constant expression for \"%s\" expected",
				df->df_idf->id_text);
			break;
		}
		if (! tst_type_equiv(nd->nd_type, df->df_type)) {
			pos_error(&nd->nd_pos,
			  "type incompatibility in actual parameter for \"%s\"",
			  df->df_idf->id_text);
		}
		df->con_const = new_node();
		*(df->con_const) = *nd;
		break;
	case D_FUNCTION:
		if (nd->nd_class != Def
		    || ! (nd->nd_def->df_kind & (D_FUNCTION|D_ERROR))) {
			pos_error(&nd->nd_pos, "procedure for \"%s\" expected",
				df->df_idf->id_text);
			break;
		}
		if (!(nd->nd_def->df_flags & D_DEFINED)) {
		    def_enlist(&CurrDef->bod_transdep, nd->nd_def);
		}
		if (! tst_proc_equiv(df->df_type, nd->nd_def->df_type)) {
			pos_error(&nd->nd_pos,
			  "type incompatibility in actual parameter for \"%s\"",
			  df->df_idf->id_text);
			break;
		}
		df->con_const = new_node();
		*(df->con_const) = *nd;
		/* ??? what else ??? */
		break;
	case D_ERROR:
		break;
	default:
		crash("chk_inst_param");
	}
}
