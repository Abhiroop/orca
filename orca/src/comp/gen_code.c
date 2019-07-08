/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: gen_code.c,v 1.87 1998/09/02 16:18:56 ceriel Exp $ */

/* Code generation for function/operation/process bodies and statements. */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <assert.h>

#include "gen_code.h"
#include "scope.h"
#include "node.h"
#include "misc.h"
#include "generate.h"
#include "error.h"
#include "main.h"
#include "temps.h"
#include "gen_expr.h"
#include "closure.h"
#include "visit.h"
#include "options.h"
#include "marshall.h"

_PROTOTYPE(static void gen_body, (t_def *, p_node));
_PROTOTYPE(static void gen_free, (t_def *));
_PROTOTYPE(static void gen_initvar, (t_def *));
_PROTOTYPE(static void gen_compl_ass, (p_node));
_PROTOTYPE(static void gen_Stat, (p_node));
_PROTOTYPE(static void gen_guard, (p_node));
_PROTOTYPE(static void gen_forstat, (p_node));
_PROTOTYPE(static void gen_casestat, (p_node));
_PROTOTYPE(static int gen_assignmemaggr, (p_node));
_PROTOTYPE(static void gen_arrelem, (void));
_PROTOTYPE(static void gen_cases, (p_node));
_PROTOTYPE(static char *gen_label, (void));
_PROTOTYPE(static void obj_copy, (t_type *tp));
_PROTOTYPE(static void objsav_copy, (t_type *tp));
_PROTOTYPE(static void gen_part_func, (t_def *));
_PROTOTYPE(static void gen_part_create, (t_def *));
_PROTOTYPE(static void gen_part_prot, (FILE *, t_def *, char *, int));
_PROTOTYPE(static void copy_shared_or_out_params, (t_def *));
_PROTOTYPE(static void declarations, (p_def, p_node));
_PROTOTYPE(static void cleanup_declarations, (p_def, p_node));
_PROTOTYPE(static void gen_statlist, (p_node, int));
_PROTOTYPE(static int designates_obj_field, (p_node));
_PROTOTYPE(static void gen_pdg_func, (p_def));
_PROTOTYPE(static void gen_oper_params, (t_dflst));
_PROTOTYPE(static void gen_op_wrapper, (p_def, int, char *));
_PROTOTYPE(static void gen_part_operation_wrapper, (p_def df));

#define UNROLL_THRESHOLD	5

int	clone_local_obj;
int	indentlevel;
int	lineno;
static t_def
	*func_def;
static char
	*return_label = "retlab";
static int
	retlab_used = 0;
static int
	gretlab_used = 0;

void
gen_data(df)
	t_def	*df;
{
	/*	Generate declarations for a DATA variable.
	*/

	assert(df->df_kind == D_VARIABLE);
	assert(df->df_flags & D_DATA);
	fprintf(fh, "extern %s %s;\n",
		df->df_type->tp_tpdef, df->df_name);
	fprintf(fc, "%s %s;\n",
		df->df_type->tp_tpdef, df->df_name);
}

void
gen_prototype(f, funcdf, nm, ftp)
	FILE	*f;
	t_def	*funcdf;
	char	*nm;
	t_type	*ftp;
{
	/*	Generate a prototype for the function indicated by
		'funcdf', on file descriptor 'f'.
		This function can be used for declaration as well as
		definition, because no terminating ; is produced.
		The name to be used for the function is indicated by 'nm'.
		if 'ftp' is set, a function type is declared instead.
	*/

	t_dflst	l;
	t_def	*df = funcdf;
	t_type	*result_tp = ftp ? result_type_of(ftp)
				 : result_type_of(df->df_type);
	int	first = 1;

	if (! ftp && (CurrDef->df_flags & D_PARTITIONED) &&
	    (df->df_kind == D_OPERATION || df == CurrDef)) {
		gen_part_prot(f, df, nm, 0);
		return;
	}

	if (! ftp && df->df_kind == D_MODULE) {
		assert(df->df_flags & D_DATA);
		fprintf(fc, "static void %s(void)", df->df_name);
		return;
	}

	/* Static or external linkage? */
	if (ftp) {
		fputs("typedef ", f);
	}
	else if (! (df->df_flags & D_EXPORTED)) {
		fputs("static ", f);
	}

	/* Result type? */
	if (result_tp) {
		if (! is_constructed_type(result_tp)) {
			fprintf(f, "%s ", result_tp->tp_tpdef);
		}
		else if (result_tp->tp_fund == T_GENERIC) {
			fprintf(f, "ch%s(%s,void) ",
				result_tp->tp_tpdef, result_tp->tp_tpdef);
		}
		else fputs("void ", f);
	}
	else fputs("void ", f);

	/* Function name. */
	if (ftp) {
		fprintf(f, "(* %s) (", ftp->prc_ftp);
	}
	else fprintf(f, "%s(", nm);

	/* If operation, the first parameter is the object. */
	if (df && df->df_kind == D_OPERATION) {
		first = 0;
		if (df->df_flags & D_BLOCKING) {
			fputs("int *op_flags, ", f);
		}
		fprintf(f, "%s *v__obj",
			df->df_type->prc_objtype->tp_tpdef);
	}

	if (ftp ||
	    (df->df_kind == D_FUNCTION &&
	     ((df->df_flags & D_EXTRAPARAM) || nm == df->prc_addrname))) {
		first = 0;
		fprintf(f, "int *op_flags");
	}

	/* Parameter list. */
	def_walklist(ftp ? ftp->prc_params: df->df_type->prc_params, l, df) {
		t_type *tp = df->df_type;
		if (! first) fputs(", ", f);
		first = 0;
		fprintf(f, "%s ", tp->tp_tpdef);
		if (is_in_param(df)) {
			if (tp->tp_fund == T_GENERIC) {
				fprintf(f, "ch%s(NOTHING,*)", tp->tp_tpdef);
			}
			else if (is_constructed_type(tp)) {
				putc('*', f);
			}
		}
		else putc('*', f);
		if (! ftp) {
			if (is_out_param(df) || (df->df_flags & D_COPY)) {
				putc('o', f);
			}
			fprintf(f, "%s", df->df_name);
		}
	}

	/* Extra parameter required for result? Also make sure that
	   (void) is produced when there are no parameters.
	*/
	if (result_tp) {
		if (result_tp->tp_fund == T_GENERIC) {
			fprintf(f, " ch%s(%s%s *v__result)",
				result_tp->tp_tpdef,
				first ? "void," : "NOTHING,COMMA ",
				result_tp->tp_tpdef);
		}
		else if (is_constructed_type(result_tp)) {
			fprintf(f, "%s%s *v__result",
				first ? "" : ", ",
				result_tp->tp_tpdef);
		}
		else if (first) fputs("void", f);
	}
	else if (first) fputs("void", f);
	putc(')', f);
}

void
gen_proto(df)
	t_def	*df;
{
	FILE	*f = (df->df_kind == D_OBJECT || (df->df_flags & D_EXPORTED))
			? fh
			: fc;

	switch(df->df_kind) {
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		if (df->df_flags & D_HAS_SHARED_OBJ_PARAM) {
			if (options['f']) {
				gen_prototype(f, df, df->prc_name, (t_type *) 0);
				fputs(";\n", f);
			}
			else if (f == fh && ! (CurrDef->df_flags & D_GENERIC)) {
				fprintf(f,
					"#define %s %s\n",
					df->prc_name,
					df->df_name);
			}
		}
		gen_prototype(f, df, df->df_name, (t_type *) 0);
		fputs(";\n", f);
		break;
	case D_OPERATION:
		if (df->prc_funcno < 2) break;
		if (CurrDef->df_flags & D_PARTITIONED) {
			fprintf(f, "void %s(int, instance_p, void **);\n",
				df->opr_namew);
			break;
		}
		if (df->df_flags & D_HASREADS) {
		    gen_prototype(f, df, df->opr_namer, (t_type *) 0);
		    fputs(";\n", f);
		}
		if (df->df_flags & D_HASWRITES) {
		    gen_prototype(f, df, df->opr_namew, (t_type *) 0);
		    fputs(";\n", f);
		}
		break;
	case D_OBJECT:
		if (df->df_flags & D_PARTITIONED) {
			int	i;
			fprintf(f, "void %s(instance_p *p", df->df_type->tp_batinit);
			for (i = 0; i < df->df_type->arr_ndim; i++) {
				fprintf(f, ", int lb%d, int ub%d", i, i);
			}
			fputs(");\n", f);
		}
		fprintf(f, "void %s(%s *v__obj, char *obj_name);\n",
			df->df_type->tp_init,
			df->df_type->tp_tpdef);
		break;
	}
}

void
gen_func(df)
	t_def	*df;
{
	/*	Produce body for a function, operation or process.
	*/
	t_scope	*csc = CurrentScope,
		*psc = ProcScope;

	CurrentScope = df->bod_scope;
	ProcScope = CurrentScope;

	switch(df->df_kind) {
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		if (! (df->df_flags & D_DEFINED)) break;
		if (options['f'] && (df->df_flags & D_HAS_SHARED_OBJ_PARAM)) {
			clone_local_obj = 1;
			gen_prototype(fc, df, df->prc_name, (t_type *) 0);
			gen_body(df, df->bod_statlist1);
			clone_local_obj = 0;
		}
		/* Fall through */
	case D_PROCESS:
		if (df->df_flags & D_DEFINED) {
			gen_prototype(fc, df, df->df_name, (t_type *) 0);
			gen_body(df, df->bod_statlist1);
		}
		break;
	case D_MODULE:
		assert(df->df_flags & D_DATA);
		gen_prototype(fc, df, df->df_name, (t_type *) 0);
		gen_body(df, df->bod_statlist1);
		break;

	case D_OPERATION:
		if (df->prc_funcno < 2) break;
		if (CurrDef->df_flags & D_PARTITIONED) {
			gen_part_func(df);
			break;
		}
		if (df->df_flags & D_HASREADS) {
		    gen_prototype(fc, df, df->opr_namer, (t_type *) 0);
		    gen_body(df, df->bod_statlist1);
		}
		if (df->df_flags & D_HASWRITES) {
		    gen_prototype(fc, df, df->opr_namew, (t_type *) 0);
		    gen_body(df, df->bod_statlist2);
		}
		break;
	case D_OBJECT:
		fprintf(fc, "void %s(%s *v__obj, char *obj_name)",
			df->df_type->tp_init,
			df->df_type->tp_tpdef);
		if (df->df_flags & D_PARTITIONED) {
			fputs("{\n    *v__obj = 0;\n}\n", fc);
			gen_part_create(df);
		}
		else {
			gen_body(df, df->bod_statlist1);
		}
		break;
	}
	def_endlist(&df->bod_transdep);

	CurrentScope = csc;
	ProcScope = psc;
}

void
gen_operation_wrappers(oper)
	t_def	*oper;
{
	if (CurrDef->df_flags & D_PARTITIONED) {
		gen_part_operation_wrapper(oper);
		return;
	}
	if (oper->df_flags & D_HASREADS) {
		char	*reads = gen_name("or__", oper->df_idf->id_text, 0);

		gen_op_wrapper(oper, 0, reads);
		free(reads);
	}
	if (oper->df_flags & D_HASWRITES) {
		char	*writes = gen_name("ow__", oper->df_idf->id_text, 0);

		gen_op_wrapper(oper, 1, writes);
		free(writes);
	}
}

static void
gen_op_wrapper(oper, writes, name)
	t_def	*oper;
	int	writes;
	char	*name;
{
	t_type	*result = result_type_of(oper->df_type);
	char	*opname;

	opname = writes ? oper->opr_namew : oper->opr_namer;

	if (oper->prc_funcno >= 2) {
		gen_prototype(fc, oper, opname, (t_type *) 0);
		fputs(";\n", fc);
	}
	fprintf(fc, "static int %s(t_object *v_obj, void **v__args) {\n",
		name);
	if (oper->df_flags & D_BLOCKING) {
		fputs("    int op_flags = NESTED;\n", fc);
	}
	fputs("    ", fc);
	if (result) {
		if (! is_constructed_type(result)) {
			fprintf(fc, "*((%s *) v__args[%d]) = ",
				result->tp_tpdef, oper->df_type->prc_nparams);
		}
		else if (result->tp_fund == T_GENERIC) {
			fprintf(fc, "ch%s(*((%s *) v__args[%d]) = ,NOTHING)",
				result->tp_tpdef,
				result->tp_tpdef,
				oper->df_type->prc_nparams);
		}
	}
	if (oper->prc_funcno == 0) {
		fprintf(fc, "%s((t_object *) (v__args[0]), \"result\");\n",
			oper->df_type->prc_objtype->tp_init);
		fputs("    ", fc);
		fprintf(fc, "%s(((t_object *) (v__args[0]))->o_fields, v_obj->o_fields);\n",
			record_type_of(oper->df_type->prc_objtype)->tp_assignfunc);
	}
	else if (oper->prc_funcno == 1) {
		fprintf(fc, "%s(v_obj->o_fields, ((t_object *) (v__args[0]))->o_fields);\n",
			record_type_of(oper->df_type->prc_objtype)->tp_assignfunc);
	}
	else {
		assert(oper->prc_funcno >= 2);
		fprintf(fc, "%s(", opname);
		if (oper->df_flags & D_BLOCKING) {
			fputs("&op_flags, ", fc);
		}
		fputs("v_obj", fc);
		if (! def_emptylist(oper->df_type->prc_params)) {
			putc(',', fc);
			gen_oper_params(oper->df_type->prc_params);
		}
		if (result) {
			if (result->tp_fund & T_GENERIC) {
				fprintf(fc, " ch%s(NOTHING,COMMA ((%s *) v__args[%d]))",
					result->tp_tpdef,
					result->tp_tpdef,
					oper->df_type->prc_nparams);
			}
			else if (is_constructed_type(result)) {
				fprintf(fc, ",((%s *) v__args[%d])",
					result->tp_tpdef,
					oper->df_type->prc_nparams);
			}
		}
		fputs(");\n", fc);
	}
	if (oper->df_flags & D_BLOCKING) {
		fputs("    return (op_flags & BLOCKING) ? 1 : 0;\n", fc);
	}
	else	fputs("    return 0;\n", fc);
	fputs("}\n", fc);
	if (writes & 2) free(opname);
}

static void
declarations(fnc, list)
	t_def	*fnc;
	p_node	list;
{
	t_body	*b = fnc->df_body;
	t_scope	*sc = fnc->bod_scope;
	t_def	*df = sc->sc_def;
	t_tmp	*t = b->temps;
	t_type	*result_tp = result_type_of(fnc->df_type);
	t_type	*objtype;

	if (fnc->df_kind == D_OPERATION) objtype = fnc->df_type->prc_objtype;
	else objtype = fnc->df_type;

	if (node_emptylist(list) && node_emptylist(b->init)) {
		if (fnc->df_kind == D_OBJECT) {
			t_type	*tp = record_type_of(fnc->df_type);

			fprintf(fc, "    v__obj->o_fields = m_malloc(sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    memset(v__obj->o_fields, 0, sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    o_init_rtsdep(v__obj, &%s, obj_name);\n",
				fnc->df_type->tp_descr);
		}
		fputs("}\n", fc);
		return;
	}

	switch(fnc->df_kind) {
	case D_OPERATION:
		if (fnc->df_flags & D_BLOCKING) break;
		if (fnc->df_flags & D_CALLS_OP) {
			fputs("    int opflags = NESTED;\n    int *op_flags = &opflags;\n", fc);
		}
		break;
	case D_PROCESS:
	case D_OBJECT:
		fputs("    int opflags = 0;\n    int *op_flags = &opflags;\n", fc);
		break;
	case D_FUNCTION:
		if ((fnc->df_flags & D_CALLS_OP) &&
		    ! (fnc->df_flags & D_EXTRAPARAM)) {
			fputs("    int opflags = 0;\n    int *op_flags = &opflags;\n", fc);
		}
	}

	func_def = fnc;

	if (fnc->bod_argtabsz) {
		fprintf(fc, "    void *argtab[%d];\n", fnc->bod_argtabsz);
	}
	/* Declare local variables. */
	if (! (fnc->df_kind & (D_OBJECT|D_MODULE))) while (df) {
		if (df->df_kind == D_VARIABLE) {
			if (! is_parameter(df)
			    || is_out_param(df)
			    || (df->df_flags & D_COPY)) {
				fprintf(fc, "    %s %s;\n",
					df->df_type->tp_tpdef, df->df_name);
			}
			if (df->df_flags & D_REDUCED) {
				fprintf(fc, "    %s *p%s = &%s;\n",
					df->df_type->tp_tpdef, df->df_name, df->df_name);
			}
		}
		df = df->df_nextinscope;
	}

	/* Declare temporaries. */
	while (t) {
		fprintf(fc, "    %s %s",
			t->tmp_type->tp_tpdef,
			t->tmp_ispointer ? "*" : "");
		fprintf(fc, TMP_FMT, t->tmp_id);
		fputs(";\n", fc);
		t = t->tmp_scopelink;
	}

	/* Possibly declare result variable. */
	if (result_tp &&
	    (fnc->df_kind != D_OPERATION ||
	     ! (CurrDef->df_flags & D_PARTITIONED))) {
		if (! is_constructed_type(result_tp)) {
			fprintf(fc, "    %s v__result = 0;\n", result_tp->tp_tpdef);
		}
		else if (result_tp->tp_fund == T_GENERIC) {
			fprintf(fc, "    ch%s(%s v__result = 0;,NOTHING)\n",
				result_tp->tp_tpdef,
				result_tp->tp_tpdef);
		}
	}
	if (fnc->df_kind == D_OPERATION &&
	    ! (CurrDef->df_flags & D_PARTITIONED)) {
		/* Declare pointer to fields of object. */
		fprintf(fc, "    %s *v__ofldp = v__obj->o_fields;\n",
			record_type_of(objtype)->tp_tpdef);
		if (list == b->statlist2
		    && (fnc->df_flags & D_OBJ_COPY)) {
			fprintf(fc, "    %s sav_obj;\n",
				record_type_of(objtype)->tp_tpdef);
		}
	}
	else if (fnc->df_kind == D_OBJECT &&
		 ! (fnc->df_flags & D_PARTITIONED)) {
		/* Declare pointer to fields of object. */
		fprintf(fc, "    %s *v__ofldp;\n",
			record_type_of(objtype)->tp_tpdef);
	}

	if (fnc->df_flags & D_REDUCED) {
		/* Has reduced parameters and/or result */
		if (fnc->opr_reducef) {
			/* reduced result. */
			fprintf(fc, "    %s Xv__result, *v__result = &Xv__result;\n",
				result_tp->tp_tpdef);
			if (result_tp->tp_flags & T_INIT_CODE) {
				fprintf(fc, "    %s(v__result, (char *) 0);\n",
					result_tp->tp_init);
			}
		}
	}

	/* Initialize temporaries. */
	for (t = b->temps; t; t = t->tmp_scopelink) {
		if (! t->tmp_ispointer &&
		    (t->tmp_type->tp_flags & T_INIT_CODE)) {
			if (! is_constructed_type(t->tmp_type)) {
				fputs("    ", fc);
				fprintf(fc, TMP_FMT, t->tmp_id);
				fputs(" = 0;\n", fc);
			}
			else {
				fprintf(fc, "    %s(&",
					t->tmp_type->tp_init);
				fprintf(fc, TMP_FMT, t->tmp_id);
				fputs(", (char *) 0);\n", fc);
			}
		}
	}
}

static void
copy_shared_or_out_params(fnc)
	t_def	*fnc;
{
	t_def	*df;
	t_scope	*sc = fnc->bod_scope;

	indentlevel = 4;
	for (df = sc->sc_def; df; df = df->df_nextinscope) {
		if (df->df_kind == D_VARIABLE && is_out_param(df)) {
			if (df->df_type->tp_flags & T_DYNAMIC) {
				if (df->df_type->tp_fund == T_GENERIC) {
				    fprintf(fc, "#if dynamic_%s\n",
					df->df_type->tp_tpdef);
				}
				fprintf(fc, "    %s(o%s);\n",
					df->df_type->tp_freefunc,
					df->df_name);
				if (df->df_type->tp_fund == T_GENERIC) {
				    fputs("#endif\n", fc);
				}
				fprintf(fc, "    *o%s = %s;\n",
					df->df_name, df->df_name);
			}
			else {
				fprintf(fc, "    *o%s = %s;\n",
					df->df_name, df->df_name);
			}
		}
	}
}

static void
cleanup_declarations(fnc, list)
	t_def	*fnc;
	p_node	list;
{
	t_body	*b = fnc->df_body;
	t_scope	*sc = fnc->bod_scope;
	t_def	*df = sc->sc_def;
	t_tmp	*t = b->temps;
	t_type	*result_tp = result_type_of(fnc->df_type);
	t_type	*objtype;

	if (fnc->df_kind == D_OPERATION) objtype = fnc->df_type->prc_objtype;
	else objtype = fnc->df_type;

	if (fnc->df_kind == D_OPERATION &&
	    list == b->statlist2 &&
	    (fnc->df_flags & D_OBJ_COPY)) {
		t_type *rectp = record_type_of(objtype);

		if (rectp->tp_flags & T_DYNAMIC) {
			if (rectp->tp_fund == T_GENERIC) {
				fprintf(fc, "#if dynamic_%s\n",
					rectp->tp_tpdef);
			}
			fprintf(fc, "    %s(&sav_obj);\n",
				rectp->tp_freefunc);
			if (rectp->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
	}

	/* Free local variables. */
	if (! (fnc->df_kind & (D_OBJECT|D_MODULE))) {
	    for (df = sc->sc_def; df; df = df->df_nextinscope) {
		if (df->df_kind == D_VARIABLE
		    && (df->df_type->tp_flags & T_DYNAMIC)
		    && ((fnc->df_kind == D_PROCESS && ! is_shared_param(df)) ||
			! is_parameter(df) || (df->df_flags & D_COPY))) {
			gen_free(df);
		}
	    }
	}

	/* Free temporaries. */
	for (t = b->temps; t; t = t->tmp_scopelink) {
		if (! t->tmp_ispointer &&
		    (t->tmp_type->tp_flags & T_DYNAMIC)) {
			if (t->tmp_type->tp_fund == T_GENERIC) {
				fprintf(fc, "#if dynamic_%s\n",
					t->tmp_type->tp_tpdef);
			}
			fprintf(fc, "    %s(&",
				t->tmp_type->tp_freefunc);
			fprintf(fc, TMP_FMT, t->tmp_id);
			fputs(");\n", fc);
			if (t->tmp_type->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
	}

	/* Possibly return result. */
	if (result_tp &&
	    (fnc->df_kind != D_OPERATION ||
	     ! (CurrDef->df_flags & D_PARTITIONED))) {
		if (! is_constructed_type(result_tp)) {
			fputs("    return v__result;\n", fc);
		}
		else if (result_tp->tp_fund == T_GENERIC) {
			fprintf(fc, "    ch%s(return v__result;,NOTHING)\n",
				result_tp->tp_tpdef);
		}
	}
}

static void
gen_body(fnc, list)
	t_def	*fnc;
	p_node	list;
{
	t_body	*b = fnc->df_body;
	t_type	*objtype;

	if (fnc->df_kind == D_OPERATION) objtype = fnc->df_type->prc_objtype;
	else objtype = fnc->df_type;

	retlab_used = 0;
	gretlab_used = 0;

	fputs(" {\n", fc);

	if (node_emptylist(list) && node_emptylist(b->init)) {
		if (fnc->df_kind == D_OBJECT) {
			t_type	*tp = record_type_of(fnc->df_type);

			fprintf(fc, "    v__obj->o_fields = m_malloc(sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    memset(v__obj->o_fields, 0, sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    o_init_rtsdep(v__obj, &%s, obj_name);\n",
				fnc->df_type->tp_descr);
		}
		fputs("}\n", fc);
		return;
	}

	func_def = fnc;

	declarations(fnc, list);

	if (fnc->df_kind == D_OBJECT) {
		t_type	*tp = record_type_of(fnc->df_type);

		fprintf(fc, "    v__obj->o_fields = m_malloc(sizeof(%s));\n",
			tp->tp_tpdef);
		fprintf(fc, "    memset(v__obj->o_fields, 0, sizeof(%s));\n",
			tp->tp_tpdef);
		fprintf(fc, "    o_init_rtsdep(v__obj, &%s, obj_name);\n",
			fnc->df_type->tp_descr);
		fputs("    v__ofldp = v__obj->o_fields;\n", fc);
	}

	if (fnc->df_kind == D_OPERATION
	    && list == b->statlist2
	    && (fnc->df_flags & D_OBJ_COPY)) {
		indentlevel = 4;
		obj_copy(record_type_of(objtype));
		indentlevel = 0;
	}

	gen_statlist(b->init, 0);
	if (fnc->df_kind == D_PROCESS) {
		gen_score_calls(fnc);
	}
	gen_statlist(list, fnc->df_kind == D_FUNCTION);

	if (fnc->df_kind != D_OBJECT
	    && result_type_of(fnc->df_type) != 0) {
		fprintf(fc, "    m_trap(FALL_THROUGH, %s, %d);\n",
			CurrDef->mod_fn, lineno);
	}

	if (gretlab_used || retlab_used) {
		fputs("retlab:;\n", fc);
	}

	if (fnc->df_kind == D_PROCESS) {
		gen_erocs_calls(fnc);
	}

	if (fnc->df_kind == D_OPERATION
	    && list == b->statlist2) {
		if ((fnc->df_flags & (D_OBJ_COPY|D_GUARDS)) == D_OBJ_COPY) {
			fputs("    if (*op_flags & BLOCKING) {\n", fc);
			indentlevel = 8;
			objsav_copy(record_type_of(objtype));
			fputs("\tgoto blocking_oper;\n    }\n", fc);
		}
	}

	if (! (fnc->df_kind & (D_OBJECT|D_MODULE))) {
		copy_shared_or_out_params(fnc);
	}

	if (fnc->df_kind == D_OPERATION && (fnc->df_flags & D_BLOCKING)) {
		fputs("blocking_oper:;\n", fc);
	}

	indentlevel = 0;

	cleanup_declarations(fnc, list);

	fputs("}\n", fc);
}

static char *
gen_label()
{
	static int
		labcount;
	char	buf[20];

	sprintf(buf, "%d", ++labcount);
	return mk_str("lbl__", buf, (char *) 0);
}

static char *exitlabel;

static void
gen_Stat(nd)
	p_node	nd;
{
	/*	Generate code for the statement indicated by nd.
		Sorry, but this is just a very large switch.
	*/

	/* Level counts are maintained to get EXIT code right. In some
	   cases it can just be a "break", but not when it occurs inside
	   a CASE statement. In this case, a 'goto' is required.
	*/
	static int
		level, looplevel, caselevel;
	int	sv_looplevel = looplevel,
		sv_caselevel = caselevel;
	char	*sv_exitlabel = exitlabel;
	FILE	*f = fc;

	level++;
	switch(nd->nd_symb) {
	case CHECK:
		nd = nd->nd_expr;
		if (! (nd->nd_symb)) {
			/* Check removed by optimizer? */
			putc('\n', f);
			break;
		}
		switch(nd->nd_symb) {
		case A_CHECK:
			if (nd->nd_left &&
			    (nd->nd_left->nd_type->tp_flags & T_CONSTBNDS)) {
				t_node	*bnds = nd->nd_left->nd_type->arr_bounds(nd->nd_dimno);
				fprintf(f, "a_fixcheck(%ld, ",
					bnds->nd_right->nd_int - bnds->nd_left->nd_int+1);
				gen_expr(f, nd->nd_right);
				break;
			}
			fputs("a_check(", f);
			if (! nd->nd_left) {
				fputs("val", f);
			}
			else {
				gen_addr(f, nd->nd_left);
			}
			fputs(", ", f);
			gen_expr(f, nd->nd_right);
			fprintf(f, ", %d", nd->nd_dimno);
			break;

		case U_CHECK:
			/* Union tagvalue check. */
			fputs("u_check(", f);
			gen_addr(f, nd->nd_left);
			fputs(", ", f);
			nd = nd->nd_right;
			assert(nd->nd_class == Def && nd->nd_def->df_kind == D_UFIELD);
			gen_expr(f, nd->nd_def->fld_tagvalue);
			break;

		case G_CHECK:
			/* Nodename and graph check. */
			fputs("g_check(", f);
			gen_addr(f, nd->nd_left);
			fputs(", ", f);
			gen_addr(f, nd->nd_right);
			break;

		case ALIAS_CHK:
			/* Alias check. */
			fputs("m_aliaschk(", f);
			gen_addr(f, nd->nd_left);
			fputs(", ", f);
			gen_addr(f, nd->nd_right);
			fprintf(f, ", &%s, &%s",
				nd->nd_left->nd_type->tp_descr,
				nd->nd_right->nd_type->tp_descr);
			break;
		case FROM_CHECK:
			fprintf(f, "%c_fromcheck(", base_char_of(nd->nd_right->nd_type));
			gen_addr(f, nd->nd_right);
			break;
		case DIV_CHECK:
			fputs("m_divcheck(", f);
			gen_expr(f, nd->nd_right);
			break;
		case MOD_CHECK:
			fputs("m_modcheck(", f);
			gen_expr(f, nd->nd_right);
			break;
		case CPU_CHECK:
			fputs("m_cpucheck(", f);
			gen_expr(f, nd->nd_right);
			break;
		}
		fprintf(f, ", %s, %d);\n", CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		break;

	case INIT:
		/* Initialization of variable or object field. */
		gen_initvar(nd->nd_desig->nd_def);
		break;

	case DO:
		/* DO loop (or WHILE or REPEAT (with COND_EXIT) */
		exitlabel = 0;
		looplevel = level;
		fputs("for (;;) {\n", f);
		gen_statlist(nd->nd_list1, 1);
		indent(f);
		fputs("}\n", f);
		looplevel = sv_looplevel;
		if (exitlabel) {
			/* The loop body contains an EXIT that requires
			   a label.
			*/
			fprintf(f, "%s:;\n", exitlabel);
			free(exitlabel);
		}
		exitlabel = sv_exitlabel;
		break;

	case COND_EXIT:
		fputs("if (", f);
		gen_expr_ld(f, nd->nd_expr);
		fputs(") break;\n", f);
		break;

	case IF:
		fputs("if (", f);
		gen_expr_ld(f, nd->nd_expr);
		fputs(") {\n", f);
		gen_statlist(nd->nd_list1, 0);
		indent(f);
		if (! node_emptylist(nd->nd_list2)) {
			/* There is an ELSE part. */
			fputs("} else {\n", f);
			gen_statlist(nd->nd_list2, 0);
			indent(f);
		}
		fputs("}\n", f);
		break;

	case CASE:
		caselevel = level;
		gen_casestat(nd);
		caselevel = sv_caselevel;
		break;

	case ARROW:
		/* A case in a case statement. Produce case labels and
		   body.
		*/
		gen_cases(nd->nd_list1);
		gen_statlist(nd->nd_list2, 0);
		indent(f);
		fputs("    break;\n", f);
		break;

	case FOR:
		exitlabel = 0;
		looplevel = level;
		gen_forstat(nd);
		looplevel = sv_looplevel;
		exitlabel = sv_exitlabel;
		break;

	case EXIT:
		if (caselevel < looplevel) {
			fputs("break;\n", f);
			break;
		}
		if (! exitlabel) exitlabel = gen_label();
		fprintf(f, "goto %s;\n", exitlabel);
		break;

	case RETURN:
		if (! nd->nd_expr) {
			fprintf(f, "goto %s;\n", return_label);
			retlab_used = 1;
			break;
		}
		/* fall through */
	case ALIASBECOMES:
	case BECOMES:
	case TMPBECOMES: {
		p_node	expr = nd->nd_expr;
		t_type *tp = expr->nd_type;

		if (expr->nd_class == Call && expr->nd_target) {
			gen_expr(f, expr);
			fputs(";\n", f);
			if (nd->nd_symb == RETURN) {
				indent(f);
				fprintf(f, "goto %s;\n", return_label);
				retlab_used = 1;
			}
			break;
		}
		if (nd->nd_desig->nd_class == Tmp
		    && nd->nd_symb == TMPBECOMES
		    && (! (nd->nd_desig->nd_flags & ND_RETVAR))
		    && nd->nd_desig->nd_tmpvar->tmp_ispointer) {
			gen_addr(f, nd->nd_desig);
			fputs(" = ", f);
			gen_addr(f, expr);
			fputs(";\n", f);
			break;
		}
		if (nd->nd_symb == ALIASBECOMES) {
			fputs("if (", f);
			gen_addr(f, nd->nd_desig);
			fputs(" != ", f);
			gen_addr(f, nd->nd_expr);
			fputs(") {\n", f);
			indentlevel += 4;
			indent(f);
		}
		switch(tp->tp_fund) {
		case T_OBJECT:
			gen_compl_ass(nd);
			break;
		case T_ARRAY:
		case T_BAG:
		case T_SET:
		case T_GRAPH:
		case T_RECORD:
		case T_UNION:
		case T_GENERIC:
			if ((expr->nd_class & (Aggr|Row)) ||
			    ((tp->tp_flags & T_DYNAMIC) &&
			     (expr->nd_class != Tmp ||
			      expr->nd_tmpvar->tmp_ispointer))) {
				gen_compl_ass(nd);
				break;
			}
			/* fall through */
		default:
			if (tp->tp_flags & T_DYNAMIC) {
				if (tp->tp_fund == T_GENERIC) {
					fprintf(fc, "\n#if dynamic_%s\n",
						tp->tp_tpdef);
					indent(f);
				}
				fprintf(f, "%s(", tp->tp_freefunc);
				gen_addr(f, nd->nd_desig);
				fputs(");\n", f);
				if (tp->tp_fund == T_GENERIC) {
					fputs("#endif\n", f);
				}
				indent(f);
			}
			gen_expr_st(f, nd->nd_desig);
			fputs(" = ", f);
			gen_expr_ld(f, expr);
			fputs(";\n", f);
			if (tp->tp_flags & T_DYNAMIC) {
				assert(expr->nd_class == Tmp);
				assert(! (expr->nd_flags & ND_RETVAR));

				indent(f);
				fprintf(f, "%s(&", tp->tp_init);
				fprintf(f, TMP_FMT, expr->nd_tmpvar->tmp_id);
				fputs(", (char *) 0);\n", f);
			}
			break;
		}
		if (nd->nd_symb == ALIASBECOMES) {
			indentlevel -= 4;
			indent(f);
			fputs("}\n", f);
		}
		if (nd->nd_symb == RETURN) {
			indent(f);
			fprintf(f, "goto %s;\n", return_label);
			retlab_used = 1;
		}
		break;
		}

	case ORBECOMES:
	case ANDBECOMES:
		if (! node_emptylist(nd->nd_list1)) {
			assert(! nd->nd_expr);
			fprintf(f, "if (%c",
				nd->nd_symb == ANDBECOMES ? ' ' : '!');
			gen_expr_stld(f, nd->nd_desig);
			fputs(") {\n", f);
			gen_statlist(nd->nd_list1, 0);
			indent(f);
			fputs("}\n", f);
		}
		else {
			assert(nd->nd_expr);
			gen_expr_st(f, nd->nd_desig);
			fputs(" = ", f);
			gen_expr_ld(f, nd->nd_desig);
			fputs(c_sym(nd->nd_symb), f);
			gen_expr_ld(f, nd->nd_expr);
			fputs(";\n", f);
		}
		break;

	case MODBECOMES:
	case DIVBECOMES:
		/* Special treatment for / and % for integers.
		   No straightforward translation to the C operators / and %
		   because we don't know how they behave.
		*/
		if (nd->nd_expr->nd_type->tp_fund == T_INTEGER) {
			gen_expr_st(f, nd->nd_desig);
			fprintf(f, " = m_%s",
				nd->nd_symb == DIVBECOMES ? "div" : "mod");
			if (nd->nd_desig->nd_type == longint_type) putc('l', f);
			putc('(', f);
			gen_expr_ld(f, nd->nd_desig);
			putc(',', f);
			gen_expr_ld(f, nd->nd_expr);
			fputs(");\n", f);
			break;
		}
		/* fall through */
	case PLUSBECOMES:
	case MINBECOMES:
	case TIMESBECOMES:
	case B_ORBECOMES:
	case B_ANDBECOMES:
	case B_XORBECOMES:
	case LSHBECOMES:
	case RSHBECOMES:
		if (nd->nd_expr->nd_type->tp_fund & (T_INTEGER|T_REAL)) {
			gen_expr_stld(f, nd->nd_desig);
			fprintf(f, " %s= ", c_sym(nd->nd_symb));
			gen_expr_ld(f, nd->nd_expr);
			fputs(";\n", f);
			break;
		}
		fprintf(f, "%c_%s(",
			base_char_of(nd->nd_expr->nd_type),
			nd->nd_symb == PLUSBECOMES ? "add"
			  : nd->nd_symb == MINBECOMES ? "sub"
			  : nd->nd_symb == TIMESBECOMES ? "inter"
			  : "symdiff");
		gen_addr_stld(f, nd->nd_desig);
		fputs(", ", f);
		gen_addr_ld(f, nd->nd_expr);
		fprintf(f, ", &%s);\n", nd->nd_expr->nd_type->tp_descr);
		break;

	case GUARD:
		gen_guard(nd);
		break;

	case FOR_UPDATE:
		if (options['n'] &&
		    nd->nd_expr->nd_class == Link &&
		    (nd->nd_expr->nd_flags & ND_FORDONE) &&
		    (unsigned) (nd->nd_expr->nd_count) < UNROLL_THRESHOLD) {
			gen_expr(f, nd->nd_desig);
			fputs("++;\n", f);
			break;
		}
		fputs("if (", f);
		if (nd->nd_expr->nd_class == Link) {
			gen_expr(f, nd->nd_desig);
			fputs(" == ", f);
			gen_expr(f, nd->nd_expr->nd_right);
			fputs(") break;\n", f);
			indent(f);
			gen_expr(f, nd->nd_desig);
			fputs("++;\n", f);
		}
		else {
			fprintf(f,
				"%c_size(",
				nd->nd_expr->nd_type->tp_fund == T_SET ?
					's' : 'b');
			gen_addr(f, nd->nd_expr);
			fputs(") == 0) break;\n", f);
		}
		break;

	case UPDATE:
		fprintf(f, TMP_FMT, nd->nd_desig->nd_tmpvar->tmp_id);
		if (nd->nd_expr) {
			fputs(" += ", f);
			gen_expr_ld(f, nd->nd_expr);
		}
		else	fputs("++", f);
		fputs(";\n", f);
		break;

	case ACCESS:
		{ int	i;
		  p_node
			n = nd->nd_list1;

		  for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			fprintf(fc, "dest[%d] = ", i);
			assert(n != 0);
			gen_expr(fc, n);
			fputs(";\n", fc);
			n = node_nextlistel(n);
			indent(fc);
		  }
		  fputs("pdest = partition(instance, dest);\n", fc);
		  indent(fc);
		  fputs("if (pdest != psource && pdest >= 0) add_edge(pdg, psource, state->owner[psource], pdest, state->owner[pdest]);\n", fc);
		}
		break;

	case 0:
		break;
	default:
		crash("gen_Stat");
	}
	level--;
}

extern int possibly_blocking;

static void
gen_statlist(statlist, gen_rts_call)
	p_node	statlist;
	int	gen_rts_call;
{
	p_node	l;
	p_node	nd;
	FILE	*f = fc;

	indentlevel += 4;
	if (gen_rts_call && ! options['p']) {
		indent(f);
		fputs("m_rts();\n", f);
	}
	node_walklist(statlist, l, nd) {
		if (! nd) continue;
		lineno = nd->nd_pos.pos_lineno;
		if (nd->nd_symb != 0) indent(f);
		possibly_blocking = 0;
		switch(nd->nd_class) {
		case Stat:
			gen_Stat(nd);
			break;
		case Call:
			gen_expr(f, nd);
			fputs(";\n", f);
			break;
		default:
			crash("gen_statlist");
		}
		if (possibly_blocking) {
			possibly_blocking = 0;
			if (could_be_called_from_operation(func_def)) {
				indent(f);
				fprintf(f,
					"if (*op_flags & BLOCKING) goto %s;\n",
					return_label);
				retlab_used = 1;
			}
		}
	}
	indentlevel -= 4;
}

int
could_be_called_from_operation(df)
	t_def	*df;
{
	/* Check if the current function/process/operation could be
	   called from within an operation.
	   If it is a process, it can not;
	   If it is an operation or object initialization, it can.
	   If it is a function, it can if the function is exported,
	   or it is called from an exported function,
	   and it can if it is not, but the current translation unit
	   is an object.
	*/

	t_dflst	l;
	t_def	*d;

	if (df->df_kind == D_PROCESS) return 0;
	if (df->df_kind & (D_OPERATION|D_OBJECT)) return 1;
	assert(df->df_kind == D_FUNCTION);
	if (df->df_flags & D_EXPORTED) return 1;
	def_walklist(CurrDef->mod_funcaddrs, l, d) {
		if (d == df) return 1;
	}
	if (df->df_scope->sc_definedby->df_kind == D_OBJECT) return 1;
	/* For the time being, we assume that df could be called from an
	   exported function if the module has an exported function.
	*/
	df = df->df_scope->sc_definedby;
	while (df) {
		if (df->df_kind == D_FUNCTION
		    && (df->df_flags & D_EXPORTED)) return 1;
		df = df->df_nextinscope;
	}
	return 0;
}

static void
gen_cases(cslist)
	p_node	cslist;
{
	p_node	l;
	p_node	nd;
	int	first = 1;
	FILE	*f = fc;

	node_walklist(cslist, l, nd) {
		if (nd->nd_symb == UPTO) {
			long i;

			for (i = nd->nd_left->nd_int;
			     i <= nd->nd_right->nd_int;
			     i++) {
				if (! first) indent(f);
				fprintf(f, "case %ld:\n", i);
				first = 0;

			}
		}
		else {
			if (! first) indent(f);
			fprintf(f, "case %ld:\n", nd->nd_int);
			first = 0;
		}
	}
}

static void
gen_casestat(nd)
	p_node	nd;
{
	p_node	l;
	p_node	n;
	FILE	*f = fc;

	if (! nd->nd_list1) return;
	node_walklist(nd->nd_list1, l, n) {
		p_node	c;
		p_node	cn;

		assert(n->nd_symb == ARROW);
		if (! (n->nd_flags & ND_LARGE_RANGE)) {
			l = nd->nd_list1;
			while (node_getlistel(l) != n) {
				l = node_nextlistel(l);
			}
			break;
		}
		if (n != node_getlistel(nd->nd_list1)) {
			fputs("else ", f);
		}
		fputs("if (", f);
		node_walklist(n->nd_list1, c, cn) {
			if (c != node_nextlistel(n->nd_list1)) {
				fputs(" || ", f);
			}
			if (cn->nd_symb == UPTO) {
				putc('(', f);
				gen_expr_ld(f, nd->nd_expr);
				fprintf(f, " >= %ld && ", cn->nd_left->nd_int);
				gen_expr_ld(f, nd->nd_expr);
				fprintf(f, " <= %ld)", cn->nd_right->nd_int);
			}
			else {
				gen_expr_ld(f, nd->nd_expr);
				fprintf(f, " == %ld", cn->nd_int);
			}
		}
		fputs(") {\n", f);
		gen_statlist(n->nd_list2, 0);
		indent(f);
		fputs("}\n", f);
		indent(f);
	}
	if (node_emptylist(l)) {
		fputs("else {\n", f);
		if (nd->nd_flags & ND_HAS_ELSEPART) {
			gen_statlist(nd->nd_list2, 0);
		}
		else {
			indent(f);
			fprintf(f, "    m_trap(CASE_ERROR, %s, %d);\n",
				CurrDef->mod_fn, nd->nd_pos.pos_lineno);
		}
		indent(f);
		putc('}', f);
		return;
	}
	if (l != nd->nd_list1) fputs("else ", f);
	fputs("switch(", f);
	gen_expr_ld(f, nd->nd_expr);
	fputs(") {\n", f);
	gen_statlist(l, 0);
	indent(f);
	fputs("default:\n", f);
	if (nd->nd_flags & ND_HAS_ELSEPART) {
		gen_statlist(nd->nd_list2, 0);
	}
	else {
		indent(f);
		fprintf(f, "    m_trap(CASE_ERROR, %s, %d);\n",
			CurrDef->mod_fn, nd->nd_pos.pos_lineno);
	}
	indent(f);
	fputs("    break;\n", f);
	indent(f);
	fputs("}\n", f);
}

static int	offset;
static t_type	*eltype;
static p_node	desig;

static void
gen_arrelem()
{
	fprintf(fc, "((%s *)", eltype->tp_tpdef);
	gen_addr(fc, desig);
	fprintf(fc, "->a_data)[%d]", offset++);
}

static int
gen_assignmemaggr(nd)
	p_node	nd;
{
	t_type	*tp = eltype;
	FILE	*f = fc;
	p_node	l;

	assert(nd->nd_symb == '[');
	l = node_getlistel(nd->nd_memlist);
	if (l && l->nd_class == Row) return 0;

	node_walklist(nd->nd_memlist, l, nd) {
		indent(f);
		if (tp->tp_flags & T_DYNAMIC) {
			fputs("memset(&", f);
			gen_arrelem();
			offset--;
			fprintf(fc, ", '\\0', sizeof(%s));\n",
				tp->tp_tpdef);
			indent(f);
			fprintf(f, "%s(&", tp->tp_assignfunc);
			gen_arrelem();
			fputs(", ", f);
			gen_addr(f, nd);
			fputs(");\n", f);
		}
		else {
			gen_arrelem();
			fputs(" = ", f);
			gen_expr_ld(f, nd);
			fputs(";\n", f);
		}
	}
	return 1;
}

/*
void
gen_expr_ld(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_expr(f, nd);
}

void
gen_expr_st(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_expr(f, nd);
}

void
gen_expr_stld(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_expr(f, nd);
}

void
gen_addr_st(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_addr(f, nd);
}

void
gen_addr_stld(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_addr(f, nd);
}

void
gen_addr_ld(f, nd)
	FILE	*f;
	p_node	nd;
{
	gen_addr(f, nd);
}
*/

static int
designates_obj_field(nd)
	p_node	nd;
{
	if (! nd) return 0;
	switch(nd->nd_class) {
	case Ofldsel:
	case Select:
	case Arrsel:
		return designates_obj_field(nd->nd_left);
	case Def:
		return nd->nd_def->df_kind == D_OFIELD;
	case Tmp:
		if (nd->nd_tmpvar && nd->nd_tmpvar->tmp_ispointer) {
			return designates_obj_field(nd->nd_tmpvar->tmp_expr);
		}
		break;
	}
	return 0;
}

static void
gen_compl_ass(nd)
	p_node	nd;
{
	t_type	*tp = nd->nd_expr->nd_type;
	FILE	*f = fc;

	if (nd->nd_expr->nd_class & (Aggr|Row)) {
		int	i;
		t_def	*df;
		p_node	n = nd->nd_expr;
		p_node	l = n->nd_memlist;

		if (tp->tp_flags & T_DYNAMIC) {
			if (tp->tp_fund == T_GENERIC) {
				fprintf(fc, "#if dynamic_%s\n",
					tp->tp_tpdef);
				indent(f);
			}
			fprintf(f, "%s(", tp->tp_freefunc);
			gen_addr(f, nd->nd_desig);
			fputs(");\n", f);
			if (tp->tp_fund == T_GENERIC) {
				fputs("#endif\n", f);
			}
			indent(f);
		}
		switch(tp->tp_fund) {
		case T_ARRAY:
			if (node_emptylist(l)) break;
			offset = 0;
			eltype = element_type_of(tp);
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fputs("a_allocate(", f);
			    gen_addr(f, nd->nd_desig);
			    fprintf(f, ", %d, sizeof(%s)",
				tp->arr_ndim,
				eltype->tp_tpdef);
			    for (i = 0; i < tp->arr_ndim; i++) {
				offset *= n->nd_nelements;
				if (tp->arr_index(i)->tp_fund == T_ENUM) {
					fprintf(f, ", 0, %d", n->nd_nelements-1);
				}
				else {
					offset++;
					fprintf(f, ", 1, %d", n->nd_nelements);
				}
				l = n->nd_memlist;
				n = node_getlistel(l);
			    }
			    fputs(");\n", f);
			}
			desig = nd->nd_desig;
			visit_node(nd, Aggr|Row, gen_assignmemaggr, 0);
			break;
		case T_BAG:
		case T_SET:
			fprintf(f, "%c_initialize(",
				tp->tp_fund == T_BAG ? 'b' : 's');
			gen_addr(f, nd->nd_desig);
			fputs(", (char *) 0);\n", f);
			node_walklist(l, l, n) {
				indent(f);
				fprintf(f, "%c_addel(",
					tp->tp_fund == T_BAG ? 'b' : 's');
				gen_addr(f, nd->nd_desig);
				fprintf(f, ", &%s, ", tp->tp_descr);
				gen_addr(f, n);
				fputs(");\n", f);
			}
			break;
		case T_RECORD:
			df = tp->rec_scope->sc_def;
			node_walklist(l, l, n) {
				if (df->df_type->tp_flags & T_DYNAMIC) {
					fprintf(f, "%s(&(",
						df->df_type->tp_assignfunc);
					gen_expr(f, nd->nd_desig);
					fprintf(f, ".%s), ", df->df_name);
					gen_addr(f, n);
					fputs(");\n", f);
				}
				else {
					gen_expr(f, nd->nd_desig);
					fprintf(f, ".%s", df->df_name);
					fputs(" = ", f);
					gen_expr_ld(f, n);
					fputs(";\n", f);
				}
				df = df->df_nextinscope;
				if (df) indent(f);
			}
			break;
		case T_UNION:
			df = tp->rec_scope->sc_def;
			gen_expr(f, nd->nd_desig);
			fputs(".u_init", f);
			fputs(" = 1;\n", f);
			indent(f);
			gen_expr(f, nd->nd_desig);
			fprintf(f, ".%s", df->df_name);
			fputs(" = ", f);
			n = node_getlistel(l);
			l = node_nextlistel(l);
			gen_expr_ld(f, n);
			fputs(";\n", f);
			assert(n->nd_class == Value);
			do {
				df = df->df_nextinscope;
			} while (df->fld_tagvalue->nd_int != n->nd_int);
			n = node_getlistel(l);
			if (df->df_type->tp_flags & T_DYNAMIC) {
				fprintf(f, "%s(&(",
					df->df_type->tp_assignfunc);
				gen_expr(f, nd->nd_desig);
				fprintf(f, ".u_el.%s), ", df->df_name);
				gen_addr(f, n);
				fputs(");\n", f);
			}
			else {
				gen_expr(f, nd->nd_desig);
				fprintf(f, ".u_el.%s", df->df_name);
				fputs(" = ", f);
				gen_expr_ld(f, n);
				fputs(";\n", f);
			}
			break;
		}
		return;
	}
	/* Attempt to optimize assignment to v__result.
	   Basically, we optimize if the code sais:
		RETURN <variable>,
	   and the contents of the variable can be deleted after doing the
	   assignment.
	*/
	if ((nd->nd_desig->nd_flags & ND_RETVAR) &&
	    nd->nd_expr->nd_class == Def &&
	    nd->nd_expr->nd_def->df_kind == D_VARIABLE &&
	    (! is_parameter(nd->nd_expr->nd_def) ||
	     (nd->nd_expr->nd_def->df_flags & D_COPY)) &&
	    tp->tp_fund != T_GENERIC &&
	    ! (tp->tp_flags & T_HASOBJ)) {
		fprintf(f, "%s(", tp->tp_freefunc);
		gen_addr(f, nd->nd_desig);
		fputs(");\n", f);
		indent(f);
		fputs("*v__result = ", f);
		gen_expr(f, nd->nd_expr);
		fputs(";\n", f);
		indent(f);
		fprintf(f, "(void) memset((void *) &%s, 0, (size_t) sizeof(%s));\n",
			nd->nd_expr->nd_def->df_name,
			tp->tp_tpdef);
		return;
	}
	fprintf(f, "%s(", tp->tp_assignfunc);
	gen_addr(f, nd->nd_desig);
	fputs(", ", f);
	gen_addr(f, nd->nd_expr);
	fputs(");\n", f);
}

static void
gen_free(df)
	t_def	*df;
{
	FILE	*f = fc;

	if (df->df_type->tp_fund == T_GENERIC) {
		fprintf(f, "#if dynamic_%s\n", df->df_type->tp_tpdef);
	}
	indent(f);
	fprintf(f, "%s(%s%s);\n",
		df->df_type->tp_freefunc,
		(is_in_param(df) && ! (df->df_flags & D_COPY)) ? "" : "&",
		df->df_name);
	if (df->df_type->tp_fund == T_GENERIC) {
		fputs("#endif\n", f);
	}
}

static void
obj_copy(tp)
	t_type	*tp;
{
	indent(fc);
	if (tp->tp_flags & T_DYNAMIC) {
		fprintf(fc, "memset(&sav_obj, '\\0', sizeof(%s));\n",
			tp->tp_tpdef);
		indent(fc);
		fprintf(fc,
			"%s(&sav_obj, v__obj->o_fields);\n",
			tp->tp_assignfunc);
	}
	else {
		fputs("sav_obj = ", fc);
		fprintf(fc,
			"*((%s *) (v__obj->o_fields));\n",
			tp->tp_tpdef);
	}
}

static void
objsav_copy(tp)
	t_type	*tp;
{
	indent(fc);
	fprintf(fc,
		"%s(v__obj->o_fields, &sav_obj);\n",
		tp->tp_assignfunc);
}

static void
gen_guard(nd)
	p_node	nd;
{
	t_def	*df;
	char	*sv_retlab = return_label;
	int	sv_rused = retlab_used;
	FILE	*f = fc;
	t_type	*rctype = record_type_of(func_def->df_type->prc_objtype);

	fputs("if (", f);
	if (nd->nd_expr->nd_flags & ND_BLOCKS) {
		fputs("! (*op_flags & BLOCKING) && ", fc);
	}
	gen_expr_ld(f, nd->nd_expr);
	fputs(") {\n", f);
	if (nd->nd_flags & ND_BLOCKS) {
		return_label = gen_label();
		retlab_used = 0;
	}
	gen_statlist(nd->nd_list1, 0);
	if (result_type_of(func_def->df_type) != 0) {
		/* ??? only when it is possible to fall through ??? */
		indent(f);
		fprintf(fc, "    m_trap(FALL_THROUGH, %s, %d);\n",
			CurrDef->mod_fn, lineno);
	}
	if (nd->nd_flags & ND_BLOCKS) {
		if (retlab_used) {
			fprintf(f, "%s:;\n", return_label);
		}
		free(return_label);
		return_label = sv_retlab;
		retlab_used = sv_rused;
		indent(f);
		fputs("    if (! (*op_flags & BLOCKING)) {\n", f);
		indent(f);
		fputs("        goto retlab;\n", f);
		gretlab_used = 1;
		indent(f);
		fputs("    }\n", f);
		/* restore object here if body writes it but guard does not. */
		if ((nd->nd_flags & ND_WRITES) &&
		    ! (nd->nd_expr->nd_flags & ND_WRITES)) {
			objsav_copy(rctype);
		}
		indent(f);
	}
	else {
		indent(f);
		fputs("    goto retlab;\n", f);
		gretlab_used = 1;
	}
	indent(f);
	fputs("}\n", f);
	if (nd->nd_expr->nd_flags & ND_WRITES) {
		/* Guard writes object. Restore. */
		objsav_copy(rctype);
	}
	if (! (nd->nd_flags & ND_LASTGUARD)) {
		/* Re-initialize variables. */
		/* For now implemented by freeing locals and then
		   reinitializing.
		*/
		indent(f);
		fputs("*op_flags &= ~BLOCKING;\n", f);
		for (df = func_def->bod_scope->sc_def;
		     df;
		     df = df->df_nextinscope) {
			if (df->df_kind == D_VARIABLE
			    && (df->df_type->tp_flags & T_DYNAMIC)
			    && (! is_in_param(df) || (df->df_flags & D_COPY))) {
				gen_free(df);
			}
		}
		gen_statlist(func_def->bod_init, 0);
	}
	else {
		for (df = func_def->bod_scope->sc_def;
		     df;
		     df = df->df_nextinscope) {
			if (df->df_kind == D_VARIABLE
			    && (df->df_type->tp_flags & T_DYNAMIC)
			    && is_out_param(df)) {
				gen_free(df);
			}
		}
		indent(f);
		fputs("*op_flags |= BLOCKING;\n", f);
		indent(f);
		fputs("goto blocking_oper;\n", f);
	}
}

static void
gen_initvar(df)
	t_def	*df;
{
	FILE	*f = fc;

	if (df->df_kind == D_OFIELD) {
		char *buf = mk_str("&(v__ofldp->",
				   df->df_name,
				   ")",
				   (char *) 0);
		char *s = gen_trace_name(df);
		if (df->fld_bat) {
			gen_init_with_bat(buf, df->df_type, f, df->fld_bat, s, "\n");
			putc('\n', f);
		}
		else {
			fprintf(f, "%s(%s, %s);\n",
				df->df_type->tp_init,
				buf,
				s);
		}
		free(s);
		free(buf);
		return;
	}
	assert(df->df_kind == D_VARIABLE);
	if ((df->df_flags & D_COPY)) {
		if (df->df_type->tp_fund != T_GENERIC
		    && ! (df->df_type->tp_flags & T_DYNAMIC)) {
			fprintf(f, "%s = ", df->df_name);
			if (is_constructed_type(df->df_type)) {
				putc('*', f);
			}
			fprintf(f, "o%s;\n", df->df_name);
			return;
		}
		fprintf(f, "memset(&%s, '\\0', sizeof(%s)); ",
			df->df_name, df->df_type->tp_tpdef);
		fprintf(f, "%s(&%s, ",
			df->df_type->tp_assignfunc, df->df_name);
		if (df->df_type->tp_fund == T_GENERIC) {
			fprintf(f, "ch%s(&,NOTHING)", df->df_type->tp_tpdef);
		}
		fprintf(f, "o%s);\n", df->df_name);
	}
	else if (df->var_bat) {
		char *buf = mk_str("&", df->df_name, (char *) 0);
		char *s = gen_trace_name(df);
		gen_init_with_bat(buf, df->df_type, f, df->var_bat, s, "\n");
		putc('\n', f);
		free(buf);
		free(s);
	}
	else if (is_out_param(df)) {
		if (! is_constructed_type(df->df_type)) {
			fprintf(f, "%s = 0;\n", df->df_name);
		}
		else if (! df->df_type->tp_init) {
#ifdef __NOTDEF__
			fprintf(f, "(void) memset((void *) &%s, 0, (size_t) sizeof(%s));\n",
				df->df_name,
				df->df_type->tp_tpdef);
#endif
		}
		else {
			char	*s = gen_trace_name(df);
			fprintf(f, "%s(&%s, %s);\n",
				df->df_type->tp_init,
				df->df_name,
				s);
			free(s);
		}
	}
	else if (! is_in_param(df)) {
		if (! is_constructed_type(df->df_type)) {
			fprintf(f, "%s = 0;\n", df->df_name);
		}
		else {
			char	*s = gen_trace_name(df);
			fprintf(f, "%s(&%s, %s);\n",
				df->df_type->tp_init,
				df->df_name,
				s);
			free(s);
		}
	}
	else {
		fprintf(f, "/* no initialization or copy required for %s */\n",
			df->df_name);
	}
}

static void
gen_forstat(nd)
	p_node	nd;
{
	p_node	e = nd->nd_expr;
	int	setch = e->nd_class != Link
			&& e->nd_type->tp_fund == T_SET
				? 's' : 'b';
	FILE	*f = fc;

	if (! (nd->nd_flags & ND_FORDONE)) {
		fputs("if (", f);
		if (e->nd_class == Link) {
			gen_expr_ld(f, nd->nd_desig);
			fputs(" <= ", f);
			gen_expr_ld(f, e->nd_right);
			putc(')', f);
		}
		else {
			fprintf(f, "%c_size(", setch);
			gen_addr(f, e);
			fputs(") != 0)", f);
		}
		fputs(" {\n", f);
		gen_statlist(nd->nd_list2, 0);
		indentlevel += 4;
		indent(f);
	}
	else {
		if (nd->nd_list2) {
			fputs("\n", f);
			indentlevel -= 4;
			gen_statlist(nd->nd_list2, 0);
			indentlevel += 4;
			indent(f);
		}
	}
	if (options['n'] &&
	    e->nd_class == Link &&
	    (nd->nd_flags & ND_FORDONE) &&
	    (unsigned)(e->nd_count) < UNROLL_THRESHOLD) {
		int cnt = e->nd_count;
		while (cnt-- > 0) {
			gen_statlist(nd->nd_list1, 1);
		}
	}
	else {
		fputs("for (;;) {\n", f);
		if (e->nd_class != Link) {
			indent(f);
			fprintf(f, "    %c_from(", setch);
			gen_addr(f, e);
			fprintf(f, ", &%s, ", e->nd_type->tp_descr);
			gen_addr(f, nd->nd_desig);
			fputs(");\n", f);
		}
		gen_statlist(nd->nd_list1, 1);
		indent(f);
		fputs("}\n", f);
		if (! (nd->nd_flags & ND_FORDONE)) {
			indentlevel -= 4;
			indent(f);
			fputs("}\n", f);
		}
	}
	if (exitlabel) {
		fprintf(f, "%s:;\n", exitlabel);
		free(exitlabel);
	}
	if (e->nd_class != Link) {
		indent(f);
		fprintf(f, "%s(", e->nd_type->tp_freefunc);
		gen_addr(f, e);
		fputs(");\n", f);
	}
}

static void
gen_oper_params(dl)
	t_dflst	dl;
{
	t_def	*df;
	int	cnt = 0;

	def_walklist(dl, dl, df) {
		t_type *tp = df->df_type;
		if (cnt) fputs(", ", fc);
		if (! is_in_param(df)) {
			fprintf(fc, "v__args[%d]", cnt);
		}
		else if (tp->tp_fund == T_GENERIC) {
			fprintf(fc, "ch%s(*((%s *) v__args[%d]),v__args[%d])",
				tp->tp_tpdef,
				tp->tp_tpdef,
				cnt,
				cnt);
		}
		else if (is_constructed_type(tp)) {
			fprintf(fc, "v__args[%d]", cnt);
		}
		else {
			fprintf(fc, "*((%s *) v__args[%d])",
				tp->tp_tpdef,
				cnt);
		}
		cnt++;
	}
}

char *
gen_process_wrapper(df)
	p_def	df;
{
	char	*prcfnc = gen_name("pf_", df->df_idf->id_text, 0);
	FILE	*f = (df->df_flags & D_EXPORTED) ? fh : fc;

	gen_prototype(f, df, df->df_name, (t_type *) 0);
	fputs(";\n", f);
	fprintf(fc, "static void %s(void **v__args) {\n  %s(", prcfnc, df->df_name);
	gen_oper_params(df->df_type->prc_params);
	fputs(");\n}\n", fc);
	return prcfnc;
}

/* Code generation for partitioned objects ... */

_PROTOTYPE(static void gen_objarray, (t_def *));
_PROTOTYPE(static int pdg_edge, (t_node *));
_PROTOTYPE(static void process_gather, (t_def *));

static void
process_gather(df)
	t_def	*df;
{
	t_def	*d;
	int	i;
	t_dflst	l;
	t_def	*dnames = df->bod_scope->sc_def;
	t_def	*dn;
	while (!(dnames->df_flags & D_PART_INDEX)) {
		dnames = dnames->df_nextinscope;
	}
	def_walklist(df->df_type->prc_params, l, d) {
		if (d->df_flags & D_GATHERED) {
			dn = dnames;
			indent(fc);
			fprintf(fc, "%s *%s = &o%s[",
				element_type_of(d->df_type)->tp_tpdef,
				d->df_name, d->df_name);
			for (i = 1; i < CurrDef->df_type->arr_ndim; i++) {
				putc('(', fc);
			}
			for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
				if (i > 0) {
					fprintf(fc, ") * len%d + ", i);
				}
				if (i < CurrDef->df_type->arr_ndim-1) {
					fprintf(fc, "%s-ss%d", dn->df_name, i);
					dn = dn->df_nextinscope;
				}
				else {
					fprintf(fc, "ps%d-ss%d", i, i);
				}
			}
			fputs("];\n", fc);
		}
	}
	if (result_type_of(df->df_type) && ! df->opr_reducef) {
		/* Result is gathered as well */
		dn = dnames;
		indent(fc);
		fprintf(fc, "%s *v__result = &ov__result[",
			element_type_of(result_type_of(df->df_type))->tp_tpdef);
		for (i = 1; i < CurrDef->df_type->arr_ndim; i++) {
			putc('(', fc);
		}
		for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			if (i > 0) {
				fprintf(fc, ") * len%d + ", i);
			}
			if (i < CurrDef->df_type->arr_ndim-1) {
				fprintf(fc, "%s-ss%d", dn->df_name, i);
				dn = dn->df_nextinscope;
			}
			else {
				fprintf(fc, "ps%d-ss%d", i, i);
			}
		}
		fputs("];\n", fc);
	}
}

static void gen_part_func(df)
	t_def	*df;
{
	/* Produce code for operation df on a partitioned object. */

	p_node	list = df->bod_statlist1;
	t_body	*b = df->df_body;
	t_def	*d;
	int	i;
	t_dflst	l;

	if (df->df_flags & D_HASWRITES) {
		list = df->bod_statlist2;
	}
	gen_part_prot(fc, df, df->opr_namer, 0);
	fputs(" {\n", fc);
	if (node_emptylist(list) && node_emptylist(b->init)) {
		fputs("}\n", fc);
		return;
	}

	if (df->df_flags & D_PARALLEL) {
		fputs("    partition_p part = instance->state->partition[part_num];\n", fc);
		fprintf(fc, "    %s *v__ofldp = part->elements;\n",
			record_type_of(CurrDef->df_type)->tp_tpdef);
		for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			fprintf(fc, "    int ps%d = part->start[%d];\n", i, i);
			fprintf(fc, "    int pe%d = part->end[%d];\n", i, i);
		}
	}
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
	    fprintf(fc, "    int ss%d = instance->state->start[%d];\n", i, i);
	    fprintf(fc, "    int se%d = instance->state->end[%d];\n", i, i);
	    if (i > 0) {
		fprintf(fc, "    int len%d = instance->state->length[%d];\n", i, i);
	    }
	}

	if (df->df_flags & D_HAS_OFLDSEL) {
		fprintf(fc, "    t_array *val = instance->state->val;\n");
		fprintf(fc, "    %s *datap = val->a_data;\n",
			record_type_of(CurrDef->df_type)->tp_tpdef);
	}

	declarations(df, list);

	if (df->df_flags & D_REDUCED) {
		fputs("    if (*first) { *first = 0;\n", fc);
		def_walklist(df->df_type->prc_params, l, d) {
			if (d->df_flags & D_REDUCED) {
				fprintf(fc, "        p%s = o%s;\n",
					d->df_name, d->df_name);
			}
		}
		if (df->opr_reducef) {
			fputs("        v__result = ov__result;\n", fc);
		}
		fputs("    }\n", fc);
	}

	gen_statlist(b->init, 0);
	indentlevel = 0;

	if (df->df_flags & D_PARALLEL) {
		d = df->bod_scope->sc_def;
		for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			while (!(d->df_flags & D_PART_INDEX)) {
				d = d->df_nextinscope;
			}
			indentlevel += 4;
			if ((df->df_flags & D_GATHERED) &&
			    i == CurrDef->df_type->arr_ndim-1) {
				process_gather(df);
			}
			indent(fc);
			fprintf(fc, "for (%s = ps%d; %s <= pe%d; %s++) {\n",
				d->df_name, i, d->df_name, i, d->df_name);
			d = d->df_nextinscope;
		}
	}

	retlab_used = 0;
	gen_statlist(list, 0);

	if (result_type_of(df->df_type) != 0) {
		indent(fc);
		fprintf(fc, "    m_trap(FALL_THROUGH, %s, %d);\n",
			CurrDef->mod_fn, lineno);
	}

	if (retlab_used) fputs("retlab:\n", fc);

	if (df->df_flags & D_PARALLEL) {
		int	i;

		indent(fc);
		fputs("    v__ofldp++;\n", fc);
		if (df->df_flags & D_GATHERED) {
		    def_walklist(df->df_type->prc_params, l, d) {
			if (d->df_flags & D_GATHERED) {
				indent(fc);
				fprintf(fc, "    %s++;\n",
					d->df_name);

			}
		    }
		    if (result_type_of(df->df_type) && ! df->opr_reducef) {
			indent(fc);
			fputs("    v__result++;\n", fc);
		    }
		}
		if (df->df_flags & D_REDUCED) {
		    int first = 1;
		    indent(fc);
		    def_walklist(df->df_type->prc_params, l, d) {
			if (d->df_flags & D_REDUCED) {
		    		if (first) {
					fprintf(fc, "    if (o%s != p%s) {\n",
						d->df_name,
						d->df_name);
					first = 0;
				}
				indent(fc);
				fprintf(fc, "        %s(o%s, p%s);\n",
					d->var_reducef->df_name,
					d->df_name,
					d->df_name);
			}
		    }
		    if (df->opr_reducef) {
			if (first) {
				fputs("    if (ov__result != v__result) {\n", fc);
			}
			indent(fc);
			fprintf(fc,"        %s(ov__result, v__result);\n", df->opr_reducef->df_name);
		    }
		    indent(fc);
		    fputs("    } else {\n", fc);
		    def_walklist(df->df_type->prc_params, l, d) {
			if (d->df_flags & D_REDUCED) {
				indent(fc);
				fprintf(fc, "        p%s = o%s;\n",
					d->df_name, d->df_name);
			}
		    }
		    if (df->opr_reducef) {
			indent(fc);
			fputs("        v__result = &Xv__result;\n", fc);
		    }
		    indent(fc);
		    fputs("    }\n", fc);
		}
		for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
			indent(fc);
			indentlevel -= 4;
			fputs("}\n", fc);
		}
	}
	else copy_shared_or_out_params(df);

	cleanup_declarations(df, list);

	fputs("}\n", fc);
}


static void gen_part_prot(f, funcdf, nm, deps)
	FILE	*f;
	t_def	*funcdf;
	char	*nm;
	int	deps;
{
	/*	Generate a prototype for the function indicated by
		'funcdf', on file descriptor 'f'.
		This function can be used for declaration as well as
		definition, because no terminating ; is produced.
		The name to be used for the function is indicated by 'nm'.
		if 'ftp' is set, a function type is declared instead.
	*/

	t_def	*df = funcdf;
	t_type	*result_tp = result_type_of(df->df_type);
	t_dflst	l;

	assert(df->df_kind == D_OPERATION);

	if (deps) {
		fprintf(f, "static pdg_p %s(instance_p instance", nm);
	}
	else if (df->df_flags & D_PARALLEL) {
		fprintf(f, "static void %s(int part_num, instance_p instance", nm);
	}
	else {
		fprintf(f, "static void %s(int sender, instance_p instance", nm);
	}
	def_walklist(df->df_type->prc_params, l, df) {
		t_type *tp = df->df_type;
		fprintf(f, ", %s ", tp->tp_tpdef);
		if (is_in_param(df)) {
			if (tp->tp_fund == T_GENERIC) {
				fprintf(f, "ch%s(NOTHING,*)", tp->tp_tpdef);
			}
			else if (is_constructed_type(tp)) {
				putc('*', f);
			}
		}
		else putc('*', f);
		if (is_out_param(df) || (df->df_flags & D_COPY)) {
			putc('o', f);
		}
		fprintf(f, "%s", df->df_name);
	}
	if (result_tp && ! deps) {
		if ((funcdf->df_flags & D_PARALLEL) &&
		    ! (funcdf->opr_reducef)) {
			/* Result is gathered. Use element type. */
			fprintf(f, ", %s *ov__result",
				element_type_of(result_tp)->tp_tpdef);
		}
		else fprintf(f, ", %s *%cv__result",
			result_tp->tp_tpdef,
			funcdf->opr_reducef ? 'o' : ' ');
	}
	if (! deps && (funcdf->df_flags & D_REDUCED)) {
		fputs(", int *first", f);
	}
	putc(')', f);
}

static void gen_part_operation_wrapper(oper)
	t_def	*oper;
{
	/* Produce wrapper around operation on partitioned object. */

	t_type	*result = result_type_of(oper->df_type);

	gen_marshall(oper);
	if (oper->prc_funcno < 2) return;
	gen_part_prot(fc, oper, oper->opr_namer, 0);
	fputs(";\n", fc);
	if (oper->df_flags & D_PARALLEL) {
		fprintf(fc, "void %s(int part_num, instance_p instance, void **v__args) {\n", oper->opr_namew);
		fprintf(fc, "\t%s(part_num, instance", oper->opr_namer);
	}
	else {
		fprintf(fc, "void %s(int sender, instance_p instance, void **v__args) {\n", oper->opr_namew);
		fprintf(fc, "\t%s(sender, instance", oper->opr_namer);
	}
	if (! def_emptylist(oper->df_type->prc_params)) {
		fputs(", ", fc);
		gen_oper_params(oper->df_type->prc_params);
	}
	if (result) {
		fprintf(fc, ", (%s *) v__args[%d]",
			(oper->df_flags & D_PARALLEL) && ! oper->opr_reducef ?
				element_type_of(result)->tp_tpdef :
				result->tp_tpdef,
			oper->df_type->prc_nparams);
	}
	if (oper->df_flags & D_REDUCED) {
		fprintf(fc, ", (int *) v__args[%d]",
			oper->df_type->prc_nparams+1);
	}
	fputs(");\n}\n", fc);
	if ((oper->df_flags & (D_HAS_DEPENDENCIES|D_SIMPLE_DEPENDENCIES)) == D_HAS_DEPENDENCIES) {
		char	*s = gen_name("deps__", oper->df_idf->id_text, 0);
		gen_part_prot(fc, oper, oper->opr_depname, 1);
		fputs(";\n", fc);
		fprintf(fc, "pdg_p %s(instance_p instance, void **v__args) {\n",
			s);
		fprintf(fc, "\treturn %s(instance", oper->opr_depname);
		if (! def_emptylist(oper->df_type->prc_params)) {
			fputs(", ", fc);
			gen_oper_params(oper->df_type->prc_params);
		}
		fputs(");\n}\n", fc);
		free(s);
	}
}

static void
gen_objarray(df)
	t_def	*df;
{
	int	i;

	fprintf(fc, "    state->val = val = malloc(sizeof(ARRAY_TYPE(%d)));\n", df->df_type->arr_ndim);
	for (i = 0; i < df->df_type->arr_ndim; i++) {
		fprintf(fc, "    val->a_dims[%d].a_lwb = ss%d;\n", i, i);
		fprintf(fc, "    val->a_dims[%d].a_nel = se%d-ss%d+1;\n", i, i, i);
		if (i > 0) {
		    fprintf(fc, "    len%d = val->a_dims[%d].a_nel;\n", i, i);
		}
	}
	fprintf(fc, "    val->a_sz = state->total_size;\n");
	fputs("    val->a_offset = ", fc);
	for (i = 0; i < df->df_type->arr_ndim-1; i++) putc('(', fc);
	for (i = 0; i < df->df_type->arr_ndim-1; i++) {
		fprintf(fc, "val->a_dims[%d].a_lwb * val->a_dims[%d].a_nel) + ", i, i+1);
	}
	fprintf(fc, "val->a_dims[%d].a_lwb;\n", df->df_type->arr_ndim - 1);
	fprintf(fc, "    val->a_data = datap = (%s *)(state->data) - val->a_offset;\n", record_type_of(df->df_type)->tp_tpdef);
}

static void
gen_part_create(df)
	t_def	*df;
{
	/*	Creation and initialization of a partitioned object.
	*/
	int	i;
	t_def	*d;

	fprintf(fc, "void %s(instance_p *p", df->df_type->tp_batinit);
	for (i = 0; i < df->df_type->arr_ndim; i++) {
		fprintf(fc, ", int lb%d, int ub%d", i, i);
	}
	fputs(") {\n", fc);
	fprintf(fc, "    int length[%d];\n    int start[%d];\n",
		df->df_type->arr_ndim,
		df->df_type->arr_ndim);

	for (i = 0; i < df->df_type->arr_ndim; i++) {
		fprintf(fc, "    length[%d] = ub%d-lb%d+1;\n", i, i, i);
		fprintf(fc, "    start[%d] = lb%d;\n", i, i);
	}
	fprintf(fc, "    *p = do_new_instance(%s, length, start);\n",
		df->df_name);
	for (d = df->bod_scope->sc_def; d; d = d->df_nextinscope) {
		if (d->df_kind == D_OPERATION &&
		    (d->df_flags & D_PARALLEL) &&
		    (d->df_flags & D_HAS_COMPLEX_OFLDSEL) &&
		    ! (d->df_flags & D_HAS_DEPENDENCIES)) {
			fprintf(fc, "    do_change_attribute(*p, (po_opcode) %s, TRANSFER, t_collective_all);\n",
				d->opr_namew);
			fprintf(fc, "    do_change_attribute(*p, (po_opcode) %s, EXECUTION, e_parallel_consistent);\n",
				d->opr_namew);
		}
	}
	fputs("}\n", fc);

	/* Produce an initialization operation. */
	fprintf(fh, "void %s(int sender, instance_p instance, void **args);\n",
		df->mod_initname);
	fprintf(fc, "void %s(int sender, instance_p instance, void **args) {\n",
		df->mod_initname);
	fprintf(fc, "    %s *datap;\n", record_type_of(df->df_type)->tp_tpdef);
	fprintf(fc, "    t_array *val;\n");
	fprintf(fc, "    state_p state = instance->state;\n");
	for (i = 0; i < df->df_type->arr_ndim; i++) {
		fprintf(fc, "    int ss%d = state->start[%d];\n", i, i);
		fprintf(fc, "    int se%d = state->end[%d];\n", i, i);
		if (i > 0) {
			fprintf(fc, "    int len%d;\n", i);
		}
	}

	if (! node_emptylist(df->bod_init) ||
	    ! node_emptylist(df->bod_statlist1)) {
		declarations(df, df->bod_statlist1);
	}

	gen_objarray(df);

	gen_statlist(df->bod_init, 0);
	gen_statlist(df->bod_statlist1, 0);

	if (! node_emptylist(df->bod_init) ||
	    ! node_emptylist(df->bod_statlist1)) {
		cleanup_declarations(df, df->bod_statlist1);
	}
	fputs("}\n", fc);
}

static void
gen_pdg_func(df)
	t_def	*df;
{
	char	*s;
	int	i;
	t_def	*d;

	if (! (df->df_flags & D_HAS_DEPENDENCIES) &&
	    (df->df_flags & D_HAS_COMPLEX_OFLDSEL)) {
		return;
	}
	s = gen_name("pdg_", df->df_idf->id_text, 0);

	fprintf(fc, "pdg_p %s(instance_p instance) {\n", s);
	fprintf(fc, "    int dest[%d];\n", CurrDef->df_type->arr_ndim);
	for (d = df->bod_scope->sc_def; d; d = d->df_nextinscope) {
		if (d->df_flags & D_PART_INDEX) {
			fprintf(fc, "    int %s;\n", d->df_name);
		}
	}
	fputs("    int psource, p, pdest;\n"
	      "    state_p state = instance->state;\n"
	      "    pdg_p pdg = new_pdg(state->num_parts);\n",
	      fc);

	if (df->df_flags & D_HAS_DEPENDENCIES) {
		assert(df->opr_dependencies == NULL);
		fputs("    return pdg;\n}\n", fc);
		return;
	}
	fputs("    for (p = 0; p < state->num_parts; p++) {\n"
	      "        psource = state->partition[p]->part_num;\n",
	      fc);

	indentlevel = 4;

	d = df->bod_scope->sc_def;
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		while (!(d->df_flags & D_PART_INDEX)) {
			d = d->df_nextinscope;
		}
		indentlevel += 4;
		indent(fc);
		fprintf(fc, "for (%s = state->partition[p]->start[%d]; %s <= state->partition[p]->end[%d]; %s++) {\n",
			d->df_name, i, d->df_name, i, d->df_name);
		d = d->df_nextinscope;
	}

	indentlevel += 4;
	visit_ndlist((df->df_flags & D_HASWRITES) ? df->bod_statlist2 : df->bod_statlist1,
		Ofldsel|Tmp, pdg_edge, 0);
	indentlevel -= 4;

	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		indent(fc);
		indentlevel -= 4;
		fputs("}\n", fc);
	}

	fputs("    }\n", fc);

	free(s);
	fputs("    return pdg;\n", fc);
	fputs("}\n", fc);
}

static int
pdg_edge(nd)
	p_node	nd;
{
	int	i;

	if (nd->nd_class == Tmp) {
		if (! nd->nd_tmpvar) return 0;
		nd = nd->nd_tmpvar->tmp_expr;
		if (! nd || nd->nd_class != Ofldsel) {
			return 0;
		}
	}
	nd = nd->nd_right;
	no_temporaries = 1;
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		indent(fc);
		fprintf(fc, "dest[%d] = ", i);
		if (i < CurrDef->df_type->arr_ndim - 1) {
			p_node	n = nd;
			if (n->nd_class == Tmp) {
				n = n->nd_tmpvar->tmp_expr;
			}
			assert(n->nd_symb == '+');
			nd = n->nd_right;
			n = n->nd_left;
			if (n->nd_class == Tmp) {
				n = n->nd_tmpvar->tmp_expr;
			}
			assert(n->nd_symb == '*');
			assert(n->nd_right->nd_symb == ARR_SIZE);
			gen_expr(fc, n->nd_left);
		}
		else	gen_expr(fc, nd);
		fputs(";\n", fc);
	}
	indent(fc);
	fputs("pdest = partition(instance, dest);\n", fc);
	indent(fc);
	fputs("if (pdest != psource && pdest >= 0) {\n", fc);
	indent(fc);
	fputs("    add_edge(pdg, psource, state->owner[psource], pdest, state->owner[pdest]);\n", fc);
	indent(fc);
	fputs("}\n", fc);
	no_temporaries = 0;
	return 0;
}

void gen_deps_func(df)
	t_def	*df;
{
	/* Produce code for dependencies of a parallel operation, either
	   user defined or PDG.
	*/

	p_node	list = df->opr_dependencies;
	t_body	*b = df->df_body;
	t_def	*d;
	int	i;

	if (! (df->df_flags & D_HAS_DEPENDENCIES)) {
		gen_pdg_func(df);
		return;
	}
	if (df->df_flags & D_SIMPLE_DEPENDENCIES) {
	    char *s = gen_name("pdg_", df->df_idf->id_text, 0);

	    fprintf(fc, "pdg_p %s(instance_p instance) {\n", s);
	}
	else {
	    gen_part_prot(fc, df, df->opr_depname, 1);
	    fputs(" {\n", fc);
	}
	fputs("    state_p state = instance->state;\n", fc);
	fputs("    pdg_p pdg = new_pdg(state->num_parts);\n", fc);
	if (node_emptylist(list)) {
		fputs("\treturn pdg;\n", fc);
		fputs("}\n", fc);
		return;
	}

	fprintf(fc, "    int dest[%d];\n", CurrDef->df_type->arr_ndim);
	fputs("    int p;\n", fc);
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
	    fprintf(fc, "    int ss%d = state->start[%d];\n", i, i);
	    fprintf(fc, "    int se%d = state->end[%d];\n", i, i);
	    if (i > 0) {
		fprintf(fc, "    int len%d = state->length[%d];\n", i, i);
	    }
	}
	fputs("    for (p = 0; p < state->num_parts; p++) {\n", fc);
	fputs("\tpartition_p part = state->partition[p];\n", fc);
	fputs("\tint pdest, psource = part->part_num;\n", fc);
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		fprintf(fc, "\tint ps%d = part->start[%d];\n", i, i);
		fprintf(fc, "\tint pe%d = part->end[%d];\n", i, i);
	}

	declarations(df, list);

	indentlevel = 8;
	gen_statlist(b->init, 0);

	d = df->bod_scope->sc_def;
	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		while (!(d->df_flags & D_PART_INDEX)) {
			d = d->df_nextinscope;
		}
		indentlevel += 4;
		indent(fc);
		fprintf(fc, "for (%s = ps%d; %s <= pe%d; %s++) {\n",
			d->df_name, i, d->df_name, i, d->df_name);
		d = d->df_nextinscope;
	}

	gen_statlist(list, 0);

	for (i = 0; i < CurrDef->df_type->arr_ndim; i++) {
		indent(fc);
		indentlevel -= 4;
		fputs("}\n", fc);
	}

	cleanup_declarations(df, list);

	fputs("    }\n    return pdg;\n}\n", fc);
}
