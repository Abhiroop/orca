/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: generate.c,v 1.66 1998/06/11 12:00:49 ceriel Exp $ */

/* General utility routines for code generation. */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <flt_arith.h>
#include <assert.h>
#include <alloc.h>

#include "generate.h"
#include "scope.h"
#include "misc.h"
#include "gen_expr.h"
#include "gen_code.h"
#include "error.h"
#include "main.h"
#include "const.h"
#include "visit.h"
#include "options.h"
#include "chk.h"
#include "marshall.h"

_PROTOTYPE(static void gen_tp_inst, (FILE *, t_type *));
_PROTOTYPE(static void gen_tp_uninst, (FILE *, t_type *));
_PROTOTYPE(static void gen_str, (t_string *, int));
_PROTOTYPE(static void gen_const_aggr, (p_node, int, char *));
_PROTOTYPE(static void gen_member_aggr, (p_node));
_PROTOTYPE(static int memb_aggr, (p_node));
_PROTOTYPE(static int gen_memaggr, (p_node));
_PROTOTYPE(static void new_init_macros, (t_def *));
_PROTOTYPE(static void gen_unioninit, (t_def *));
_PROTOTYPE(static void gen_initu, (p_node, char *));
_PROTOTYPE(static void gen_type_names, (t_type *, t_def *));
_PROTOTYPE(static void gen_init_name, (t_type *, char *));
_PROTOTYPE(static void instantiate_on_file, (t_def *, FILE *));
_PROTOTYPE(static void assign_fields, (t_type *));
_PROTOTYPE(static char *gen_dummyname, (char *, int));
_PROTOTYPE(static void gen_value, (FILE *f, p_node nd));

static int dummy_cnt;

void
init_gen()
{
	dummy_cnt = 0;
}

void
walkdefs(dflist, dfkinds, func)
	t_def	*dflist;
	int	dfkinds;
#if __STDC__
	void	(*func)(t_def *);
#else
	void	(*func)();
#endif
{
	/*	Walk through a list of definitions, applying function "func"
		to each definition with df_kind in "dfkinds".
	*/
	t_scope	*sc = CurrentScope;
	t_scope	*psc = ProcScope;

	if (dflist) {
		CurrentScope = dflist->df_scope;
		if (CurrentScope->sc_definedby
		    && (CurrentScope->sc_definedby->df_kind
			& (D_ERROR|D_OBJECT|D_MODULE|D_OPERATION|D_FUNCTION|D_PROCESS))) {
			ProcScope = CurrentScope;
		}
		while (dflist) {
			if (dflist->df_kind & dfkinds) {
				(*func)(dflist);
			}
			dflist = dflist->df_nextinscope;
		}
		CurrentScope = sc;
		ProcScope = psc;
	}
}

char *
gen_name(pref, id_str, fix)
	char	*pref,
		*id_str;
	int	fix;
{
	/*	Generate a name using prefix "pref" and the string "id_str".
		If in a generic module or object, and "fix" is not set,
		leave this to the preprocessor, because in this case the
		name of the scope is a macro. Otherwise, we use the following
		scheme: <pref><scopename>__<id_str>. <pref> should probably
		end with an underscore.
	*/

	if (CurrDef->df_flags & D_GENERIC) {
	    if (! fix) {
		if (*pref == '\0') {
			return mk_str("_concat(", CurrentScope->sc_genname,
					",__", id_str, ")", (char *) 0);
		}
		return mk_str("_concat(_concat(", pref, ",",
			CurrentScope->sc_genname, "),__", id_str, ")",
			(char *) 0);
	    }
	    return mk_str(pref, CurrentScope->sc_name,
				"__", id_str, (char *) 0);
	}
	if (! fix) {
		return mk_str(pref, CurrentScope->sc_genname, "__",
			id_str, (char *) 0);
	}
	return mk_str(pref, CurrentScope->sc_name, "__", id_str, (char *) 0);
}

char *
gen_trace_name(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_OPERATION:
	case D_PROCESS:
		if (CurrDef->df_flags & D_GENERIC) {
			return mk_str("_str(_concat(",
				      CurrentScope->sc_genname,
				      ",.",
				      df->df_idf->id_text,
				      "))",
				      (char *) 0);
		}
		return mk_str("\"",
			      CurrentScope->sc_name,
			      ".",
			      df->df_idf->id_text,
			      "\"",
			      (char *) 0);

	case D_VARIABLE:
		if (df->df_flags & D_DATA) {
			if (CurrDef->df_flags & D_GENERIC) {
				return mk_str("_str(_concat(",
					      CurrentScope->sc_genname,
					      ",.",
					      df->df_idf->id_text,
					      "))",
					      (char *) 0);
			}
			return mk_str("\"",
				      CurrentScope->sc_name,
				      ".",
				      df->df_idf->id_text,
				      "\"",
				      (char *) 0);
		}
		if (CurrDef->df_flags & D_GENERIC) {
			return mk_str("_str(_concat(",
				      enclosing(ProcScope)->sc_genname,
				      ",.",
				      ProcScope->sc_definedby->df_idf->id_text,
				      ".",
				      df->df_idf->id_text,
				      "))",
				      (char *) 0);
		}
		return mk_str("\"",
			      enclosing(ProcScope)->sc_name,
			      ".",
			      ProcScope->sc_definedby->df_idf->id_text,
			      ".",
			      df->df_idf->id_text,
			      "\"",
			      (char *) 0);
	case D_OFIELD:
		if (CurrDef->df_flags & D_GENERIC) {
			return mk_str("_str(_concat(",
				      CurrDef->bod_scope->sc_genname,
				      ",.",
				      df->df_idf->id_text,
				      "))",
				      (char *) 0);
		}
		return mk_str("\"",
			      CurrDef->bod_scope->sc_name,
			      ".",
			      df->df_idf->id_text,
			      "\"",
			      (char *) 0);

	case D_OBJECT:
	case D_MODULE:
		if (df->df_flags & D_GENERIC) {
			return mk_str("_str(",
				      df->bod_scope->sc_genname,
				      ")",
				      (char *) 0);
		}
		return mk_str("\"", df->df_idf->id_text, "\"", (char *) 0);
	}
	return (char *) 0;
}

static char *
gen_dummyname(pref, fix)
	char	*pref;
	int	fix;
{
	/*	Generate a name for a dummy using prefix "pref".
		If in a generic module or object, and "fix" is not set,
		leave this to the preprocessor, because in this case the
		name of the scope is a macro. Otherwise, we use the following
		scheme: <pref><scopename>__<cnt>. <pref> should probably
		end with an underscore.
	*/

	char	buf[20];

	sprintf(buf, "%d", dummy_cnt++);
	if ((CurrDef->df_flags & D_GENERIC) && !fix) {
		return mk_str("_concat(_concat(", pref, ",",
			CurrentScope->sc_name, "),__", buf, ")", (char *) 0);
	}
	return mk_str(pref, CurrentScope->sc_name, "__", buf, (char *) 0);
}

static FILE *macro_f;

void
gen_instantiation(instdef)
	t_def	*instdef;
{
	/*	Instantiation is done by defining some macros and then
		including. If the instantiation is in a specification
		module, it ends up in both the generated include file and
		the generated C file.
	*/

	if (instdef->df_flags & D_INSTDONE) return;
	instdef->df_flags |= D_INSTDONE;

	macro_f = fc;

	if (instdef->df_flags & D_EXPORTED) {
		instantiate_on_file(instdef, fh);
		macro_f = fh;
	}
	instantiate_on_file(instdef, fc);

	/* Generate new initialization macros for the types exported from
	   generic modules/objects.
	   However, we must first change the name of the scope, because
	   we may get name conflicts otherwise. This is only a problem when
	   translating generic modules/objects, because otherwise sc_genname
	   can be used to produce the name.
	*/
	if (CurrDef->df_flags & D_GENERIC) {
		char *s = mk_str(instdef->df_scope->sc_name,
				 "__",
				 instdef->df_idf->id_text,
				 (char *) 0);
		instdef->bod_scope->sc_name = s;
	}

	walkdefs(instdef->bod_scope->sc_def, D_OBJECT|D_MODULE|D_TYPE, new_init_macros);

	/* Finished ... */
	if (instdef->df_flags & D_EXPORTED) {
		fprintf(fh, "\n/**** End of instantiation. ****/\n\n");
	}
	fprintf(fc, "\n/**** End of instantiation. ****/\n\n");
}

static void
instantiate_on_file(instdef, f)
	t_def	*instdef;
	FILE	*f;
{
	/*	Common code for instantiation on C-file as well as on
		include file.
	*/

	t_scope	*sc = instdef->bod_scope;
	t_def	*df = sc->sc_def;

	/* First, generate a macro for the module name. */
	fprintf(f, "\n/**** Instantiate %s from %s. ****/\n\n",
		instdef->df_idf->id_text, sc->sc_name);
	fprintf(f,
		"#define %s _concat(%s,__%s)\n",
		sc->sc_name,
		instdef->df_scope->sc_name,
		instdef->df_idf->id_text);

	/* Then, produce macros for the generic parameters. */
	while (df) {
		if ((df->df_kind & (D_TYPE|D_CONST|D_FUNCTION))
		    && (df->df_flags & D_GENERICPAR)) {
			switch(df->df_kind) {
			case D_TYPE:
				gen_tp_inst(f, df->df_type);
				break;
			case D_CONST:
				if ((df->con_const->nd_class & (Aggr|Row))
				    && ! df->con_const->nd_dummynam) {
					df->con_const->nd_dummynam =
						gen_dummyname("aggr_", 0);
					gen_const_aggr(
						df->con_const,
						f == fh,
						df->con_const->nd_dummynam);
				}
				fprintf(f, "#define %s ", df->df_name);
				gen_value(f, df->con_const);
				fputs("\n", f);
				break;
			case D_FUNCTION:
				fprintf(f, "#define %s %s\n",
					df->df_name,
					df->con_const->nd_def->df_name);
				fprintf(f, "#define extra%s ", df->df_name);
				if (df->con_const->nd_def->df_flags & D_GENERICPAR) {
					fprintf(f, "extra%s",
					    df->con_const->nd_def->df_name);
				}
				else if (df->con_const->nd_def->df_flags & D_EXTRAPARAM) {
					fputs("op_flags, ", f);
				}
				fputs("\n", f);
			}
		}
		df = df->df_nextinscope;
	}

	/* Then, #include. */
	if (f == fh) {
		fprintf(f, "#include \"%s.gh\"\n", instdef->mod_gendf->mod_file);
	}
	if (f == fc) {
		if (! (instdef->df_flags & D_EXPORTED)) {
			fprintf(f, "#include \"%s.gh\"\n", instdef->mod_gendf->mod_file);
		}
		fprintf(f, "#include \"%s.gc\"\n", instdef->mod_gendf->mod_file);
	}

	/* Produce #undef for the module name. */
	fprintf(f, "#undef %s\n", sc->sc_name);

	/* Produce #undefs for the macros produced for the generic parameters. */
	df = sc->sc_def;
	while (df) {
		if ((df->df_kind & (D_TYPE|D_CONST|D_FUNCTION))
		    && (df->df_flags & D_GENERICPAR)) {
			switch(df->df_kind) {
			case D_TYPE:
				gen_tp_uninst(f, df->df_type);
				if (f == fh) break;
				free(df->df_type->tp_tpdef);
				free(df->df_type->tp_descr);
				if (df->df_type->tp_init) {
					free(df->df_type->tp_init);
				}
				*(df->df_type) =
					*(generic_actual_of(df->df_type));
				break;
			case D_CONST:
			case D_FUNCTION:
				fprintf(f, "#undef %s\n", df->df_name);
				if (f == fh) break;
				if (df->df_kind == D_FUNCTION) {
					df->df_name = df->con_const->nd_def->df_name;
					fprintf(f, "#undef extra%s\n", df->df_name);
				}
				break;
			}
		}
		df = df->df_nextinscope;
	}
}

static void
new_init_macros(df)
	t_def	*df;
{
	/*	A generic object/module exports a type. This type
		could have a generic sub-type, so an initialization
		macro must be produced for each instantiation.
		Unfortunately, this cannot be done with the
		preprocessor, like the other names, because macro
		names cannot be constructed in that way, so we
		have to do this for every instantiation.
		The alternative is to have an initialization function
		instead of a macro, but this results in a performance
		penalty, and we don't want to do that (don't punish the
		user for using GENERICS).
	*/

	if (! (df->df_flags & D_EXPORTED)) return;

	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		if (df->df_flags & D_INSTANTIATION) {
			walkdefs(df->bod_scope->sc_def,
				 D_OBJECT|D_MODULE|D_TYPE,
				 new_init_macros);
		}
		break;
	case D_TYPE:
		if (df->df_flags & D_GENERICPAR) break;
		if (! df->df_type->tp_def
		    || df->df_type->tp_def != df) break;
		gen_init_name(df->df_type, df->df_idf->id_text);
		df->df_type->tp_flags &= ~T_INIT_DONE;
		gen_init_macro(df->df_type, macro_f);
		break;
	}
}

void
assign_name(df)
	t_def	*df;
{
	/*	Assign a name for code generation to fields, variables, etc.
	*/

	char	*nm = df->df_idf->id_text;
	t_type	*tp = df->df_type;
	static int
		level;

	if (df->df_name &&
	    !(df->df_kind & (D_FUNCTION|D_PROCESS|D_OPERATION))) return;

	switch(df->df_kind) {
	case D_CONST:
		df->df_name = gen_name("c_", nm, df->df_flags & D_GENERICPAR);
		if (!(df->df_flags & D_GENERICPAR) &&
		    df->con_const->nd_class == Value &&
		    df->con_const->nd_type == string_type) {
			df->con_const->nd_str->s_name = df->df_name;
		}
		break;
	case D_PROCESS:
		if (! df->prc_name) {
			if (df->df_flags & D_MAIN) {
				df->prc_name = Salloc("OrcaMain", 9);
			}
			else	df->prc_name = gen_name("pr_", df->df_idf->id_text, 0);
			df->bod_marshallnames[0] = gen_name("sz_arg_", nm, 0);
			df->bod_marshallnames[1] = gen_name("ma_arg_", nm, 0);
			df->bod_marshallnames[2] = gen_name("um_arg_", nm, 0);
		}
		gen_type_names(tp, (t_def *) 0);
		/* fall through */
	case D_FUNCTION:
		if (! df->df_name) {
			df->df_name =
				gen_name("f_", nm, df->df_flags & D_GENERICPAR);
		}
		if (df->df_kind == D_FUNCTION &&
		    (df->df_flags & D_HAS_SHARED_OBJ_PARAM)) {
			df->prc_name =
			    gen_name("onf_", nm, df->df_flags & D_GENERICPAR);
		}
		if (! (df->df_flags & D_GENERICPAR)) {
			walkdefs(df->bod_scope->sc_def,
				D_TYPE|D_CONST|D_VARIABLE,
				assign_name);
		}
		break;
	case D_VARIABLE:
		/* Variables may have an anonymous type. */
		if (df->df_flags & D_SELF) break;
		if (! tp->tp_def) {
			gen_type_names(tp, (t_def *) 0);
		}
		if (! df->df_name) {
		    if (df->df_flags & D_DATA) {
			df->df_name = gen_name("v_", nm, 0);
		    }
		    else	df->df_name = mk_str("v_", nm, (char *) 0);
		}
		assign_fields(tp);
		break;
	case D_OFIELD:
	case D_FIELD:
	case D_UFIELD:
		df->df_name = mk_str("f_", nm, (char *) 0);
		assign_fields(tp);
		break;
	case D_OPERATION:
		if (! df->opr_namer) {
			if (CurrDef->df_flags & D_PARTITIONED) {
				df->opr_namer = gen_name("op_", nm, 0);
				df->opr_namew = gen_name("op__", nm, 0);
				if (df->opr_dependencies) {
					df->opr_depname = gen_name("deps_", nm, 0);
				}
			}
			else {
				df->opr_namer = gen_name("or_", nm, 0);
				df->opr_namew = gen_name("ow_", nm, 0);
			}
			df->bod_marshallnames[0] = gen_name("sz_call_", nm, 0);
			df->bod_marshallnames[1] = gen_name("ma_call_", nm, 0);
			df->bod_marshallnames[2] = gen_name("um_call_", nm, 0);
			df->bod_marshallnames[3] = gen_name("sz_ret_", nm, 0);
			df->bod_marshallnames[4] = gen_name("ma_ret_", nm, 0);
			df->bod_marshallnames[5] = gen_name("um_ret_", nm, 0);
			df->bod_marshallnames[6] = gen_name("fr_ret_", nm, 0);
		}
		walkdefs(df->bod_scope->sc_def,
			 D_TYPE|D_CONST|D_VARIABLE,
			 assign_name);
		break;
	case D_MODULE:
	case D_OBJECT: {
		/* The order in which names are assigned is a bit tricky,
		   because the names depend on CurrDef. Therefore, imported
		   modules/objects must be handled first, so that types
		   imported from them already have a name.
		*/
		t_def *df1;
		t_dflst l;

		if (df->df_flags & D_BUSY) break;
		df->df_flags |= D_BUSY;
		if (df->df_flags & D_DATA) {
			df->df_name = mk_str("d_", nm, (char *) 0);
		}
		def_walklist(df->mod_hincludes, l, df1) {
			if (! (df1->df_flags & D_GENERIC)) {
				assign_name(df1);
			}
		}
		def_walklist(df->mod_cincludes, l, df1) {
			if (! (df1->df_flags & D_GENERIC)) {
				assign_name(df1);
			}
		}
		if (! level) {
			t_scope *sc = CurrentScope;
			CurrDef = df;
			CurrentScope = df->bod_scope;
			if (df->df_kind == D_OBJECT && ! df->df_name) {
				df->df_name = gen_name("poc_", nm, 0);
				df->mod_initname = gen_name("poi_", nm, 0);
			}
			CurrentScope = sc;
		}
		level++;
		if (! df->mod_fn) {
			t_scope	*sc = CurrentScope;
			CurrentScope = CurrDef->bod_scope;
			df->bod_marshallnames[0] = gen_name("sz_obj_", nm, 0);
			df->bod_marshallnames[1] = gen_name("ma_obj_", nm, 0);
			df->bod_marshallnames[2] = gen_name("um_obj_", nm, 0);
			df->mod_fn = gen_name("fn_", nm, 0);
			CurrentScope = sc;
		}
		for (df1 = df->bod_scope->sc_def; df1; df1 = df1->df_nextinscope) {
			if ((df1->df_kind & (D_OBJECT|D_MODULE))
			    && (df1->df_flags & D_INSTANTIATION)) {
				assign_name(df1);
			}
		}
		walkdefs(df->bod_scope->sc_def,
			 D_OFIELD|D_ISTYPE|D_CONST|D_OPERATION|D_VARIABLE|
				D_PROCESS|D_FUNCTION,
			 assign_name);
		level--;
		df->df_flags &= ~D_BUSY;
		}
		if (CurrentScope == GlobalScope) break;
		/* Fall through */
	case D_TYPE:
	case D_OPAQUE:
		assign_fields(tp);
		gen_type_names(tp, df);
		break;
	}
}

static void
assign_fields(tp)
	t_type	*tp;
{
	switch(tp->tp_fund) {
	case T_GRAPH:
		walkdefs(tp->gra_root->rec_scope->sc_def,
			 D_FIELD,
			 assign_name);
		walkdefs(tp->gra_node->rec_scope->sc_def,
			 D_FIELD,
			 assign_name);
		break;
	case T_UNION:
	case T_RECORD:
		walkdefs(tp->rec_scope->sc_def,
			 D_FIELD|D_UFIELD,
			 assign_name);
		break;
	case T_OBJECT:
		/* Handled in assign_name. */
		break;
	case T_ARRAY:
	case T_SET:
	case T_BAG:
		assign_fields(element_type_of(tp));
		break;
	}
}

void
gen_value(f, nd)
	FILE	*f;
	p_node	nd;
{
	/*	'nd' indicates an actual parameter for an instantiation.
		Print a suitable definition.
	*/

	switch(nd->nd_class) {
	case Def:
		if (nd->nd_def->df_kind == D_CONST
		    && nd->nd_def->con_const) {
			gen_value(f, nd->nd_def->con_const);
			break;
		}
		fprintf(f, "(&%s)", nd->nd_def->df_name);
		break;
	case Value:
		if (nd->nd_type == nil_type) {
			fputs("(&nil)", f);
			break;
		}
		if (nd->nd_type == string_type) {
			fprintf(f, "(&%s)", nd->nd_str->s_name);
			break;
		}
		gen_numconst(f, nd);
		break;
	case Aggr:
	case Row:
		fprintf(f, "(&%s)", nd->nd_dummynam);
		break;
	default:
		crash("gen_value");
		break;
	}
}

static void
gen_tp_inst(f, tp)
	FILE	*f;
	t_type	*tp;
{
	/*	Produce #defines for the generic type indicated by 'tp'.
		Defines are required for
		- the type definition
		- the type descriptor
		- an assignment function
		- and, if the generic parameter is just TYPE:
		  - macros to indicate whether the type is constructed and/or
		    dynamic
		  - a macro that chooses between "simple" and "constructed".
		    (this is needed for parameter passing).
		  - an initialization macro
		  - a de-allocating macro
		  - marshalling macros
	*/
	t_type	*actual = generic_actual_of(tp);

	fprintf(f, "#define %s %s\n", tp->tp_tpdef, actual->tp_tpdef);
	fprintf(f, "#define %s %s\n", tp->tp_descr, actual->tp_descr);

	/* Assignment function */
	if (f == fc) {
		fprintf(f, "#define %s %s\n",
			tp->tp_assignfunc,
			actual->tp_assignfunc);
	}

	if (tp->tp_fund == T_GENERIC) {
		fprintf(f, "#define constructed_%s ", tp->tp_tpdef);
		if (actual->tp_fund == T_GENERIC) {
			fprintf(f, "constructed_%s\n", actual->tp_tpdef);
		}
		else {
			fprintf(f, "%d\n", is_constructed_type(actual) != 0);
		}
		fprintf(f, "#if constructed_%s\n", tp->tp_tpdef);
		fprintf(f, "#define ch%s(a, b) b\n", tp->tp_tpdef);
		fprintf(f, "#else\n");
		fprintf(f, "#define ch%s(a, b) a\n", tp->tp_tpdef);
		fprintf(f, "#endif\n");

		fprintf(f, "#define dynamic_%s ", tp->tp_tpdef);
		if (actual->tp_fund == T_GENERIC) {
			fprintf(f, "dynamic_%s\n", actual->tp_tpdef);
		}
		else {
			fprintf(f, "%d\n", (actual->tp_flags & T_DYNAMIC) != 0);
		}

		if (f == fc) {
			/* Marshalling macros */
			gen_marshall_macros(tp, actual);

			/* Freeing function */
			if (actual->tp_flags & T_DYNAMIC) {
				fprintf(f, "#define %s %s\n",
					tp->tp_freefunc,
					actual->tp_freefunc);
			}
			else {
				fprintf(f, "#define %s ((void *) 0)\n",
					tp->tp_freefunc);
			}

			/* initialization macro */
			fprintf(f, "#define init_%s", tp->tp_tpdef);
			if (actual->tp_fund == T_GENERIC) {
				fprintf(f, " init_%s\n", actual->tp_tpdef);
			}
			else if (actual->tp_flags & T_INIT_CODE) {
				fprintf(f, " %s\n", actual->tp_init);
			}
			else	fprintf(f, "(p, s)\n");
		}
	}

}

static void
gen_tp_uninst(f, tp)
	FILE	*f;
	t_type	*tp;
{
	/*	Produce #undefs for the generic type indicated by 'tp'.
	*/

	fprintf(f, "#undef %s\n", tp->tp_tpdef);
	fprintf(f, "#undef %s\n", tp->tp_descr);
	if (tp->tp_fund == T_GENERIC) {
		fprintf(f, "#undef constructed_%s\n", tp->tp_tpdef);
		fprintf(f, "#undef ch%s\n", tp->tp_tpdef);
		fprintf(f, "#undef dynamic_%s\n", tp->tp_tpdef);
		if (f == fc) {
			fprintf(f, "#undef %s\n", tp->tp_szfunc);
			fprintf(f, "#undef %s\n", tp->tp_mafunc);
			fprintf(f, "#undef %s\n", tp->tp_umfunc);
			fprintf(f, "#undef %s\n", tp->tp_freefunc);
			fprintf(f, "#undef init_%s\n", tp->tp_tpdef);
		}
	}
	if (f == fc) fprintf(f, "#undef %s\n", tp->tp_assignfunc);
}

static void
gen_str(s, gennam)
	t_string
		*s;
	int	gennam;
{
	/*	Generate an array structure for a string.
		If gennam is set, produce an initialization, otherwise
		only produce a display.
		Note that an extra character is produced in front of the
		character string to get the array indexing right.
	*/

	int	i;
	char	*p;

	if (gennam) fprintf(fc, "%s %s = ", string_type->tp_tpdef, s->s_name);
	fputs("{\"X", fc);
	for (i = s->s_length, p = s->s_str; i > 0; i--, p++) {
		switch(*p) {
		case '\\':
			putc('\\', fc); putc('\\', fc);
			break;
		case '"':
			putc('\\', fc); putc('"', fc);
			break;
		case '?':
			putc('\\', fc); putc('?', fc);
			break;
		default:
			putc(*p, fc);
		}
	}
	fprintf(fc, "\", %d, %d, ", 1, (int)(s->s_length));
	fprintf(fc, "{ { 1, %d } } }", (int)(s->s_length));
	if (gennam) fputs(";\n", fc);
}

void
gen_stringtab(df)
	p_def	df;
{
	/*	Produce array structures for all anonymous strings in
		the compilation unit 'df'.
	*/
	p_string
		s = df->mod_stringlist;

	CurrentScope = df->bod_scope;
	while (s) {
		if (s->s_name) {
			/* Not anonymous. */
			s = s->s_next;
			continue;
		}
		s->s_name = gen_dummyname("str_", 0);
		if (s->s_exported) {
			fprintf(fh, "extern %s %s;\n",
				string_type->tp_tpdef,
				s->s_name);
		}
		else fputs("static ", fc);
		gen_str(s, 1);
		s = s->s_next;
	}
}

void
gen_const(df)
	t_def	*df;
{
	/*	Generate structured constants.
	*/

	switch(df->df_kind) {
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) return;
		/* fall through */
	case D_OPERATION:
	case D_PROCESS:
		/* Generate local constants. */
		walkdefs(df->bod_scope->sc_def, D_CONST, gen_const);
		break;
	case D_CONST: {
		p_node	nd = df->con_const;

		if (df->df_flags & D_GENERICPAR) return;

		if ((nd->nd_class == Value && nd->nd_type == string_type)
		    || nd->nd_class == Aggr) {
			/* Either strings or aggregates. */
			if (df->df_flags & D_EXPORTED) {
				fprintf(fh, "extern %s %s;\n",
					df->df_type->tp_tpdef,
					df->df_name);
			}
			if (nd->nd_class == Value) {
				/* String. */
				if (! (df->df_flags & D_EXPORTED)) {
					fputs("static ", fc);
				}
				gen_str(nd->nd_str, 1);
				break;
			}
			/* Aggregate. */
			gen_const_aggr(nd, df->df_flags & D_EXPORTED, df->df_name);
		}
		}
	}
}

static int
memb_aggr(nd)
	p_node	nd;
{
	/* Wrapper around gen_member_aggr for visit_node. */
	if (nd->nd_symb == '[') {
		p_node l;
		node_walklist(nd->nd_memlist, l, nd) {
			gen_member_aggr(nd);
		}
		return 1;
	}
	return 0;
}

static int gen_separator;

static int
gen_memaggr(nd)
	p_node	nd;
{
	if (nd->nd_symb == '[') {
		p_node l;

		node_walklist(nd->nd_memlist, l, nd) {
			if (gen_separator) {
				fputs(",\n\t", fc);
			}
			if (nd->nd_class == Def) {
				assert(nd->nd_def->df_kind == D_CONST);
				nd = nd->nd_def->con_const;
			}
			switch(nd->nd_class) {
			case Value:
				if (nd->nd_type == string_type) {
					gen_str(nd->nd_str, 0);
				}
				else gen_value(fc, nd);
				break;
			case Aggr:
			case Row:
				gen_const_aggr(nd, 0, (char *) 0);
				break;
			default:
				assert(0);
			}
			gen_separator = 1;
		}
		return 1;
	}
	if (gen_separator) {
		fputs(",\n\t", fc);
	}
	switch(nd->nd_class) {
	case Value:
		if (nd->nd_type == string_type) {
			gen_str(nd->nd_str, 0);
		}
		else gen_value(fc, nd);
		break;
	case Aggr:
	case Row:
		gen_const_aggr(nd, 0, (char *) 0);
		break;
	default:
		assert(0);
	}
	gen_separator = 1;
	return 0;
}

static void
gen_member_aggr(nd)
	p_node	nd;
{
	/*	Generate member aggregates as far as needed to produce an
		aggregate for nd.
	*/

	p_node	l;
	t_type	*tp = nd->nd_type;
	p_node	nd1;

	switch(tp->tp_fund) {
	case T_ARRAY:
		if ((element_type_of(tp)->tp_flags & T_DYNAMIC) &&
		    element_type_of(tp) != string_type) {
			/* Need aggregates for the members. */
			visit_node(nd, Aggr|Row, memb_aggr, 0);
		}

		/* The array itself. */
		nd->nd_dummynam = gen_dummyname("dmy_", 0);
		fprintf(fc, "static %s %s[] = {\n\t",
			element_type_of(tp)->tp_tpdef,
			nd->nd_dummynam);
		gen_separator = 0;
		visit_ndlist(nd->nd_memlist, Row|Aggr|Value, gen_memaggr, 0);
		fputs("\n};\n", fc);
		break;
	case T_RECORD:
		if (tp->tp_flags & T_DYNAMIC) {
			node_walklist(nd->nd_memlist, l, nd1) {
				if ((nd1->nd_type->tp_flags & T_DYNAMIC) &&
				    nd1->nd_type != string_type) {
					gen_member_aggr(nd1);
				}
			}
		}
		break;
	case T_UNION:
		node_walklist(nd->nd_memlist, l, nd1) {
			switch(nd1->nd_type->tp_fund) {
			case T_ARRAY:
				if (nd1->nd_type == string_type) break;
				gen_member_aggr(nd1);
				break;
			case T_RECORD:
			case T_UNION:
				nd1->nd_dummynam = gen_dummyname("dmy_", 0);
				gen_member_aggr(nd1);
				fprintf(fc, "static %s %s = ",
					nd1->nd_type->tp_tpdef,
					nd1->nd_dummynam);
				gen_const_aggr(nd1, 0, (char *) 0);
				fputs(";\n", fc);
				break;
			}
		}
		break;
	default:
		assert(0);
	}
}

static void
gen_const_aggr(nd, exported, nm)
	p_node	nd;
	int	exported;
	char	*nm;
{
	/*	Generate a constant aggregate. Its name can be found in
		nm. If it is set, possible sub-aggregates are produced
		by calls to gen_member_aggr. If nm is not set, it is assumed
		that sub-aggregates have already been produced, and only
		a display is generated.
		Unions cause trouble, because ANSI C does not allow union
		initializations.
	*/

	p_node	l;
	t_type	*tp = nd->nd_type;
	p_node	nd1;
	int	first;

	assert(nd->nd_class & (Aggr|Row));

	if (exported) {
		assert(nm != 0);
		fprintf(fh, "extern %s %s;\n", tp->tp_tpdef, nm);
	}

	if (nm) {
		gen_member_aggr(nd);
		fprintf(fc, "%s%s %s = ",
			exported ? "" : "static ",
			tp->tp_tpdef,
			nm);
	}

	switch(tp->tp_fund) {
	case T_ARRAY: {
		int i;
		int sz = 1;
		int offset = 0;

		nd1 = nd;
		if (node_emptylist(nd1->nd_memlist)) {
			sz = 0;
		}
		else {
			for (i = 0; i < tp->arr_ndim; i++) {
				sz *= nd1->nd_nelements;
				offset *= nd1->nd_nelements;
				if (tp->arr_index(i)->tp_fund != T_ENUM) {
					offset++;
				}
				nd1 = node_getlistel(nd1->nd_memlist);
			}
		}
		fprintf(fc,
			"{ &%s[%d], %d, %d, ",
			nd->nd_dummynam,
			-offset,
			offset,
			sz);
		nd1 = nd;
		for (i = 0; i < tp->arr_ndim; i++) {
			int nel = nd1->nd_nelements;
			fprintf(fc, "%s{ %d, %d }",
				i == 0 ? "" : ",",
				tp->arr_index(i)->tp_fund != T_ENUM ? 1 : 0,
				nel);
			if (nel != 0) nd1 = node_getlistel(nd1->nd_memlist);
		}
		putc('}', fc);
		}
		break;
	case T_RECORD:
		first = 1;
		fputs("{\n\t", fc);
		node_walklist(nd->nd_memlist, l, nd1) {
			if (! first) fputs(",\n\t", fc);
			first = 0;
			switch(nd1->nd_class) {
			case Aggr:
			case Row:
				gen_const_aggr(nd1, 0, (char *) 0);
				break;
			case Def:
			case Value:
				gen_value(fc, nd1);
				break;
			default:
				assert(0);
			}
		}
		fputs("\n}", fc);
		break;
	case T_UNION:
		fputs("{ ", fc);
		gen_value(fc, node_getlistel(nd->nd_memlist));
		fputs(", 1 }", fc);
		break;
	default:
		assert(0);
	}
	if (nm) fputs(";\n", fc);
}

void
gen_init_macro(tp, f)
	t_type	*tp;
	FILE	*f;
{
	/*	Produce an initialization macro for type 'tp', on file 'f'.
		Unions and arrays have two initialization macros because
		they can have bounds and tag expressions.
	*/

	t_def	*df;

	if (tp->tp_flags & T_INIT_DONE) {
		/* Initialization macro already generated. */
		return;
	}

	if (! (tp->tp_flags & T_INIT_CODE)) {
		/* Initialization macro not needed, because this type has no
		   initialization.
		*/
		return;
	}

	if (tp->tp_def &&
	    (tp->tp_def->df_flags & D_EXPORTED) &&
	    tp->tp_def->df_scope->sc_definedby != CurrDef &&
	    (tp->tp_fund == T_OBJECT ||
	     !(tp->tp_def->df_scope->sc_definedby->df_flags & D_GENERIC))) {
		/* In this case, the initialization macro is produced
		   elsewhere (in the corresponding include file).
		   However, if a generic module/object exports a type,
		   an initialization macro must still be produced. See comment
		   in new_init_macros().
		*/
		return;
	}

	tp->tp_flags |= T_INIT_DONE;

	if (CurrDef->df_flags & D_GENERIC) f = fc;

	switch(tp->tp_fund) {
	case T_RECORD:
		/* First the record fields ... */
		df = tp->rec_scope->sc_def;
		while (df) {
			if ((df->df_kind & (D_FIELD|D_OFIELD))
			    && (df->df_type->tp_flags & T_INIT_CODE)) {
				gen_init_macro(df->df_type, f);
			}
			df = df->df_nextinscope;
		}

		/* and then the record itself. */
		fprintf(f, "#undef %s\n", tp->tp_init);
		fprintf(f, "#define %s(p, s) { \\\n", tp->tp_init);
		for (df = tp->rec_scope->sc_def; df; df = df->df_nextinscope) {
			if (! (df->df_kind & (D_FIELD|D_OFIELD))) {
				continue;
			}
			if (df->df_flags & (D_LOWER_BOUND|D_UPPER_BOUND)) continue;
			if (df->fld_bat) {
				char *dfnam = mk_str("&((p)->", df->df_name, ")", (char *) 0);
				gen_init_with_bat(dfnam, df->df_type, f, df->fld_bat, "s", " \\\n\t\t");
				fputs(" \\\n", f);
				free(dfnam);
			}
			else if (df->df_type->tp_flags & T_INIT_CODE) {
				fprintf(f, "\t\t%s(&((p)->%s), s); \\\n",
					df->df_type->tp_init,
					df->df_name);
			}
		}
		fputs("\t}\n", f);
		break;

	case T_GRAPH:
		gen_init_macro(tp->gra_node, f);
		if (tp->gra_root->tp_flags & T_INIT_CODE) {
			gen_init_macro(tp->gra_root, f);

			fprintf(f, "#undef %s\n", tp->tp_init);
			fprintf(f, "#define %s(p, s) { \\\n", tp->tp_init);
			fprintf(f, "\t\t%s(&((p)->g_root), s); \\\n",
				tp->gra_root->tp_init);
			fputs("\t\tg_initialize(p, s); \\\n\t}\n", f);
		}
		break;

	case T_UNION:
		df = tp->rec_scope->sc_def;
		while (df) {
			if (df->df_type->tp_flags & T_INIT_CODE) {
				gen_init_macro(df->df_type, f);
			}
			df = df->df_nextinscope;
		}
		df = tp->rec_scope->sc_def;
		fprintf(f, "#undef %s\n", tp->tp_batinit);
		fprintf(f, "#define %s(p, bat, fn, ln, s) { \\\n", tp->tp_batinit);
		fprintf(f, "\t\t(p)->u_init = 1; (p)->%s = (bat); \\\n",
			df->df_name);
		fputs("\t\tswitch(bat) { \\\n", f);
		while ((df = df->df_nextinscope)) {
			fprintf(f, "\t\tcase %ld: \\\n", df->fld_tagvalue->nd_int);
			if (df->df_type->tp_flags & T_INIT_CODE) {
				if (df->fld_bat) {
					char *dfnam = mk_str("&((p)->u_el.", df->df_name, ")", (char *) 0);
					char *snam = mk_str("s", " \".", df->df_idf->id_text, "\"", (char *) 0);
					gen_init_with_bat(dfnam, df->df_type, f, df->fld_bat, snam, " \\\n\t\t\t");
					fputs(" \\\n", f);
					free(dfnam);
					free(snam);
				}
				else {
					fprintf(f, "\t\t\t%s(&((p)->u_el.%s), s \".%s\"); \\\n",
						df->df_type->tp_init,
						df->df_name,
						df->df_idf->id_text);
				}
			}
			fputs("\t\t\tbreak; \\\n", f);
		}
		fputs("\t\tdefault: m_trap(UNION_ERROR, fn, ln); break; \\\n\t\t} \\\n\t}\n", f);

		if (tp->rec_init) {
			fprintf(f, "#undef %s\n", tp->tp_init);
			fprintf(f, "#define %s(p, s) { \\\n", tp->tp_init);
			fprintf(f, "\t\t%s(p, ", tp->tp_batinit);
			gen_expr(f, tp->rec_init);
			fprintf(f, ", %s, %d", CurrDef->mod_fn, tp->rec_init->nd_pos.pos_lineno);
			fputs(", s); \\\n\t}\n", f);
		}
		break;

	case T_ARRAY: {
		t_type *eltype = element_type_of(tp);
		int i;

		gen_init_macro(eltype, f);
		fprintf(f, "#undef %s\n", tp->tp_batinit);
		fprintf(f, "#define %s(p, nestinit", tp->tp_batinit);
		for (i = 0; i < tp->arr_ndim; i++) {
			fprintf(f, ", lb%d, ub%d", i, i);
		}
		fputs(", s) { \\\n", f);
		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    fprintf(f, "\t\ta_allocate(p, %d, sizeof(%s)",
			tp->arr_ndim,
			eltype->tp_tpdef);
		    for (i = 0; i < tp->arr_ndim; i++) {
			fprintf(f, ", lb%d, ub%d", i, i);
		    }
		    fputs("); \\\n", f);
		}
		if (eltype->tp_flags & T_INIT_CODE) {
			char *a = gen_dummyname("a_", 1);

			fputs("\t\tif (nestinit) { \\\n", f);
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fprintf(f, "\t\t\t%s *%s = &((%s *)((p)->a_data))[(p)->a_offset]; \\\n",
				eltype->tp_tpdef, a, eltype->tp_tpdef);
			    fputs("\t\t\tunsigned i_ = (p)->a_sz; \\\n", f);
			}
			else {
			    fprintf(f, "\t\t\t%s *%s = (p)->a_data; \\\n",
				eltype->tp_tpdef, a);
			    fprintf(f, "\t\t\tunsigned i_ = %d; \\\n",
				tp->arr_size);
			}
			fputs("\t\t\twhile (i_-- != 0) { \\\n", f);
			if (eltype->tp_flags & T_HASOBJ) {
				fprintf(f, "\t\t\t\tchar buf[64]; \\\n");
				if (! (tp->tp_flags & T_CONSTBNDS)) {
				     fprintf(f, "\t\t\t\tsprintf(buf, \"%%.48s[%%d]\", s, (p)->a_offset+(p)->a_sz - i_ - 1); \\\n");
				}
				else {
				     fprintf(f, "\t\t\t\tsprintf(buf, \"%%.48s[%%d]\", s, %d - i_); \\\n", tp->arr_size - 1);
				}
				fprintf(f, "\t\t\t\t%s(%s, buf); %s++; \\\n", eltype->tp_init, a, a);
			}
			else {
				fprintf(f, "\t\t\t\t%s(%s, s \"[?]\"); %s++; \\\n", eltype->tp_init, a, a);
			}
			fputs("\t\t\t} \\\n", f);
			fputs("\t\t} \\\n", f);
			free(a);
		}
		fputs("\t}\n", f);
		if (tp->arr_bounds(0)) {
			fprintf(f, "#undef %s\n", tp->tp_init);
			fprintf(f, "#define %s(p, s) { \\\n", tp->tp_init);
			fprintf(f, "\t\t%s(p, 1", tp->tp_batinit);
			for (i = 0; i < tp->arr_ndim; i++) {
				fputs(", ", f);
				gen_expr(f, tp->arr_bounds(i)->nd_left);
				fputs(", ", f);
				gen_expr(f, tp->arr_bounds(i)->nd_right);
			}
			fputs(", s); \\\n", f);
			fputs("\t}\n", f);
		}
		}
		break;
	}
}

void
gen_init_with_bat(dfnam, tp, f, bat, snam, nl)
	char	*snam, *dfnam, *nl;
	t_type	*tp;
	FILE	*f;
	p_node	bat;
{
	/*	Generate an initialization with a bounds-and-tag expression.
	*/

	p_node	nd = node_getlistel(bat);
	int	i;
	p_node	n;

	bat = node_nextlistel(bat);
	switch(tp->tp_fund) {
	case T_ARRAY:
		fprintf(f, "%s(%s, ", tp->tp_batinit, dfnam);
		/* Find out if there are bounds-and-tag expressions for the
		   elements. In that case, initialize elements explicitly.
		*/
		fprintf(f, "%d", node_emptylist(bat) ? 1 : 0);

		/* Now, the array bounds. */
		for (i = 0; i < tp->arr_ndim; i++) {
			if (nd->nd_symb != UPTO) {
				assert(i < tp->arr_ndim-1);
				n = nd->nd_left;
				nd = nd->nd_right;
			}
			else {
				assert(i == tp->arr_ndim-1);
				n = nd;
			}
			fputs(", ", f);
			gen_expr(f, n->nd_left);
			fputs(", ", f);
			gen_expr(f, n->nd_right);
		}
		fprintf(f, ", %s);", snam);

		if (! node_emptylist(bat)) {
			t_type *eltype = element_type_of(tp);
			char *a = gen_dummyname("a_", 1);
			char *newnl = mk_str(nl, "\t\t", (char *) 0);

			fprintf(f, "%s", nl);
			if (tp->tp_flags & T_CONSTBNDS) {
			    fprintf(f, "{\t%s *%s = (%s)->a_data;%s",
				eltype->tp_tpdef,
				a, dfnam, nl);
			    fprintf(f, "\tunsigned i_ = %d;%s",
				tp->arr_size, nl);
			}
			else {
			    fprintf(f, "{\t%s *%s = &((%s *)((%s)->a_data))[(%s)->a_offset];%s",
				eltype->tp_tpdef,
				a, eltype->tp_tpdef, dfnam,
				dfnam, nl);
			    fprintf(f, "\tunsigned i_ = (%s)->a_sz;%s",
				dfnam, nl);
			}
			fprintf(f, "\twhile (i_-- != 0) { %s\t    ", nl);
			if (eltype->tp_flags & T_HASOBJ) {
				fprintf(f, "char buf[64]; %s\t    ", nl);
				if (!(tp->tp_flags & T_CONSTBNDS)) {
				    fprintf(f, "sprintf(buf, \"%%.48s[%%d]\", %s, (%s)->a_offset+(%s)->a_sz - 1 - i_); %s\t    ", snam, dfnam, dfnam, nl);
				}
				else {
				    fprintf(f, "sprintf(buf, \"%%.48s[%%d]\", %s, %d - i_); %s\t    ", snam, tp->arr_size - 1, nl);
				}
				gen_init_with_bat(a, eltype, f, bat, "buf", newnl);
			}
			else {
				gen_init_with_bat(a, eltype, f, bat, "(char *) 0", newnl);
			}
			fprintf(f, "%s\t    %s++;%s\t}%s    }", nl, a, nl, nl);
			free(a);
			free(newnl);
		}
		break;
	case T_UNION:
		assert(node_emptylist(bat));
		fprintf(f, "%s(%s, ", tp->tp_batinit, dfnam);
		assert(nd->nd_class != Link);
		gen_expr(f, nd);
		fprintf(f, ", %s, %d", CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		fprintf(f, ", %s);", snam);
		break;
	case T_OBJECT:
		fprintf(f, "%s(%s", tp->tp_batinit, dfnam);

		for (i = 0; i < tp->arr_ndim; i++) {
			if (nd->nd_symb != UPTO) {
				assert(i < tp->arr_ndim-1);
				n = nd->nd_left;
				nd = nd->nd_right;
			}
			else {
				assert(i == tp->arr_ndim-1);
				n = nd;
			}
			fputs(", ", f);
			gen_expr(f, n->nd_left);
			fputs(", ", f);
			gen_expr(f, n->nd_right);
		}
		fputs(");", f);
		break;

	default:
		crash("gen_init_with_bat");
	}
}

int
base_char_of(tp)
	t_type	*tp;
{
	switch(tp->tp_fund) {
	case T_NODENAME:
		return 'n';
	case T_ARRAY:
		return 'a';
	case T_SET:
		return 's';
	case T_BAG:
		return 'b';
	case T_GRAPH:
		return 'g';
	case T_UNION:
		return 'u';
	case T_RECORD:
		return 'r';
	case T_OBJECT:
		if (tp->tp_flags & T_PART_OBJ) return 'p';
		return 'o';
	}
	crash("base_char_of");
	return 'x';
}

static int something_to_do;

void
gen_modinit(moddef)
	p_def	moddef;
{
	/*	 Generate initialization routine.
	*/
	p_def	df;
	t_dflst	dl;

	def_walklist(moddef->mod_funcaddrs, dl, df) {
		if (! (df->df_flags & D_EXTRAPARAM)) {
			/* Generate stub for function with extra parameter. */
			t_type *result_tp = result_type_of(df->df_type);
			t_dflst	l;
			t_def	*d;
			int	first = 1;

			df->prc_addrname = gen_name("af_", df->df_idf->id_text, 0);
			gen_prototype(fc, df, df->prc_addrname, (t_type *) 0);
			fputs("{\n    ", fc);
			if (result_tp) {
				if (! is_constructed_type(result_tp)) {
					fputs("return ", fc);
				}
				else if (result_tp->tp_fund == T_GENERIC) {
					fprintf(fc, "ch%s(return , NOTHING) ",
						result_tp->tp_tpdef);
				}
			}
			fprintf(fc, "%s(", df->df_name);
			def_walklist(df->df_type->prc_params, l, d) {
				if (! first) fputs(", ", fc);
				first = 0;
				fprintf(fc, "%c%s",
					(is_out_param(d)
					 || (d->df_flags & D_COPY)) ? 'o' : ' ',
					d->df_name);
			}
			if (result_tp) {
				if (result_tp->tp_fund == T_GENERIC) {
					fprintf(fc, "ch%s(NOTHING, %s v_result)",
						result_tp->tp_tpdef,
						first ? "" : "COMMA");
				}
				else if (is_constructed_type(result_tp)) {
					if (first) fputs(", ", fc);
					fputs("v__result", fc);
				}
			}
			fputs(");\n}\n", fc);
		}
		else df->prc_addrname = df->df_name;
	}

	if (moddef->df_kind == D_OBJECT &&
	    (moddef->df_flags & D_PARTITIONED)) {
		for (df = moddef->bod_scope->sc_def;
		     df;
		     df = df->df_nextinscope) {
			if (df->df_kind == D_OPERATION &&
			    (df->df_flags & D_PARALLEL)) {
				gen_deps_func(df);
			}
		}
	}

	fprintf(fc, "void (%s)(void) {\n", moddef->mod_name);
	if (moddef->df_flags & D_PARTITIONED) {
		fputs("\tpo_operation_p opid;\n", fc);
	}
	fputs("\tstatic int done = 0;\n\n\tif (done) return;\n\tdone = 1;\n", fc);

	something_to_do = options['i'];
	/* Functions of which address is taken: */
	def_walklist(moddef->mod_funcaddrs, dl, df) {
		something_to_do = 1;
		fprintf(fc, "\t%s[%d] = m_ptrregister((void *)%s);\n",
			moddef->mod_funcaddrname,
			df->prc_funcno-1,
			df->prc_addrname);
	}

	/* Register process descriptors. */
	for (df = moddef->bod_scope->sc_def; df; df = df->df_nextinscope) {
		if (df->df_kind == D_PROCESS) {
			something_to_do = 1;
			fprintf(fc, "\t%s.prc_registration = m_ptrregister((void *)&%s);\n",
				df->prc_name,
				df->prc_name);
		}
	}

	/* Register object descriptor. */
	if (moddef->df_kind == D_OBJECT) {
		int	count = 0;
		char	*s = gen_trace_name(moddef);

		something_to_do = 1;
		fprintf(fc, "\ttd_registration(&%s) = m_ptrregister((void *)&%s);\n",
			moddef->df_type->tp_descr,
			moddef->df_type->tp_descr);
		/* Also call object descriptor registration for tracing.
		   The second parameter of m_objdescr_reg is the number
		   of operations for this object type.
		   The third parameter is the object type name.
		*/
		for (df = record_type_of(moddef->df_type)->rec_scope->sc_def;
		     df;
		     df = df->df_nextinscope) {
			if (df->df_kind == D_OPERATION) count++;
		}
		fprintf(fc, "\tm_objdescr_reg(&%s, %d, %s);\n",
			moddef->df_type->tp_descr, count, s);

		if (moddef->df_flags & D_PARTITIONED) {
			fprintf(fc, "\t%s = new_po(%s, %d, sizeof(%s), &%s);\n",
				moddef->df_name,
				s,
				moddef->df_type->arr_ndim,
				record_type_of(moddef->df_type)->tp_tpdef,
				record_type_of(moddef->df_type)->tp_descr);
		}

		def_walklist(moddef->mod_reductionfuncs, dl, df) {
			fprintf(fc, "\tregister_reduction_function((reduction_function_p)%s);\n", df->df_name);
		}

		if (moddef->df_flags & D_PARTITIONED) {
			for (df = moddef->bod_scope->sc_def;
			     df;
			     df = df->df_nextinscope) {
				if (df->df_kind == D_OPERATION && df->prc_funcno >= 2) {
					char *n = gen_trace_name(df);
					char *s2 = gen_name("poa_", type_name(df->df_type), 0);
					fprintf(fc, "\topid = new_po_operation(%s, %s, ",
						moddef->df_name,
						n);
					free(n);
					fprintf(fc, "(po_opcode) %s, ",
						df->opr_namew);
					fputs((df->df_flags & D_PARALLEL) ? "Par" : "Seq", fc);
					fputs(df->df_flags & D_HASWRITES ? "Write, " : "Read, ", fc);
					fprintf(fc, "%d, %s, ",
						df->df_type->prc_nparams +
						 (result_type_of(df->df_type) ? 1 : 0),
						s2);
					free(s2);
					if ((df->df_flags & D_PARALLEL) &&
					    ((df->df_flags & D_SIMPLE_DEPENDENCIES) ||
					     !(df->df_flags & D_HAS_COMPLEX_OFLDSEL))) {
						char *s = gen_name("pdg_",
							df->df_idf->id_text,
							0);

						fprintf(fc, "%s);\n", s);
						free(s);
					}
					else fputs("NULL);\n", fc);
					fprintf(fc, "\topid->opdescr = &(td_operations(&%s))[%d];\n",
						moddef->df_type->tp_descr,
						df->prc_funcno);
					if (df->opr_dependencies &&
					    ! (df->df_flags & D_SIMPLE_DEPENDENCIES)) {
						char *s = gen_name("deps__",
							df->df_idf->id_text,
							0);
					  fprintf(fc, "\topid->dyn_pdg = %s;\n",
						s);
					  free(s);
					}
				}
			}

			fprintf(fc, "\t(void) new_po_operation(%s, %s, (po_opcode) %s, SeqInit, 0, 0, NULL);\n",
				moddef->df_name,
				s,
				moddef->mod_initname);
			fprintf(fc, "\tsynch_handlers();\n");
		}
		free(s);
	}

	/* Imported modules: call initialization routine: */
	def_walklist(moddef->mod_imports, dl, df) {
		if (df != moddef) {
			something_to_do = 1;
			fprintf(fc, "\t%s();\n", df->mod_name);
		}
	}
	if ((moddef->df_flags & D_INOUT_NEEDED) &&
	    ! (moddef->df_flags & D_INOUT_DONE)) {
		something_to_do = 1;
		fputs("\tini_InOut__InOut();\n", fc);
	}

	/* Union constants initialization. */
	walkdefs(moddef->bod_scope->sc_def,
		D_FUNCTION|D_PROCESS|D_OPERATION|D_CONST,
		gen_unioninit);

	if (moddef->df_flags & D_DATA) {
		something_to_do = 1;
		fprintf(fc, "\t%s();\n", moddef->df_name);
	}

	fputs("}\n", fc);
	if (something_to_do || (moddef->df_flags & D_GENERIC)) {
		fprintf(fh, "void %s(void);\n", moddef->mod_name);
	}
	else {
		fprintf(fh, "#define %s()\t/* nothing */\n", moddef->mod_name);
	}
}

static void
gen_unioninit(df)
	t_def	*df;
{
	p_node	nd;

	switch(df->df_kind) {
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) return;
		/* Fall through */
	case D_PROCESS:
	case D_OPERATION:
		/* Generate union initializations for local constants. */
		walkdefs(df->bod_scope->sc_def, D_CONST, gen_unioninit);
		return;
	case D_CONST:
		if (df->df_flags & D_GENERICPAR) return;
	}
	nd = df->con_const;
	if (! (nd->nd_type->tp_flags & T_UNION_INIT)) return;
	something_to_do = 1;
	gen_initu(nd, df->df_name);
}

static void
gen_initu(nd, s)
	p_node	nd;
	char	*s;
{
	p_node	l;
	t_type	*tp = nd->nd_type;
	int	i;
	p_node	n;
	t_def	*df;
	char	*p;

	if (nd->nd_class == Def) {
		assert(nd->nd_def->df_kind == D_CONST);
		nd = nd->nd_def->con_const;
	}
	switch(tp->tp_fund) {
	case T_ARRAY:
		i = 0;
		p = Malloc((unsigned)(strlen(nd->nd_dummynam)+20));
		node_walklist(nd->nd_memlist, l, n) {
			sprintf(p, "%s[%d]", nd->nd_dummynam, i);
			gen_initu(n, p);
			i++;
		}
		free(p);
		break;
	case T_RECORD:
		df = tp->rec_scope->sc_def;
		node_walklist(nd->nd_memlist, l, n) {
			if (n->nd_type->tp_flags & T_UNION_INIT) {
				p = Malloc((unsigned)(strlen(s)+ strlen(df->df_name)+2));
				sprintf(p, "%s.%s", s, df->df_name);
				gen_initu(n, p);
				free(p);
			}
			df = df->df_nextinscope;
		}
		break;
	case T_UNION:
		n = node_getlistel(node_nextlistel(nd->nd_memlist));
		df = get_union_variant(tp, node_getlistel(nd->nd_memlist));
		assert(df);
		p = Malloc((unsigned)(strlen(s)+strlen(df->df_name)+10));
		sprintf(p, "%s.u_el.%s", s, df->df_name);
		if (n->nd_type->tp_flags & T_UNION_INIT) {
			assert(n->nd_dummynam);
			gen_initu(n, n->nd_dummynam);
		}
		fprintf(fc, "\t%s = ", p);
		switch(n->nd_type->tp_fund) {
		case T_ARRAY:
			if (n->nd_type == string_type) {
				fprintf(fc, "%s;\n", n->nd_str->s_name);
				break;
			}
			/* fall through */
		case T_RECORD:
		case T_UNION:
			assert(n->nd_dummynam);
			fprintf(fc, "%s;\n", n->nd_dummynam);
			break;
		default:
			gen_value(fc, n);
			fputs(";\n", fc);
			break;
		}
		free(p);
		break;
	}
}

static void
gen_type_names(tp, df)
	t_type	*tp;
	t_def	*df;
{
	/*	Produce names for typedefs and type descriptors.
	*/
	char	*s;
	int	fix = 0;
	t_scope	*osc = CurrentScope;

	if (tp->tp_tpdef) return;

	if (! df) df = tp->tp_def;
	if (! tp->tp_def) tp->tp_def = df;

	if (df && df->df_scope != CurrentScope) return;
	fix = df && (df->df_kind&(D_CONST|D_TYPE|D_FUNCTION|D_OBJECT))
	       && (df->df_flags & D_GENERICPAR);

	if (tp->tp_fund == T_OBJECT) {
		if (df && (df->df_flags & D_INSTANTIATION)) {
		    if (! (df->df_type->tp_tpdef)) {
			char *buf = mk_str(df->df_idf->id_text,
					   "__",
					   df->bod_scope->sc_name,
					   (char *) 0);
			tp->tp_descr = gen_name("td_", buf, fix);
			tp->tp_szfunc = gen_name("sz_", buf, fix);
			tp->tp_mafunc = gen_name("ma_", buf, fix);
			tp->tp_umfunc = gen_name("um_", buf, fix);
			tp->tp_assignfunc = gen_name("ass_", buf, fix);
			tp->tp_freefunc = gen_name("free_", buf, fix);
			tp->tp_tpdef = gen_name("t_", buf, fix);
			tp->tp_init = gen_name("init_t_", buf, fix);
			tp->tp_batinit = gen_name("batinit_", buf, fix);
			free(buf);
		    }
		    return;
		}
	}
	s = type_name(tp);

	if (df && df->df_kind == D_TYPE && df != tp->tp_def) {
		CurrentScope = tp->tp_def->df_scope;
		df = tp->tp_def;
	}
	if (CurrentScope == GlobalScope) {
		assert(df);
		CurrentScope = df->bod_scope;
	}
	tp->tp_descr = gen_name("td_", s, fix);
	tp->tp_tpdef = gen_name("t_", s, fix);
	if (df && df->df_kind == D_MODULE && (df->df_flags & D_DATA)) {
		tp->tp_init = gen_name("init_t_", s, fix);
	}
	tp->tp_szfunc = gen_name("sz_", s, fix);
	tp->tp_mafunc = gen_name("ma_", s, fix);
	tp->tp_umfunc = gen_name("um_", s, fix);
	if (! (tp->tp_flags & T_NOEQ) && ! tp->tp_comparefunc) {
		tp->tp_comparefunc = gen_name("cmp_", s, fix);
	}
	if ((tp->tp_flags & T_DYNAMIC) && ! tp->tp_freefunc) {
		tp->tp_freefunc = gen_name("free_", s, fix);
	}
	if (! tp->tp_assignfunc) {
		tp->tp_assignfunc = gen_name("ass_", s, fix);
	}

	gen_init_name(tp, s);

	switch(tp->tp_fund) {
	case T_RECORD:
	case T_UNION:
		df = tp->rec_scope->sc_def;

		while (df) {
		    if (df->df_kind & (D_FIELD|D_OFIELD|D_UFIELD|D_OPERATION)) {
			gen_type_names(df->df_type, (t_def *) 0);
		    }
		    df = df->df_nextinscope;
		}
		break;
	case T_SET:
	case T_ARRAY:
	case T_BAG:
		gen_type_names(element_type_of(tp), (t_def *) 0);
		break;
	case T_GRAPH:
		gen_type_names(tp->gra_root, (t_def *) 0);
		gen_type_names(tp->gra_node, (t_def *) 0);
		gen_type_names(tp->gra_name, (t_def *) 0);
		break;
	case T_FUNCTION: {
		t_dflst l;

		if (tp->tp_flags & T_FUNCADDR) {
			tp->prc_ftp = gen_name("tdf_", s, fix);
		}
		def_walklist(tp->prc_params, l, df) {
			gen_type_names(df->df_type, (t_def *) 0);
		}
		if (result_type_of(tp)) {
			gen_type_names(result_type_of(tp), (t_def *) 0);
		}
		}
		break;
	case T_NODENAME:
		if (named_type_of(tp)) {
			gen_type_names(named_type_of(tp), (t_def *) 0);
		}
		break;
	case T_OBJECT:
		if (record_type_of(tp)) {
			gen_type_names(record_type_of(tp), (t_def *) 0);
		}
	}
	CurrentScope = osc;
}

char *
type_name(tp)
	t_type	*tp;
{
	/*	Find the name of the type indicated by 'tp'.
		If it does not have a name, produce an anonymous one.
	*/
	static char
		buf[20];
	int	i;

	if (tp->tp_def) return tp->tp_def->df_idf->id_text;
	i = tp->tp_anon;
	if (! i) {
		i = ++(ProcScope->sc_anoncount);
		tp->tp_anon = i;
	}
	(void) sprintf(buf, "%d", i);
	return &buf[0];
}

static void
gen_init_name(tp, s)
	t_type	*tp;
	char	*s;
{
	/*	Produce a name for the initialization routine/macro for
		type tp, using the string s. When translating a generic
		unit, the name must be fixed, as it is used as a macro
		name.
	*/

	int	generic = CurrDef->df_flags & D_GENERIC;

	if (tp->tp_init && tp->tp_init[1] != '_') {
		/* If tp->tp_init[1] == '_', the name is not Malloced. */
		free(tp->tp_init);
	}
	switch(tp->tp_fund) {
	case T_GENERIC:
		tp->tp_init = gen_name("init_t_", s, generic);
		break;
	case T_RECORD:
		if (tp->tp_flags & T_INIT_CODE) {
			tp->tp_init = gen_name("init_t_", s, generic);
		}
		break;
	case T_UNION:
		if (tp->tp_batinit) free(tp->tp_batinit);
		tp->tp_batinit = gen_name("batinit_", s, generic);
		if (tp->rec_init) {
			tp->tp_init = gen_name("init_t_", s, generic);
		}
		else tp->tp_init = "u_initialize";
		break;
	case T_ARRAY:
		if (tp->tp_batinit) free(tp->tp_batinit);
		tp->tp_batinit = gen_name("batinit_", s, generic);
		if (tp->arr_bounds(0)) {
			tp->tp_init = gen_name("init_t_", s, generic);
		}
		else tp->tp_init = "a_initialize";
		break;
	case T_OBJECT:
		tp->tp_init = gen_name("init_t_", s, 0);
		if (tp->tp_batinit) free(tp->tp_batinit);
		tp->tp_batinit = gen_name("batinit_", s, 0);
		break;
	case T_GRAPH:
		if (tp->gra_root->tp_flags & T_INIT_CODE) {
			tp->tp_init = gen_name("init_t_", s, generic);
		}
		else tp->tp_init = "g_initialize";
		break;
	case T_SET:
		tp->tp_init = "s_initialize";
		break;
	case T_BAG:
		tp->tp_init = "b_initialize";
		break;
	case T_NODENAME:
		tp->tp_init = "n_initialize";
		break;
	}
}
