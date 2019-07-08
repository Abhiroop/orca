/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: gen_expr.c,v 1.58 1998/05/13 11:48:35 ceriel Exp $ */

/* Code generation for expressions. */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <assert.h>
#include <alloc.h>
#include <flt_arith.h>

#include "gen_expr.h"
#include "scope.h"
#include "def.h"
#include "type.h"
#include "generate.h"
#include "gen_code.h"
#include "simplify.h"
#include "error.h"
#include "main.h"
#include "oc_stds.h"
#include "options.h"

_PROTOTYPE(static void gen_params, (FILE *, p_node, t_type *));
_PROTOTYPE(static void gen_target_assign, (FILE *, p_node));
_PROTOTYPE(static void gen_Oper, (FILE *, p_node));
_PROTOTYPE(static void gen_Call, (FILE *, p_node));
_PROTOTYPE(static void gen_Select, (FILE *, p_node));
_PROTOTYPE(static void gen_Ofldselect, (FILE *, p_node));
_PROTOTYPE(static void gen_AorG_Sel, (FILE *, p_node));
_PROTOTYPE(static void gen_defnam, (FILE *, t_def *));
_PROTOTYPE(static void gen_stds, (FILE *, p_node));
_PROTOTYPE(static void gen_par_result, (FILE *, p_node, t_type *, int));
_PROTOTYPE(static int test_nil, (p_node));
_PROTOTYPE(static void gen_IN_parameter, (FILE *, p_node));

int	possibly_blocking;
int	no_temporaries = 0;

static void
gen_IN_parameter(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	For IN parameter passing, constructed types are passed as
		an address and simple types are passed as a value.
		The problem lies in generic type parameters: we use the
		preprocessor to decide in this case.
		There is a macro ch<tpdef> with two parameters, which selects
		the first parameter if the type is simple, the second if it
		is not.
	*/
	t_type	*ndtp = nd->nd_type;

	if (ndtp->tp_fund != T_GENERIC) {
		if (is_constructed_type(ndtp)) {
			gen_addr_ld(f, nd);
			return;
		}
		gen_expr_ld(f, nd);
		return;
	}
	switch(nd->nd_class) {
	case Tmp:
		assert(!(nd->nd_flags & ND_RETVAR));
		if (! nd->nd_tmpvar->tmp_ispointer) {
			fprintf(f, "ch%s(NOTHING,&)", ndtp->tp_tpdef);
		}
		else	fprintf(f, "ch%s(*,NOTHING)", ndtp->tp_tpdef);
		fprintf(f, TMP_FMT, nd->nd_tmpvar->tmp_id);
		putc(')', f);
		break;
	case Def:
		if (! (nd->nd_def->df_flags & D_COPY)) {
			if (is_shared_param(nd->nd_def)) {
				fprintf(f, "(ch%s(*,NOTHING)", ndtp->tp_tpdef);
				gen_defnam(f, nd->nd_def);
				putc(')', f);
				break;
			}
			if (is_in_param(nd->nd_def)) {
				/* Think about it ... */
				gen_defnam(f, nd->nd_def);
				break;
			}
		}
		fprintf(f, "(ch%s(NOTHING,&)", ndtp->tp_tpdef);
		gen_defnam(f, nd->nd_def);
		putc(')', f);
		break;
	case Select:
	case Arrsel:
		fprintf(f, "(ch%s(NOTHING,&)", ndtp->tp_tpdef);
		gen_expr_ld(f, nd);
		putc(')', f);
		break;
	case Ofldsel:
		fprintf(f, "(ch%s(*,NOTHING)", ndtp->tp_tpdef);
		gen_addr(f, nd);
		putc(')', f);
		break;
	default:
		crash("gen_IN_parameter");
	}
}

void
gen_addr(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate the address of the designator indicated by 'nd'.
	*/
	switch(nd->nd_class) {
	case Tmp:
		if (nd->nd_flags & ND_RETVAR) {
			putc('(', f);
			if (ProcScope->sc_definedby->df_kind == D_OPERATION &&
			    (CurrDef->df_flags & D_PARTITIONED)) {
			}
			else if (! is_constructed_type(nd->nd_type)) {
				putc('&', f);
			}
			else if (nd->nd_type->tp_fund == T_GENERIC) {
				fprintf(f, "ch%s(&,NOTHING)", nd->nd_type->tp_tpdef);
			}
			fputs("v__result)", f);
			break;
		}
		if (! nd->nd_tmpvar->tmp_ispointer) fputs("(&", f);
		fprintf(f, TMP_FMT, nd->nd_tmpvar->tmp_id);
		if (! nd->nd_tmpvar->tmp_ispointer) putc(')', f);
		break;
	case Value:
		assert(nd->nd_type == string_type || nd->nd_type == nil_type);
		if (nd->nd_type == nil_type) {
			fputs("(&nil)", f);
			break;
		}
		if (nd->nd_type == string_type) {
			fprintf(f, "(&%s)", nd->nd_str->s_name);
			break;
		}
		break;
	case Def:
		/* Usually just take the address, but there are some
		   exceptions when the def is a parameter and no copy was
		   made.
		*/
		if (! (nd->nd_def->df_flags & D_COPY)) {
			if (is_shared_param(nd->nd_def)) {
				gen_defnam(f, nd->nd_def);
				break;
			}
			if (is_in_param(nd->nd_def)) {
				if (nd->nd_def->df_type->tp_fund == T_GENERIC) {
					fprintf(f, "(ch%s(&,NOTHING)",
						nd->nd_def->df_type->tp_tpdef);
					gen_defnam(f, nd->nd_def);
					putc(')', f);
					break;
				}
				if (is_constructed_type(nd->nd_def->df_type)) {
					gen_defnam(f, nd->nd_def);
					break;
				}
			}
		}
		fputs("(&", f);
		gen_defnam(f, nd->nd_def);
		putc(')', f);
		break;
	case Select:
		fputs("(&", f);
		gen_Select(f, nd);
		putc(')', f);
		break;
	case Ofldsel:
		gen_Ofldselect(f, nd);
		break;
	case Arrsel:
		gen_AorG_Sel(f, nd);
		break;
	default:
		crash("gen_addr");
	}
}

void
gen_expr(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for the expression in 'nd'.
	*/
	switch(nd->nd_class) {
	case Call:
		gen_Call(f, nd);
		break;
	case Oper:
		gen_Oper(f, nd);
		break;
	case Uoper:
		if (nd->nd_symb == ARR_SIZE) {
			if (! nd->nd_right) {
				if (nd->nd_dimno > 0) {
					fprintf(f, "len%d", nd->nd_dimno);
					break;
				}
				fprintf(f, "(val->a_dims[%d].a_nel)", nd->nd_dimno);
				break;
			}
			if (nd->nd_right->nd_type->tp_flags & T_CONSTBNDS) {
				t_node	*bnd = nd->nd_right->nd_type->arr_bounds(nd->nd_dimno);
				fprintf(f, "%ld",
				   bnd->nd_right->nd_int -
				      bnd->nd_left->nd_int + 1);
				break;
			}
			putc('(', f);
			gen_addr(f, nd->nd_right);
			fprintf(f, "->a_dims[%d].a_nel)", nd->nd_dimno);
			break;
		}
		fprintf(f, "(%s", c_sym(nd->nd_symb));
		gen_expr(f, nd->nd_right);
		putc(')', f);
		break;
	case Tmp:
		if (no_temporaries) {
			gen_expr(f, nd->nd_tmpvar->tmp_expr);
			break;
		}
		if (nd->nd_flags & ND_RETVAR) {
			if (ProcScope->sc_definedby->df_kind == D_OPERATION &&
			    (CurrDef->df_flags & D_PARTITIONED)) {
				fputs("(*", f);
			}
			else if (! is_constructed_type(nd->nd_type)) {
				putc('(', f);
			}
			else if (nd->nd_type->tp_fund == T_GENERIC) {
				fprintf(f, "(ch%s(NOTHING,*)", nd->nd_type->tp_tpdef);
			}
			else {
				fputs("(*", f);
			}
			fputs("v__result)", f);
			break;
		}
		if (nd->nd_tmpvar->tmp_ispointer) {
			fputs("(*", f);
		}
		fprintf(f, TMP_FMT, nd->nd_tmpvar->tmp_id);
		if (nd->nd_tmpvar->tmp_ispointer) {
			putc(')', f);
		}
		break;
	case Value:
		if (nd->nd_type == string_type) {
			fprintf(f, "%s", nd->nd_str->s_name);
			break;
		}
		if (nd->nd_type == nil_type) {
			if (options['a']) {
				fputs("0", f);
			}
			else	fputs("nil", f);
			break;
		}
		gen_numconst(f, nd);
		break;
	case Def:
		if (nd->nd_def->df_kind == D_CONST
		    && nd->nd_def->con_const) {
			gen_expr(f, nd->nd_def->con_const);
			break;
		}
		if (nd->nd_def->df_flags & D_COPY) {
			gen_defnam(f, nd->nd_def);
			break;
		}
		if (is_shared_param(nd->nd_def)) {
			fputs("(*", f);
			gen_defnam(f, nd->nd_def);
			putc(')', f);
			break;
		}
		if (is_in_param(nd->nd_def)) {
			if (nd->nd_def->df_type->tp_fund == T_GENERIC) {
				fprintf(f, "(ch%s(NOTHING,*)",
					nd->nd_def->df_type->tp_tpdef);
				gen_defnam(f, nd->nd_def);
				putc(')', f);
				break;
			}
			else if (is_constructed_type(nd->nd_def->df_type)) {
				fputs("(*", f);
				gen_defnam(f, nd->nd_def);
				putc(')', f);
				break;
			}
		}
		gen_defnam(f, nd->nd_def);
		break;
	case Select:
		gen_Select(f, nd);
		break;
	case Ofldsel:
		fputs("(*", f);
		gen_Ofldselect(f, nd);
		putc(')', f);
		break;
	case Arrsel:
		fputs("(*", f);
		gen_AorG_Sel(f, nd);
		putc(')', f);
		break;

	default:
		crash("gen_expr");
	}
}

void
gen_numconst(f, nd)
	FILE	*f;
	p_node	nd;
{
	p_type	tp = nd->nd_type;

	if (tp->tp_fund == T_REAL) {
		if (! nd->nd_real->r_real) {
			char buf[FLT_STRLEN];

			flt_flt2str(&(nd->nd_real->r_val),
				    buf,
				    FLT_STRLEN);
			fprintf(f, "((%s) %s)",
				tp->tp_tpdef,
				buf);
		}
		else	fprintf(f, "((%s) %s)",
				tp->tp_tpdef,
				nd->nd_real->r_real);
	}
	else {
		assert(tp->tp_fund & T_DISCRETE);
		if (nd->nd_int == min_int[sizeof(long)]) {
			fprintf(f, "((%s) (%ldL-1))",
				tp->tp_tpdef,
				min_int[sizeof(long)]+1);
		}
		else fprintf(f, "((%s) %ldL)",
				tp->tp_tpdef,
				nd->nd_int);
	}
}

static void
gen_stds(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for a call to a standard function.
		This is all pretty straight-forward.
	*/
	t_def	*df;
	p_node	args = nd->nd_parlist;
	p_node	arg;

	assert(nd->nd_callee->nd_class == Def);
	df = nd->nd_callee->nd_def;

	if (nd->nd_target) {
		switch(df->df_stdname) {
		case S_FROM:
			arg = node_getlistel(args);
			fprintf(f, "%c_from(", base_char_of(arg->nd_type));
			gen_addr(f, arg);
			fprintf(f, ", &%s, ", arg->nd_type->tp_descr);
			gen_addr_st(f, nd->nd_target);
			putc(')', f);
			break;

		case S_ADDNODE:
			arg = node_getlistel(args);
			if (options['a']) {
				gen_expr_st(f, nd->nd_target);
				fputs(" = ", f);
			}
			fputs("g_addnode(", f);
			if (! options['a']) {
				gen_addr_st(f, nd->nd_target);
				fputs(", ", f);
			}
			gen_addr(f, arg);
			fprintf(f, ", sizeof(%s))", arg->nd_type->gra_node->tp_tpdef);

			/* Don't forget initialization of allocated node! */
			if (arg->nd_type->gra_node->tp_init) {
				fprintf(f, "; %s((%s *) g_elem(",
					arg->nd_type->gra_node->tp_init,
					arg->nd_type->gra_node->tp_tpdef);
				gen_addr(f, arg);
				putc(',', f);
				if (options['a']) {
					gen_expr(f, nd->nd_target);
				}
				else	gen_addr(f, nd->nd_target);
				fprintf(f, "), \"%s:%d\")", nd->nd_pos.pos_filename, nd->nd_pos.pos_lineno);
			}
			break;

		default:
			gen_expr_st(f, nd->nd_target);
			fputs(" = ", f);
		}
	}
	switch(df->df_stdname) {
	case S_FROM:
	case S_ADDNODE:
		assert(nd->nd_target);
		break;

	case S_SIZE:
		arg = node_getlistel(args);
		fprintf(f, "%c_size(", base_char_of(arg->nd_type));
		gen_addr(f, arg);
		putc(')', f);
		break;

	case S_LB:
	case S_UB: {
		int i = 0;
		arg = node_getlistel(args);
		args = node_nextlistel(args);
		if (arg->nd_type->tp_fund == T_OBJECT) {
			gen_expr(f, arg);
			fprintf(f, "->state->%s[%ld]",
				df->df_stdname == S_LB ? "start" : "end",
				args ? node_getlistel(args)->nd_int - 1 : 0);
			break;
		}
		fprintf(f, "%s(", df->df_stdname == S_UB ? "a_ub" : "a_lb");
		gen_addr(f, arg);
		if (args) {
			i = node_getlistel(args)->nd_int - 1;
		}
		fprintf(f, ", %d)", i);
		}
		break;

	case S_ODD:
		arg = node_getlistel(args);
		fputs("(", f);
		gen_expr_ld(f, arg);
		fputs(" & 1)", f);
		break;

	case S_FLOAT:
	case S_TRUNC:
		arg = node_getlistel(args);
		fprintf(f, "((%s) ", nd->nd_type->tp_tpdef);
		gen_expr_ld(f, arg);
		fputs(")", f);
		break;

	case S_WRITELN:
	case S_WRITE:
		node_walklist(args, args, arg) {
			switch(arg->nd_type->tp_fund) {
			case T_INTEGER:
				if (arg->nd_type == longint_type) {
					fputs("f_InOut__WriteLongInt(", f);
				}
				else	fputs("f_InOut__WriteInt(", f);
				gen_expr_ld(f, arg);
				break;
			case T_REAL:
				if (arg->nd_type == longreal_type) {
					fputs("f_InOut__WriteLongReal(", f);
				}
				else	fputs("f_InOut__WriteReal(", f);
				gen_expr_ld(f, arg);
				break;
			case T_ENUM:
				fputs("f_InOut__WriteChar(", f);
				gen_expr_ld(f, arg);
				break;
			case T_ARRAY:
				fputs("f_InOut__WriteString(", f);
				gen_addr_ld(f, arg);
				break;
			default:
				assert(0);
			}
			putc(')', f);
			if (! node_emptylist(args)
			    || df->df_stdname == S_WRITELN) {
				putc(';', f);
			}
		}
		if (df->df_stdname == S_WRITELN) {
			fputs("f_InOut__WriteLn()", f);
		}
		break;

	case S_READ:
		node_walklist(args, args, arg) {
			switch(arg->nd_type->tp_fund) {
			case T_INTEGER:
				if (arg->nd_type == longint_type) {
					fputs("f_InOut__ReadLongInt(", f);
				}
				else if (arg->nd_type == shortint_type) {
					fputs("f_InOut__ReadShortInt(", f);
				}
				else	fputs("f_InOut__ReadInt(", f);
				gen_addr_st(f, arg);
				break;
			case T_REAL:
				if (arg->nd_type == longreal_type) {
					fputs("f_InOut__ReadLongReal(", f);
				}
				else if (arg->nd_type == shortreal_type) {
					fputs("f_InOut__ReadShortReal(", f);
				}
				else	fputs("f_InOut__ReadReal(", f);
				gen_addr_st(f, arg);
				break;
			case T_ENUM:
				fputs("f_InOut__ReadChar(", f);
				gen_addr_st(f, arg);
				break;
			case T_ARRAY:
				fputs("f_InOut__ReadString(", f);
				gen_addr_st(f, arg);
				break;
			default:
				assert(0);
			}
			putc(')', f);
			if (! node_emptylist(args)) putc(';', f);
		}
		break;

	case S_DELETENODE:
		arg = node_getlistel(args);
		args = node_nextlistel(args);
		nd = node_getlistel(args);
		fputs("g_deletenode(", f);
		if (options['a']) {
			gen_expr_ld(f, nd);
		}
		else	gen_addr_ld(f, nd);
		fputs(", ", f);
		gen_addr(f, arg);
		fputs(", ", f);
		if (arg->nd_type->gra_node->tp_flags & T_DYNAMIC) {
			fprintf(f, "%s)", arg->nd_type->gra_node->tp_freefunc);
		}
		else {
			fprintf(f, "(void *) 0)");
		}
		break;

	case S_INSERT:
	case S_DELETE:
		arg = node_getlistel(args);
		args = node_nextlistel(args);
		nd = node_getlistel(args);
		fprintf(f, "%c_%s(",
			base_char_of(nd->nd_type),
			df->df_stdname==S_INSERT ? "addel" : "delel");
		gen_addr(f, nd);
		fprintf(f, ", &%s, ",
			nd->nd_type->tp_descr);
		gen_addr(f, arg);
		putc(')', f);
		break;

	case S_ORD:
		arg = node_getlistel(args);
		fputs("((t_integer) ", f);
		gen_expr_ld(f, arg);
		putc(')', f);
		break;

	case S_VAL:
		arg = node_getlistel(node_nextlistel(args));
		fprintf(f, "((%s) ", nd->nd_type->tp_tpdef);
		if (nd->nd_type->tp_fund == T_INTEGER) {
			gen_expr_ld(f, arg);
			putc(')', f);
			break;
		}
		assert(nd->nd_type->tp_fund == T_ENUM);
		fputs("m_check(", f);
		gen_expr_ld(f, arg);
		fprintf(f, ", %d, %s, %d))", nd->nd_type->enm_ncst,
			CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		break;

	case S_CHR:
		arg = node_getlistel(args);
		fputs("((t_char) m_check(", f);
		gen_expr_ld(f, arg);
		fprintf(f, ", %d, %s, %d))", nd->nd_type->enm_ncst,
			CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		break;

	case S_CAP:
		arg = node_getlistel(args);
		fputs("m_cap(", f);
		gen_expr_ld(f, arg);
		putc(')', f);
		break;

	case S_ABS:
		arg = node_getlistel(args);
		fputs(nd->nd_type == int_type || nd->nd_type == shortint_type ? "m_abs("
		      : nd->nd_type == longint_type ? "m_labs("
		      : nd->nd_type == real_type || nd->nd_type == shortreal_type ? "m_rabs("
		      : "m_lrabs(",
		      f);
		gen_expr_ld(f, arg);
		putc(')', f);
		break;

	case S_ASSERT:
		arg = node_getlistel(args);
		fputs("m_assert(", f);
		gen_expr_ld(f, arg);
		fprintf(f, ", %s, %d", CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		putc(')', f);
		break;

	case S_NCPUS:
		fputs("m_ncpus()", f);
		break;

	case S_MYCPU:
		fputs("m_mycpu()", f);
		break;

	case S_STRATEGY:
		arg = node_getlistel(args);
		args = node_nextlistel(args);
		fputs("m_strategy(", f);
		gen_addr(f, arg);
		fputs(", ", f);
		arg = node_getlistel(args);
		args = node_nextlistel(args);
		gen_expr(f, arg);
		fputs(", ", f);
		arg = node_getlistel(args);
		gen_expr(f, arg);
		putc(')', f);
		break;

	default:
		crash("gen_stds");
	}
}

static void
gen_params(f, plist, tp)
	FILE	*f;
	p_node	plist;
	t_type	*tp;
{
	/*	Generate code for a pararmeter list.  Look at the parameter
		type to determine how to pass the parameter.
	*/
	t_def	*df;
	t_dflst	d;
	int	first = 1;

	def_walklist(tp->prc_params, d, df) {
		if (! first) fputs(", ", f);
		first = 0;
		if (is_shared_param(df)) {
			p_node	nd = node_getlistel(plist);

			gen_addr_stld(f, nd);
		}
		else if (is_out_param(df)) {
			gen_addr_st(f, node_getlistel(plist));
		}
		else {
			gen_IN_parameter(f, node_getlistel(plist));
		}
		plist = node_nextlistel(plist);
	}
}

static void
gen_target_assign(f, nd)
	FILE	*f;
	p_node	nd;
{
	if (nd->nd_type) {
		if (! is_constructed_type(nd->nd_type)) {
			if (nd->nd_target) {
				gen_expr_st(f, nd->nd_target);
				fputs(" = ", f);
			}
		}
		else if (nd->nd_type->tp_fund == T_GENERIC) {
			assert(nd->nd_target);
			fprintf(f, "ch%s(", nd->nd_type->tp_tpdef);
			gen_expr(f, nd->nd_target);
			fputs(" = ,NOTHING)", f);
		}
		/* Otherwise it is an extra parameter. */
	}
}

static void
gen_par_result(f, nd, tp, gencomma)
	FILE	*f;
	p_node	nd;
	t_type	*tp;
	int	gencomma;
{
	if (gencomma && nd->nd_parlist) fputs(", ", f);
	gen_params(f, nd->nd_parlist, tp);
	if (nd->nd_type) {
		/* See if there is an extra parameter for the result.
		   There is if the result type is constructed.
		   We don't know this if it is generic, so we let the
		   preprocessor decide in that case.
		*/
		if (nd->nd_type->tp_fund == T_GENERIC) {
			fprintf(f, "ch%s(NOTHING,", nd->nd_type->tp_tpdef);
			if (nd->nd_parlist || gencomma) fputs(" COMMA ", f);
			gen_addr(f, nd->nd_target);
			putc(')', f);
		}
		else if (is_constructed_type(nd->nd_type)) {
			assert(nd->nd_target);
			if (nd->nd_parlist || gencomma) putc(',', f);
			gen_addr_st(f, nd->nd_target);
		}
	}
}

void
gen_Call(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for a FORK, operation, or function call.
		The node 'nd' decides which.
	*/
	t_def	*df;
	p_node	dsg;
	p_node	l;
	int	i = 0;

	switch(nd->nd_symb) {
	case '(':
		/* Function call. */
		dsg = nd->nd_callee;
		if (dsg->nd_type == std_type) {
			/* Standard (built-in) function. */
			gen_stds(f, nd);
			return;
		}
		/* There may be a destination for the function result.
		   If so, generate lhs of assignment statement.
		*/
		gen_target_assign(f, nd);

		/* Produce function designator. If the called function has a
		   clone because it has exactly one shared parameter indicating
		   an object, call the clone if the actual parameter is not
		   shared, or the call is coming from a clone.
		*/
		if (dsg->nd_class == Def
		    && dsg->nd_def->df_kind == D_FUNCTION) {
			possibly_blocking |=
				dsg->nd_def->df_flags & D_BLOCKING;
			if (options['f'] &&
			    (dsg->nd_def->df_flags & D_HAS_SHARED_OBJ_PARAM)) {
				/* Callee has a clone. See if we can call it. */
				t_dflst	d;
				p_node	p;

				l = nd->nd_parlist;
				/* Find the actual parameter corresponding to
				   the "SHARED" formal.
				*/
				def_walklist(dsg->nd_type->prc_params, d, df) {
					if (is_shared_param(df) &&
					    df->df_type->tp_fund == T_OBJECT) {
						break;
					}
					l = node_nextlistel(l);
				}

				/* Found it. Call clone if we can. */
				assert(df);
				p = node_getlistel(l);
				if (clone_local_obj ||
				    indicates_local_object(p)) {
					fprintf(f, "%s(",dsg->nd_def->prc_name);
				}
				else if (CurrentScope->sc_definedby->df_kind == D_FUNCTION) {
					fprintf(f, "%s(",dsg->nd_def->df_name);
				}
				else {
					p_node	n = select_base(p);

					if (n->nd_class == Def &&
					    is_shared_param(n->nd_def)) {
						fprintf(f, "%s(",dsg->nd_def->df_name);
					}
					else {
						fputs("(o_isshared(", f);
						gen_addr(f, p);
						fprintf(f,
							") ? %s : %s)(",
							dsg->nd_def->df_name,
							dsg->nd_def->prc_name);
					}
				}
			}
			else {
				fprintf(f, "%s(", dsg->nd_def->df_name);
			}
			if (! (dsg->nd_def->df_flags & D_DEFINED)) {
			    def_enlist(&(ProcScope->sc_definedby->bod_transdep), dsg->nd_def);
			}
			if (dsg->nd_def->df_flags & D_EXTRAPARAM) {
				fputs("op_flags", f);
				gen_par_result(f, nd, dsg->nd_type, 1);
			}
			else	gen_par_result(f, nd, dsg->nd_type, 0);
		}
		else {
			possibly_blocking = D_BLOCKING;
			fprintf(f, "(*(%s)(m_getptr(", dsg->nd_type->prc_ftp);
			gen_expr_ld(f, dsg);
			fputs(")))(op_flags", f);
			gen_par_result(f, nd, dsg->nd_type, 1);
		}

		putc(')', f);
		break;

	case FORK:
		/* Useless comment: a FORK. */
		if (! node_emptylist(nd->nd_parlist)) {
			node_walklist(nd->nd_parlist, l, dsg) {
				fprintf(f, "argtab[%d] = ", i);
				gen_addr(f, dsg);
				fputs(";\n", f);
				indent(f);
				i++;
			}
		}
		dsg = nd->nd_callee;
		fputs("DoFork(", f);

		/* CPU on which the new process must be started. */
		if (nd->nd_target) {
			gen_expr(f, nd->nd_target);
		}
		else	fputs("m_mycpu()", f);

		/* The process indication. */
		fprintf(f, ", &%s", dsg->nd_def->prc_name);

		/* And the parameters. */
		if (! node_emptylist(nd->nd_parlist)) {
			fputs(", argtab)", f);
		}
		else fputs(", (void **) 0)", f);
		break;

	case DOLDOL:
		if (! strcmp(nd->nd_callee->nd_idf->id_text, "distribute") ||
		    ! strcmp(nd->nd_callee->nd_idf->id_text, "distribute_on_n") ||
		    ! strcmp(nd->nd_callee->nd_idf->id_text, "distribute_on_list")) {
			t_node	*n;

			fprintf(f, "p_%s(", nd->nd_callee->nd_idf->id_text);
			gen_expr(f, nd->nd_obj);
			fputs(", ", f);
			fprintf(f, "(po_opcode) %s", nd->nd_obj->nd_type->tp_def->mod_initname);
			l = nd->nd_parlist;
			if (! strcmp(nd->nd_callee->nd_idf->id_text, "distribute_on_list")) {
				fputs(", ", f);
				gen_addr(f, l);
				l = node_nextlistel(l);

			}
			node_walklist(l,  l, n) {
				fputs(", ", f);
				gen_expr(f, n);
			}
			fputs(");\n", f);
			indent(f);
			fputs("do_create_gather_channel(", f);
			gen_expr(f, nd->nd_obj);
			fputs(")", f);
			break;
		}
		if (! strcmp(nd->nd_callee->nd_idf->id_text, "add_dependency") ||
		    ! strcmp(nd->nd_callee->nd_idf->id_text, "clear_dependencies") ||
		    ! strcmp(nd->nd_callee->nd_idf->id_text, "set_dependencies") ||
		    ! strcmp(nd->nd_callee->nd_idf->id_text, "remove_dependency")) {
			fprintf(f, "p_%s(", nd->nd_callee->nd_idf->id_text);
			gen_expr(f, nd->nd_obj);
			fprintf(f, ", (po_opcode) %s",
			  node_getlistel(nd->nd_parlist)->nd_def->opr_namew);
			l = node_nextlistel(nd->nd_parlist);
			node_walklist(l, l, nd) {
				fputs(", ", f);
				gen_expr(f, nd);
			}
			fputs(")", f);
			break;
		}

		if (! strcmp(nd->nd_callee->nd_idf->id_text, "partition")) {
			fputs("p_partition(", f);
		}
		else {
			fprintf(f, "do_%s(", nd->nd_callee->nd_idf->id_text);
		}
		gen_expr(f, nd->nd_obj);
		node_walklist(nd->nd_parlist, l, nd) {
			fputs(", ", f);
			gen_expr(f, nd);
		}
		fputs(")", f);
		break;

	case '$':
		/* An operation. More optimizations are possible here,
		   f.i. when an object is local.
		*/
		df = nd->nd_callee->nd_def;
		/* Enlist the translation dependency, so that it ends
		   up in the database, and the driver can check.
		*/
		if (! (df->df_flags & D_DEFINED)) {
		    def_enlist(&(ProcScope->sc_definedby->bod_transdep), df);
		}
		if (! (df->df_scope->sc_definedby->df_flags & D_PARTITIONED) &&
		    (df->df_flags & D_NONBLOCKING)) {
			/* If the operation is non-blocking, we try to do
			   it in-line, because this has less overhead than
			   going through DoOperation. Also note that when
			   the operation is non-blocking, it cannot have
			   both reads and writes. If we don't know what it is,
			   we assume that it reads. This may cause unresolved
			   references when linking, so the driver should check
			   consistency before linking, and recompile when
			   neccessary.
			*/
			int got_lock = 0;

			if (! (df->df_flags & (D_HASREADS|D_HASWRITES))) {
				df->df_flags |= D_HASREADS;
			}

			/* Ask permission from the RTS. Should be left out if
			   the object is local.
			*/
			if (! clone_local_obj &&
			    ! indicates_local_object(nd->nd_obj)) {
				fprintf(f, "if (o_start_%s(",
					(df->df_flags & D_HASWRITES) ? "write" : "read");
				gen_addr(f, nd->nd_obj);
				fputs(")) {\n", f);
				indentlevel += 4;
				indent(f);
				got_lock = 1;
			}

			/* Generate call. */
			gen_target_assign(f, nd);
			fprintf(f, "%s(",
				(df->df_flags & D_HASWRITES) ?
					df->opr_namew :
					df->opr_namer);
			gen_addr(f, nd->nd_obj);
			gen_par_result(f, nd, df->df_type, 1);
			if (! got_lock) {
				putc(')', f);
				break;
			}
			fputs(");\n", f);
			indent(f);

			/* Tell RTS that we are finished. If it was a write,
			   tell RTS that it succeeded (no guards, remember?).
			*/
			fprintf(f, "o_end_%s(",
				(df->df_flags & D_HASWRITES) ? "write" : "read");
			gen_addr(f, nd->nd_obj);
			fprintf(f, "%s);\n",
				df->df_flags & D_HASWRITES ? ",1" : "");
			indentlevel -= 4;
			indent(f);
			fputs("} else ", f);
		}
		else if (! (df->df_flags & D_NONBLOCKING)) {
			possibly_blocking = D_BLOCKING;
		}
		/* Call DoOperation, with all its parameters. */
		if (! node_emptylist(nd->nd_parlist)) {
			node_walklist(nd->nd_parlist, l, dsg) {
				fprintf(f, "(argtab[%d] = ", i);
				gen_addr(f, dsg);
				fputs("),\n", f);
				indent(f);
				i++;
			}
		}
		if (nd->nd_type) {
			assert(nd->nd_target);
			fprintf(f, "(argtab[%d] = ", i);
			gen_addr(f, nd->nd_target);
			fputs("),\n", f);
			i++;
			indent(f);
		}
		if (! (df->df_scope->sc_definedby->df_flags & D_PARTITIONED) &&
		    (df->df_flags & D_NONBLOCKING) &&
		    (! node_emptylist(nd->nd_parlist) || nd->nd_type)) {
			fputs("    ", f);
		}
		if (df->df_scope->sc_definedby->df_flags & D_PARTITIONED) {
			fputs("do_operation(", f);
			gen_expr(f, nd->nd_obj);
			fprintf(f, ", (po_opcode) %s, ", df->opr_namew);

		}
		else {
			fputs("DoOperation(", f);
			gen_addr(f, nd->nd_obj);
			fprintf(f, ", op_flags, &%s, %d, 0, ",
				df->df_type->prc_objtype->tp_descr,
				df->prc_funcno);
		}
		if (! node_emptylist(nd->nd_parlist) || nd->nd_type) {
			fputs("argtab)", f);
		}
		else	fputs("(void **) 0)", f);
		if ((df->df_scope->sc_definedby->df_flags & D_PARTITIONED) &&
		    ! pure_write(df)) {
			fputs(";\n", f);
			indent(f);
			fputs("wait_for_end_of_invocation(", f);
			gen_expr(f, nd->nd_obj);
			fputs(")", f);
		}
		break;
	default:
		crash("gen_Call");
	}
}

static void
gen_AorG_Sel(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for an array - or graph element reference.
		This routine generates code for "address off".
	*/
	t_type	*tp = nd->nd_left->nd_type;

	if (tp->tp_fund == T_ARRAY) {
		fprintf(f, "(&((%s *)", nd->nd_type->tp_tpdef);
		gen_addr(f, nd->nd_left);
		fputs("->a_data)[", f);
		gen_expr(f, nd->nd_right);
		fputs("])", f);
	}
	else {
		assert(tp->tp_fund == T_GRAPH);
		fprintf(f, "((%s *) g_elem(", tp->gra_node->tp_tpdef);
		gen_addr(f, nd->nd_left);
		fputs(", ", f);
		if (options['a']) {
			gen_expr(f, nd->nd_right);
		}
		else	gen_addr(f, nd->nd_right);
		fputs("))", f);
	}
}

static void
gen_Ofldselect(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for a partitioned object field selection.
	*/
	p_node	left = nd->nd_left;

	assert(nd->nd_class == Ofldsel);
	assert(left->nd_class == Def);
	fputs("(&(datap[", f);
	gen_expr(f, nd->nd_right);
	fprintf(f, "].%s))", left->nd_def->df_name);
}

static void
gen_Select(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for a selection.
	*/
	char	*nm;
	p_node	left = nd->nd_left;
	p_node	right = nd->nd_right;

	if (nd->nd_class == Ofldsel) {
		assert(left->nd_class == Def);
		fputs("(datap[", f);
		gen_expr(f, nd->nd_right);
		fprintf(f, "].%s)", left->nd_def->df_name);
		return;
	}
	assert(right->nd_class == Def);
	nm = right->nd_def->df_name;
	switch(left->nd_type->tp_fund) {
	case T_GRAPH:
		gen_expr(f, left);
		fprintf(f, ".g_root.%s", nm);
		break;
	case T_UNION:
		gen_expr(f, left);
		if (! (right->nd_def->df_flags & D_TAG)) {
			fprintf(f, ".u_el.%s", nm);
			break;
		}
		fprintf(f, ".%s", nm);
		break;
	case T_RECORD:
		gen_expr(f, left);
		fprintf(f, ".%s", nm);
		break;
	case T_OBJECT:
		fprintf(f, "(((%s *)",
			record_type_of(left->nd_type)->tp_tpdef);
		gen_expr(f, left);
		fprintf(f, ".o_fields)->%s)", nm);
		break;
	default:
		crash("gen_Select");
	}
}

char *
c_sym(s)
	int	s;
{
	/*	Return the C operator (as a string) that corresponds to the
		Orca operator in 's'.
	*/
	switch(s) {
	case ARR_INDEX:
		return " ARR_INDEX ";
	case ARR_SIZE:
		return " ARR_SIZE ";
	case '+':
	case PLUSBECOMES:
		return "+";
	case '-':
	case MINBECOMES:
		return "-";
	case '*':
	case TIMESBECOMES:
		return "*";
	case '<':
		return "<";
	case '>':
		return ">";
	case '/':
	case DIVBECOMES:
		return "/";
	case LEFTSHIFT:
	case LSHBECOMES:
		return "<<";
	case RIGHTSHIFT:
	case RSHBECOMES:
		return ">>";
	case '|':
	case B_ORBECOMES:
		return "|";
	case '&':
	case B_ANDBECOMES:
		return "&";
	case '^':
	case B_XORBECOMES:
		return "^";
	case LESSEQUAL:
		return "<=";
	case GREATEREQUAL:
		return ">=";
	case AND:
	case ANDBECOMES:
		return "&&";
	case OR:
	case ORBECOMES:
		return "||";
	case NOT:
		return "!";
	case '~':
		return "~";
	case '=':
		return "==";
	case NOTEQUAL:
		return "!=";
	}
	crash("c_sym");
	return "";
}

static int
test_nil(nd)
	p_node	nd;
{
	if (nd->nd_class == Value &&
	    nd->nd_type == nil_type) return 1;
	return 0;
}

static void
gen_Oper(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	Generate code for a binary operator.
	*/
	p_node	left = nd->nd_left,
		right = nd->nd_right;

	switch(nd->nd_symb) {
	case '=':
	case NOTEQUAL:
		if (is_constructed_type(left->nd_type)) {
			/* There is no direct equivalent in C, but there
			   is support from the RTS: the ?_cmp routine.
			*/
			if (nd->nd_symb == NOTEQUAL) fputs("(!", f);
			if (test_nil(left)) {
				fprintf(f, "n_isnil(");
				gen_addr(f, right);
				putc(')', f);
			}
			else if (test_nil(right)) {
				fprintf(f, "n_isnil(");
				gen_addr(f, left);
				putc(')', f);
			}
			else {
				fprintf(f, "%s(", left->nd_type->tp_comparefunc);
				gen_addr(f, left);
				putc(',', f);
				gen_addr(f, right);
				fputs(")", f);
			}
			if (nd->nd_symb == NOTEQUAL) putc(')', f);
			break;
		}
		/* fall through */
	case '+':
	case '-':
	case '*':
	case '<':
	case '>':
	case GREATEREQUAL:
	case LESSEQUAL:
	case AND:
	case OR:
	case LEFTSHIFT:
	case RIGHTSHIFT:
	case '|':
	case '&':
	case '^':
		/* These operators have a direct equivalent in C. */
		putc('(', f);
		gen_expr(f, left);
		fputs(c_sym(nd->nd_symb), f);
		gen_expr(f, right);
		putc(')', f);
		break;
	case '/':
	case '%':
		/* For integers, a RTS routine is called because C does not
		   define the effect for negative integers.
		*/
		if (nd->nd_type->tp_fund == T_INTEGER) {
			fputs(nd->nd_symb == '%' ? "m_mod" : "m_div", f);
			if (nd->nd_type == longint_type) putc('l', f);
		}
		putc('(', f);
		gen_expr(f, left);
		fputs(nd->nd_type->tp_fund != T_INTEGER ?
				c_sym(nd->nd_symb) : ",",
			f);
		gen_expr(f, right);
		putc(')', f);
		break;
	case IN:
		/* Test for set or bag membership. Call RTS routine ?_member.
		*/
		fprintf(f, "%c_member(", base_char_of(right->nd_type));
		gen_addr(f, right);
		fprintf(f, ", &%s, ",
			right->nd_type->tp_descr);
		gen_addr(f, left);
		putc(')', f);
		break;
	default:
		crash("gen_Oper");
	}
}

static void
gen_defnam(f, df)
	FILE	*f;
	t_def	*df;
{
	/*	Produce a reference to the designator indicated by 'df'.
	*/
	switch(df->df_kind) {
	case D_OFIELD:
		if (df->df_flags & D_UPPER_BOUND) {
			fprintf(f, "se%d", df->fld_dimno);
		}
		else if (df->df_flags & D_LOWER_BOUND) {
			fprintf(f, "ss%d", df->fld_dimno);
		}
		else fprintf(f, "(v__ofldp->%s)", df->df_name);
		break;
	case D_FUNCTION:
		fprintf(f, "(%s[%d])", CurrDef->mod_funcaddrname, df->prc_funcno-1);
		break;
	default:
		if (df->df_kind == D_VARIABLE) {
			if (df->df_flags & D_FORLOOP) {
				fprintf(f, TMP_FMT, df->var_tmpvar->tmp_id);
				break;
			}
			if (df->df_flags & D_GATHERED) {
				fprintf(f, "(*%s)", df->df_name);
				break;
			}
		}
		fprintf(f, "%s", df->df_name);
		break;
	}
}
