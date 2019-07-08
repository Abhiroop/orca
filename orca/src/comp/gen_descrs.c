/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: gen_descrs.c,v 1.45 1998/05/13 11:48:35 ceriel Exp $ */

/* Descriptor generation routines. */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <alloc.h>
#include <assert.h>

#include "gen_descrs.h"
#include "scope.h"
#include "node.h"
#include "type.h"
#include "main.h"
#include "generate.h"
#include "gen_code.h"
#include "error.h"
#include "const.h"
#include "closure.h"
#include "options.h"
#include "marshall.h"
#include "tpfuncs.h"

_PROTOTYPE(static void gen_defdescr, (FILE *, t_def *, t_type *));
_PROTOTYPE(static void gen_union_descr, (t_type *, int));
_PROTOTYPE(static void gen_record_descr, (t_type *, int));
_PROTOTYPE(static void gen_object_descr, (t_type *, int));
_PROTOTYPE(static void gen_func_descr, (t_type *, int));
_PROTOTYPE(static void gen_nodenm_descr, (t_type *, int));
_PROTOTYPE(static void gen_bag_descr, (t_type *, int));
_PROTOTYPE(static void gen_set_descr, (t_type *, int));
_PROTOTYPE(static void gen_array_descr, (t_type *, int));
_PROTOTYPE(static void gen_graph_descr, (t_type *, int));
_PROTOTYPE(static void gen_enum_descr, (t_type *, int));
_PROTOTYPE(static void gen_proc_descr, (t_def *));

void
generate_descrs(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_PROCESS:
		/* For processes, generate a process descriptor. */
		if (df->df_scope->sc_definedby != CurrDef) return;
		gen_defdescr(fc, (t_def *) 0, df->df_type);
		gen_proc_descr(df);
		break;
	case D_VARIABLE:
		/* Variables may have an anonymous type. */
		if (df->df_flags & D_SELF) break;
		if (! df->df_type->tp_def) {
			gen_defdescr(df->df_flags & D_DATA ? fh : fc,
				     (t_def *) 0,
				     df->df_type);
		}
		break;
	case D_OBJECT:
	case D_TYPE: {
		/* For object types and types, generate descriptors and
		   names if they belong to the compilation unit. If they
		   don't, we still need names of typedefs and type descriptors,
		   so we produce them anyway, but only internally.
		*/
		t_type	*tp = df->df_type;
		int in_def = df->df_flags & D_EXPORTED;

		if (df->df_kind == D_TYPE
		    || !(df->df_flags & D_INSTANTIATION)) {
			if (!in_def
			    || df->df_scope->sc_definedby == CurrDef) {
				gen_defdescr(in_def ? fh : fc, df, tp);
			}
		}
		if (df->df_kind == D_TYPE) {
			return;
		}
		/* fall through */
	}
	case D_MODULE:
		if ((df->df_flags & D_INSTANTIATION)
		    && (df->df_scope->sc_definedby == CurrDef)) {
			gen_instantiation(df);
		}
		break;
	}
}

void
gen_localdescrs(df)
	t_def	*df;
{
	/*	For processes, procedures and operations, generate
		descriptors for their local types.
	*/
	assert(df->df_kind & (D_FUNCTION|D_PROCESS|D_OPERATION));
	walkdefs(df->bod_scope->sc_def, D_TYPE|D_VARIABLE, generate_descrs);
}

static void
gen_proc_descr(df)
	t_def	*df;
{
	char	*shargsname;
	char	*prcfnc;
	char	*s;

	shargsname = gen_shargs(df);
	prcfnc = gen_process_wrapper(df);

	gen_marshall(df);

	if (df->df_flags & D_EXPORTED) {
		fprintf(fh, "extern prc_dscr %s;\n", df->prc_name);
	}
	s = gen_trace_name(df);
	fprintf(fc, "%sprc_dscr %s = { %s, &%s, 0, %s, %s, %s, %s, %s};\n",
		df->df_flags & D_EXPORTED ? "" : "static ",
		df->prc_name,
		prcfnc,
		df->df_type->tp_descr,
		shargsname,
		s,
		df->bod_marshallnames[0],
		df->bod_marshallnames[1],
		df->bod_marshallnames[2]);
	free(shargsname);
	free(prcfnc);
	free(s);
}

static void
gen_defdescr(f, df, tp)
	FILE	*f;
	t_def	*df;
	t_type	*tp;
{
	/* Produce a typedef and a type descriptor for type "tp", using the
	   name from "df". If "df" is a null pointer and tp->tp_def is not
	   null, then that is used. Otherwise, the type is anonymous and
	   an internal name is generated.
	   The typedef is produced on file descriptor "f", unless it concerns
	   an object. object typedefs are always produced on "fh" so that
	   other modules can refer to it.  The descriptor itself is always
	   produced on "fc", because it is an initialized data structure.
	   if "f" is "fh" or "tp" refers to an object, the descriptor is
	   made externally visible.
	*/
	t_def	*deflist;
	int	ext_descr = (f == fh || tp->tp_fund == T_OBJECT);

	if (tp->tp_flags & T_DECL_DONE) return;		/* already done */
	if (df
	    && df->df_kind != D_OBJECT
	    && tp->tp_def
	    && df != tp->tp_def) return;
	if (! df) df = tp->tp_def;
	if (df && (df->df_flags & D_EXPORTED)) {
		ext_descr = 1;
		f = fh;
		if (df->df_scope->sc_definedby != CurrDef) {
			/* Different compilation unit */
			return;
		}
	}

	if (tp->tp_fund == T_OBJECT && df && (df->df_flags & D_INSTANTIATION)) {
		/* In this case, "tp" refers to an instantiation of an object.
		*/
		generate_descrs(df);
		return;
	}

	tp->tp_flags |= T_DECL_DONE;

	/* For a generic parameter, no typedef or type descriptor is produced
	*/
	switch(tp->tp_fund) {
	case T_GENERIC:
	case T_NUMERIC:
	case T_SCALAR:
		return;
	}

	if (ext_descr) {
		fprintf(fh, "extern tp_dscr %s;\n", tp->tp_descr);
	}
	else	fprintf(fc, "static tp_dscr %s;\n", tp->tp_descr);

	/* produce typedefs for anonymous types */
	switch(tp->tp_fund) {
	case T_RECORD:
	case T_UNION:
		/* produce typedefs and desciptors for members */
		deflist = tp->rec_scope->sc_def;

		while (deflist) {
			if (deflist->df_kind & (D_FIELD|D_OFIELD|D_UFIELD)) {
				assert(deflist->df_type);
				gen_defdescr(f, (t_def *) 0, deflist->df_type);
			}
			deflist = deflist->df_nextinscope;
		}
		break;
	case T_GRAPH:
		/* produce typedef and descriptor for root and node */
		gen_defdescr(f, (t_def *) 0, tp->gra_root);
		gen_defdescr(f, (t_def *) 0, tp->gra_node);
		gen_defdescr(f, (t_def *) 0, tp->gra_name);
		break;
	case T_NODENAME:
		/* produce typedef and descriptor for graph */
		if (tp->tp_next) {
			gen_defdescr(f, (t_def *) 0, tp->tp_next);
		}
		break;
	case T_SET:
	case T_BAG:
	case T_ARRAY:
		/* produce typedef and descriptor for element */
		gen_defdescr(f, (t_def *) 0, element_type_of(tp));
		break;
	case T_OBJECT:
		/* produce typedef and descriptor for record */

		if (tp->tp_def->df_flags & D_PARTITIONED) {
			fprintf(f, "typedef instance_p %s;\n", tp->tp_tpdef);
		}
		else {
			fprintf(f, "typedef t_object %s;\n", tp->tp_tpdef);
		}
		deflist = record_type_of(tp)->rec_scope->sc_def;
		while (deflist) {
			if (deflist->df_kind == D_OPERATION) {
				gen_defdescr(fc, (t_def *) 0, deflist->df_type);
			}
			else if (deflist->df_kind & (D_TYPE|D_MODULE|D_OBJECT)) {
				generate_descrs(deflist);
			}
			deflist = deflist->df_nextinscope;
		}
		gen_defdescr(fc, (t_def *) 0, record_type_of(tp));
		gen_object_descr(tp, ext_descr);
		gen_marshall_funcs(tp, ext_descr);
		gen_compare_func(tp, ext_descr);
		gen_free_func(tp, ext_descr);
		gen_assign_func(tp, ext_descr);
		return;
	case T_FUNCTION: {
		/* produce typedef and descriptor for parameters and result */
		t_dflst pr;

		if (result_type_of(tp)) {
			gen_defdescr(f, (t_def *) 0, result_type_of(tp));
		}
		def_walklist(tp->prc_params, pr, deflist) {
			gen_defdescr(f, (t_def *) 0, deflist->df_type);
		}

		if (tp->tp_flags & T_FUNCADDR) {
			gen_prototype(f, (t_def *) 0, (char *) 0, tp);
			fputs(";\n", f);
		}
		}
		break;
	}

	/* Now produce wanted typedef. */
	fprintf(f, "typedef ");
	switch(tp->tp_fund) {
	case T_ENUM:
		fprintf(f, "%s ",
			ufit(tp->enm_ncst-1, 1) ? "t_enum" : "t_longenum");
		break;
	case T_UNION:
		fprintf(f, "struct %s {\n", tp->tp_tpdef);
		deflist = tp->rec_scope->sc_def;
		fprintf(f, "\tt_integer f_%s;\n", deflist->df_idf->id_text);
		fprintf(f, "\tt_boolean u_init;\n");
		fprintf(f, "\tunion {\n");
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(f, "\t\t%s %s;\n", deflist->df_type->tp_tpdef, deflist->df_name);
		}
		fprintf(f,"\t} u_el;\n} ");
		break;
	case T_RECORD: {
		int first = 1;
		fprintf(f, "struct %s {\n", tp->tp_tpdef);
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
			if ((deflist->df_kind & (D_FIELD|D_OFIELD)) &&
			    ! (deflist->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND))) {
				first = 0;
				fprintf(f, "\t%s %s;\n", deflist->df_type->tp_tpdef, deflist->df_name);
			}
			deflist = deflist->df_nextinscope;
		}
		fprintf(f, "%s} ", first ? "\tint dummy; /* no fields ? */\n" : "");
		}
		break;
	case T_FUNCTION:
		fprintf(f, "t_integer ");
		break;
	case T_NODENAME:
		fprintf(f, "t_nodename ");
		break;
	case T_SET:
		fprintf(f, "t_set ");
		break;
	case T_BAG:
		fprintf(f, "t_bag ");
		break;
	case T_ARRAY:
		if (tp->tp_flags & T_CONSTBNDS) {
			fprintf(f, "struct %s { %s a_data[%d]; } ",
				tp->tp_tpdef,
				element_type_of(tp)->tp_tpdef,
				tp->arr_size);
			break;
		}
		fprintf(f, "ARRAY_TYPE(%d) ", tp->arr_ndim);
		break;
	case T_GRAPH:
		fprintf(f, "struct %s {\n\tt_ghead g_grph;\n", tp->tp_tpdef);
		if (tp->gra_root->tp_tpdef) {
			fprintf(f, "\t%s g_root;\n", tp->gra_root->tp_tpdef);
		}
		fputs("} ", f);
		break;
	default:
		crash("gen_defdescr");
	}
	fprintf(f, "%s", tp->tp_tpdef);
	fputs(";\n", f);

	/* And now produce the descriptor. */
	switch(tp->tp_fund) {
	case T_ENUM:
		gen_enum_descr(tp, ext_descr);
		break;
	case T_UNION:
		gen_union_descr(tp, ext_descr);
		break;
	case T_RECORD:
		gen_record_descr(tp, ext_descr);
		break;
	case T_FUNCTION:
		gen_func_descr(tp, ext_descr);
		break;
	case T_NODENAME:
		if (tp->tp_next) gen_nodenm_descr(tp, ext_descr);
		break;
	case T_SET:
		gen_set_descr(tp, ext_descr);
		break;
	case T_BAG:
		gen_bag_descr(tp, ext_descr);
		break;
	case T_ARRAY:
		gen_array_descr(tp, ext_descr);
		break;
	case T_GRAPH:
		gen_graph_descr(tp, ext_descr);
		break;
	default:
		crash("gen_defdescr(2)");
	}
	gen_init_macro(tp, f);
	gen_marshall_funcs(tp, ext_descr);
	gen_compare_func(tp, ext_descr);
	gen_free_func(tp, ext_descr);
	gen_assign_func(tp, ext_descr);
}

static void
gen_enum_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	fprintf(fc,
		"%stp_dscr %s = { %s, sizeof(%s), 0, 0, %d, 0};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		ufit(tp->enm_ncst-1, 1) ? "ENUM" : "LONGENUM",
		tp->tp_tpdef,
		(int)tp->enm_ncst);
}

static void
gen_union_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	int	varcount = 0;
	t_def	*df = tp->rec_scope->sc_def->df_nextinscope;
	char	*s;

	s = gen_name("uf_", type_name(tp), 0);
	fprintf(fc, "static var_dscr %s[] = {\n", s);
	while (df) {
		varcount++;
		fprintf(fc,
			"\t{ %ld, offsetof(%s, u_el.%s),&%s},\n",
			df->fld_tagvalue->nd_int,
			tp->tp_tpdef,
			df->df_name,
			df->df_type->tp_descr);
		df = df->df_nextinscope;
	}
	fprintf(fc,
		"\t{0, 0}\n};\n%stp_dscr %s = { UNION, sizeof(%s), %d, &%s, %d, %s};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS,
		tp->rec_scope->sc_def->df_type->tp_descr,
		varcount,
		s);
	free(s);
}

static void
gen_record_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	int	nfields = 0;
	t_def	*df = tp->rec_scope->sc_def;
	char	*s;
	int	first = 1;

	s = gen_name("rf_", type_name(tp), 0);
	fprintf(fc, "static fld_dscr %s[] = {\n", s);
	while (df) {
		if ((df->df_kind & (D_FIELD|D_OFIELD)) &&
		    ! (df->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND))) {
			nfields++;
			fprintf(fc,
				"%s{ offsetof(%s, %s), &%s}",
				first ? "\t" : ",\n\t",
				tp->tp_tpdef,
				df->df_name,
				df->df_type->tp_descr);
			first = 0;
		}
		df = df->df_nextinscope;
	}
	fprintf(fc,
		"%s\n};\n%stp_dscr %s = { RECORD, %s%s%s, %d, 0, %d, %s};\n",
		first ? "{ 0, 0}" : "",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef ? "sizeof(" : "",
		tp->tp_tpdef ? tp->tp_tpdef : "0",
		tp->tp_tpdef ? ")" : "",
		tp->tp_flags & T_RTS_FLAGS,
		nfields,
		s);
	free(s);
}

static void
gen_func_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	t_dflst	pr;
	t_def	*df;
	char	*s;
	int	result_extra = 0;
	int	first = 1;

	if (tp->tp_def &&
	    tp->tp_def->df_kind == D_OPERATION &&
	    (CurrDef->df_flags & D_PARTITIONED) &&
	    tp->tp_def->prc_funcno < 2) {
		return;
	}
	s = gen_name("fa_", type_name(tp), 0);
	fprintf(fc, "static par_dscr %s[] = {\n", s);
	def_walklist(tp->prc_params, pr, df) {
		fprintf(fc,
			"%s{ %s, &%s, %s, %s}",
			first ? "\t" : ",\n\t",
			is_shared_param(df) ? "SHARED" :
			    is_in_param(df) ? "IN" :
				(df->df_flags & D_REDUCED) ? "REDUCED" :
				    (df->df_flags & D_GATHERED) ? "GATHERED" :
					"OUT",
			(df->df_flags & D_GATHERED) ? df->var_gathertp->tp_descr :
				df->df_type->tp_descr,
			df->df_type->tp_assignfunc,
			df->df_type->tp_freefunc ? df->df_type->tp_freefunc : "0");
		first = 0;
	}
	if (tp->tp_def
	    && tp->tp_def->df_kind == D_OPERATION
	    && result_type_of(tp) != 0) {
		result_extra = 1;
		fprintf(fc,
			"%s{ %s, &%s, %s, %s}",
			first ? "\t" : ",\n\t",
			tp->tp_def->opr_reducef ? "REDUCED" :
			    (tp->tp_def->df_flags & D_PARALLEL) ? "GATHERED" :
			    "OUT",
			result_type_of(tp)->tp_descr,
			result_type_of(tp)->tp_assignfunc,
			result_type_of(tp)->tp_freefunc ? result_type_of(tp)->tp_freefunc : "0");
		first = 0;
	}
	fprintf(fc, "%s\n};\n", first ? "\t{ 0, 0, 0, 0}" : "");

	if (tp->tp_def && tp->tp_def->df_kind == D_OPERATION &&
	    (CurrDef->df_flags & D_PARTITIONED)) {
		char *s2 = gen_name("poa_", type_name(tp), 0);
		df = tp->tp_def->opr_reducef;
		if (df) {
			gen_proto(df);
		}
		fprintf(fc, "static po_pardscr_t %s[] = {\n", s2);
		first = 1;
		def_walklist(tp->prc_params, pr, df) {
			fprintf(fc,
				"%s{ sizeof(%s), %s_PARAM|%s, (reduction_function_p) %s, &%s}",
				first ? "\t" : ",\n\t",
/*
				(df->df_flags & D_GATHERED) ?
					df->var_gathertp->tp_tpdef :
*/
					df->df_type->tp_tpdef,
				is_in_param(df) ? "IN" : "OUT",
				(df->df_flags & D_GATHERED) ? "GATHER" :
				    (df->df_flags & D_REDUCED) ? "REDUCE" : "0",
				(df->df_flags & D_REDUCED) ?
					df->var_reducef->df_name : "0",
				(df->df_flags & D_GATHERED) ?
					df->var_gathertp->tp_descr :
					df->df_type->tp_descr);
			first = 0;
		}
		if (result_type_of(tp) != 0) {
			fprintf(fc,
				"%s{ sizeof(%s), OUT_PARAM|%s,  (reduction_function_p) %s, &%s}",
				first ? "\t" : ",\n\t",
				((tp->tp_def->df_flags & D_PARALLEL) && tp->tp_def->opr_reducef == NULL) ?
					element_type_of(result_type_of(tp))->tp_tpdef :
					result_type_of(tp)->tp_tpdef,
				tp->tp_def->opr_reducef ? "REDUCE" :
					(tp->tp_def->df_flags & D_PARALLEL) ?
						"GATHER" : "0",
				tp->tp_def->opr_reducef ?
					tp->tp_def->opr_reducef->df_name : "0",
				result_type_of(tp)->tp_descr);
				first = 0;
		}
		fprintf(fc, "%s\n};\n", first ? "\t{ 0, 0, 0, 0}" : "");
		free(s2);
	}
	fprintf(fc,"%stp_dscr %s = { FUNCTION, sizeof(%s), 0, ",
		ext ? "" : "static ",
		tp->tp_descr, tp->tp_tpdef);
	if (! result_extra && result_type_of(tp) != 0) {
		fprintf(fc, "&%s, ", result_type_of(tp)->tp_descr);
	}
	else	fprintf(fc, "0, ");
	fprintf(fc, "%d, %s};\n", tp->prc_nparams+result_extra, s);
	free(s);
}

static void
gen_nodenm_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	fprintf(fc,
		"%stp_dscr %s = { NODENAME, sizeof(%s), 0, &%s, 0, 0};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef,
		tp->tp_next->tp_descr);
}

static void
gen_object_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	t_def	*df;
	char	*opnames = gen_name("od_", type_name(tp), 0);
	char	*objinfo = gen_name("oi_", type_name(tp), 0);
	int	first = 1;
	int	count = 0;

	walkdefs(CurrDef->bod_scope->sc_def,
		 D_OPERATION,
		 gen_operation_wrappers);
	if (! (CurrDef->df_flags & D_PARTITIONED)) {
		walkdefs(CurrDef->bod_scope->sc_def,
			 D_OPERATION,
			 gen_marshall);
	}
	fprintf(fc, "static op_dscr %s[] = {", opnames);
	for (df = record_type_of(tp)->rec_scope->sc_def;
	     df;
	     df = df->df_nextinscope) {
		if (df->df_kind == D_OPERATION) {
			t_scope	*sc = CurrentScope;
			char	*reads = gen_name("or__", df->df_idf->id_text, 0);
			char	*writes = gen_name("ow__", df->df_idf->id_text, 0);
			char	*s = gen_trace_name(df);

			CurrentScope = record_type_of(tp)->rec_scope;
			fprintf(fc,
				"%s\n\t{ %s, %s, &%s, %d, %s, %s",
				first ? "" : ",",
				(!(CurrDef->df_flags & D_PARTITIONED) &&
				 (df->df_flags & D_HASREADS))
					? reads
					: "0",
				(CurrDef->df_flags & D_PARTITIONED) ||
				! (df->df_flags & D_HASWRITES) ? "0"
					: writes,
				df->df_type->tp_descr,
				df->prc_funcno,
				pure_write(df) ?
				 ((df->df_flags & D_BLOCKING) ?
					"OP_BLOCKING|OP_PURE_WRITE" :
					"OP_PURE_WRITE") :
				 ((df->df_flags & D_BLOCKING) ?
					"OP_BLOCKING" : "0"),
				s);
			fprintf(fc, ",\n\t  %s", df->bod_marshallnames[0]);
			{ int i;
			  for (i = 1; i < 7; i++) {
				fprintf(fc, ", %s", df->bod_marshallnames[i]);
			  }
			}
			fputs("}", fc);
			first = 0;
			CurrentScope = sc;
			free(reads);
			free(writes);
			free(s);
			count++;
		}
	}
	assert(! first);
	fputs("\n};\n", fc);

	df = CurrDef;
	gen_marshall(df);

	if (record_type_of(tp)->tp_freefunc) {
		fprintf(fc, "static void %s(void *);\n",
			record_type_of(tp)->tp_freefunc);
	}
	fprintf(fc, "static obj_info %s = { %s, %s, %s, %s, %s };\n",
		objinfo,
		df->bod_marshallnames[0],
		df->bod_marshallnames[1],
		df->bod_marshallnames[2],
		record_type_of(tp)->tp_freefunc ?
			record_type_of(tp)->tp_freefunc : "0",
		opnames);
	fprintf(fc,
		"%stp_dscr %s = { %sOBJECT, sizeof(%s), %d, &%s, %d, &%s};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_flags & T_PART_OBJ ? "P" : "",
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS,
		record_type_of(tp)->tp_descr,
		count,
		objinfo);
	free(opnames);
	free(objinfo);
}

static void
gen_set_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	t_type	*t = element_type_of(tp);
	char	*n = gen_name("sd_", type_name(tp), 0);

	if (! ext) {
		fprintf(fc, "static void %s(void *);\n",
			tp->tp_freefunc);
		fprintf(fc, "static void %s(void *, void *);\n",
			tp->tp_assignfunc);
	}
	fprintf(fc, "static set_dscr %s = { %s, %s, %s, %s, %s};\n",
		n,
		tp->tp_freefunc,
		tp->tp_assignfunc,
		t->tp_comparefunc,
		t->tp_freefunc ? t->tp_freefunc : "0",
		t->tp_assignfunc);

	fprintf(fc,
		"%stp_dscr %s = { SET, sizeof(%s), %d, &%s, 0, &%s};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS,
		t->tp_descr,
		n);
	free(n);
}

static void
gen_bag_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	t_type	*t = element_type_of(tp);
	char	*n = gen_name("sd_", type_name(tp), 0);

	if (! ext) {
		fprintf(fc, "static void %s(void *);\n",
			tp->tp_freefunc);
		fprintf(fc, "static void %s(void *, void *);\n",
			tp->tp_assignfunc);
	}
	fprintf(fc, "static set_dscr %s = { %s, %s, %s, %s, %s};\n",
		n,
		tp->tp_freefunc,
		tp->tp_assignfunc,
		t->tp_comparefunc,
		t->tp_freefunc ? t->tp_freefunc : "0",
		t->tp_assignfunc);
	free(n);

	fprintf(fc,
		"%stp_dscr %s = { BAG, sizeof(%s), %d, &%s, 0, &%s};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS,
		t->tp_descr,
		n);
}

static void
gen_array_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	char	*arrdimname = gen_name("ar_", type_name(tp), 0);
	int	i;

	fprintf(fc, "static tp_dscr *%s[] = {\n", arrdimname);

	for (i = 0; i < tp->arr_ndim; i++) {
		fprintf(fc, "%s&%s",
			i == 0 ? "\t" : ",\n\t",
			tp->arr_index(i)->tp_descr);
	}

	fprintf(fc,
		"\n};\n%stp_dscr %s = { %sARRAY, sizeof(%s), %d, &%s, %d, %s};\n",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_flags & T_CONSTBNDS ? "FIXED_" : "",
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS,
		element_type_of(tp)->tp_descr,
		tp->arr_ndim,
		arrdimname);
	free(arrdimname);
}

static void
gen_graph_descr(tp, ext)
	t_type	*tp;
	int	ext;
{
	fprintf(fc,
		"%stp_dscr %s = { GRAPH, sizeof(%s), %d, ",
		ext ? "" : "static ",
		tp->tp_descr,
		tp->tp_tpdef,
		tp->tp_flags & T_RTS_FLAGS);
	if (tp->gra_root->tp_descr) {
		fprintf(fc,
			"&%s, offsetof(%s, g_root), ",
			tp->gra_root->tp_descr,
			tp->tp_tpdef);
	}
	else	fprintf(fc, "0, 0, ");
	fprintf(fc, "&%s};\n", tp->gra_node->tp_descr);
}
