/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: marshall.c,v 1.16 1998/06/24 10:49:55 ceriel Exp $ */

#include	<stdio.h>
#include	<assert.h>
#include	"ansi.h"
#include	"def.h"
#include	"type.h"
#include	"main.h"
#include	"generate.h"
#include	"scope.h"
#include	"options.h"
#include	"error.h"

_PROTOTYPE(static void gen_sz_op_call, (t_def *, int));
_PROTOTYPE(static void gen_ma_op_call, (t_def *, int));
_PROTOTYPE(static void gen_um_op_call, (t_def *, int));
_PROTOTYPE(static void gen_sz_op_return, (t_def *, int));
_PROTOTYPE(static void gen_ma_op_return, (t_def *, int));
_PROTOTYPE(static void gen_um_op_return, (t_def *, int));
_PROTOTYPE(static void gen_free_op_return, (t_def *));

_PROTOTYPE(static void gen_sz_args, (t_def *, int));
_PROTOTYPE(static void gen_ma_args, (t_def *, int));
_PROTOTYPE(static void gen_um_args, (t_def *, int));

_PROTOTYPE(static void gen_sz_obj, (t_def *));
_PROTOTYPE(static void gen_ma_obj, (t_def *));
_PROTOTYPE(static void gen_um_obj, (t_def *));

_PROTOTYPE(static void gen_nbytes_descr, (int, t_type *));
_PROTOTYPE(static void gen_marshall_descr, (int, t_type *));
_PROTOTYPE(static void gen_unmarshall_descr, (char *, t_type *));
_PROTOTYPE(static void gen_nbytes4_descr, (int, t_type *));
_PROTOTYPE(static void gen_marshall4_descr, (int, t_type *));
_PROTOTYPE(static void gen_unmarshall4_descr, (char *, t_type *));
_PROTOTYPE(static void gen_free_descr, (int, t_type *));

_PROTOTYPE(static void gen_sz_func, (t_type *, int));
_PROTOTYPE(static void gen_ma_func, (t_type *, int));
_PROTOTYPE(static void gen_um_func, (t_type *, int));

_PROTOTYPE(static void gen_sz4_func, (t_type *, int));
_PROTOTYPE(static void gen_ma4_func, (t_type *, int));
_PROTOTYPE(static void gen_um4_func, (t_type *, int));

/* Marshalling routine generation. */

void gen_marshall(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_OPERATION:
		fputs("#ifdef PANDA4\n", fc);
		gen_sz_op_call(df, 1);
		gen_ma_op_call(df, 1);
		gen_um_op_call(df, 1);
		gen_sz_op_return(df, 1);
		gen_ma_op_return(df, 1);
		gen_um_op_return(df, 1);
		fputs("#else\n", fc);
		gen_sz_op_call(df, 0);
		gen_ma_op_call(df, 0);
		gen_um_op_call(df, 0);
		gen_sz_op_return(df, 0);
		gen_ma_op_return(df, 0);
		gen_um_op_return(df, 0);
		fputs("#endif\n", fc);
		gen_free_op_return(df);
		break;
	case D_PROCESS:
		fputs("#ifdef PANDA4\n", fc);
		gen_sz_args(df, 1);
		gen_ma_args(df, 1);
		gen_um_args(df, 1);
		fputs("#else\n", fc);
		gen_sz_args(df, 0);
		gen_ma_args(df, 0);
		gen_um_args(df, 0);
		fputs("#endif\n", fc);
		break;
	case D_OBJECT:
		gen_sz_obj(df);
		gen_ma_obj(df);
		gen_um_obj(df);
		break;
	default:
		crash("gen_marshall");
		break;
	}
}

void
gen_marshall_macros(formal, actual)
	p_type	formal;
	p_type	actual;
{
	if (actual->tp_fund == T_GENERIC ||  
            (actual->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "#define %s %s\n", formal->tp_szfunc, actual->tp_szfunc);
	}
	else {
	    fputs("#ifdef PANDA4\n", fc);
		fprintf(fc, "#define %s(a) 1\n", formal->tp_szfunc);
	    fputs("#else\n", fc);
		fprintf(fc, "#define %s(a) sizeof(%s)\n",
			formal->tp_szfunc,
			actual->tp_tpdef);
	    fputs("#endif\n", fc);
	}
	if (actual->tp_fund == T_GENERIC ||
	    (actual->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "#define %s %s\n", formal->tp_mafunc, actual->tp_mafunc);
	}
	else {
	    fputs("#ifdef PANDA4\n", fc);
		fprintf(fc, "#define %s(a, b) ((a)->data = b, (a)->len = sizeof(%s), a+1)\n", formal->tp_mafunc, actual->tp_tpdef);
	    fputs("#else\n", fc);
		fprintf(fc, "#define %s(a, b) (memcpy(a, b, sizeof(%s)), a + sizeof(%s))\n",
			formal->tp_mafunc,
			actual->tp_tpdef,
			actual->tp_tpdef);
	    fputs("#endif\n", fc);
	}
	if (actual->tp_fund == T_GENERIC ||
	    (actual->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "#define %s %s\n", formal->tp_umfunc, actual->tp_umfunc);
	}
	else {
	    fputs("#ifdef PANDA4\n", fc);
		fprintf(fc, "#define %s(a, b) (pan_msg_consume(a, b, sizeof(%s)))\n",
			formal->tp_umfunc,
			actual->tp_tpdef);
	    fputs("#else\n", fc);
		fprintf(fc, "#define %s(a, b) (memcpy(b, a, sizeof(%s)), a + sizeof(%s))\n",
			formal->tp_umfunc,
			actual->tp_tpdef,
			actual->tp_tpdef);
	    fputs("#endif\n", fc);
	}
}

static void gen_sz_op_call(oper, panda4)
	t_def	*oper;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	fprintf(fc, "static int %s(void **argv) {\n",
		oper->bod_marshallnames[0]);
	fputs("\tint sz = 0;\n", fc);
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (is_in_param(df)) {
		    if (panda4) {
			gen_nbytes4_descr(i, df->df_type);
		    } else {
			gen_nbytes_descr(i, df->df_type);
		    }
		}
	}
	fputs("\treturn sz;\n}\n", fc);
}

static void gen_ma_op_call(oper, panda4)
	t_def	*oper;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	if (panda4) {
	    fprintf(fc, "static pan_iovec_p %s(pan_iovec_p p, void **argv) {\n",
		oper->bod_marshallnames[1]);
	} else {
	    fprintf(fc, "static char *%s(char *p, void **argv) {\n",
		oper->bod_marshallnames[1]);
	}
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (is_in_param(df)) {
		    if (panda4) {
			gen_marshall4_descr(i, df->df_type);
		    } else {
			gen_marshall_descr(i, df->df_type);
		    }
		}
	}
	fputs("\treturn p;\n}\n", fc);
}

static void gen_um_op_call(oper, panda4)
	t_def	*oper;
	int	panda4;
{
	int	i;
	t_dflst	l;
	t_def	*df;
	int	nparams = oper->df_type->prc_nparams;

	if (result_type_of(oper->df_type)) nparams++;

	if (panda4) {
		fprintf(fc, "static void %s(void *p, void ***ap) {\n",
		oper->bod_marshallnames[2]);
	} else {
		fprintf(fc, "static char *%s(char *p, void ***ap) {\n",
		oper->bod_marshallnames[2]);
	}
	if (nparams == 0) {
		fputs("\t*ap = 0;\n", fc);
		if (! panda4) fputs("\treturn p;\n", fc);
		fputs("}\n", fc);
		return;
	}

	if (dp_flag) nparams++;	/* leave room for extra parameter */
	fprintf(fc, "\tstruct {\n\t\tvoid *av[%d];\n", nparams);
	i = 1;
	def_walklist(param_list_of(oper->df_type), l, df) {
		fprintf(fc, "\t\t%s arg%d;\n", df->df_type->tp_tpdef, i);
		i++;
	}
	if (result_type_of(oper->df_type)) {
		fprintf(fc, "\t\t%s result;\n", result_type_of(oper->df_type)->tp_tpdef);
	}
	fputs("\t} *argstruct;\n", fc);

	fputs("\targstruct = m_malloc(sizeof(*argstruct));\n", fc);
	fputs("\t*ap = argstruct->av;\n", fc);
	i = -1;
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		fprintf(fc, "\targstruct->av[%d] = &(argstruct->arg%d);\n",
			i,
			i+1);
		if (! is_in_param(df)) {
			if (df->df_type->tp_fund == T_GENERIC ||
				 (df->df_type->tp_flags & T_INIT_CODE)) {
				fprintf(fc, "\tmemset(&(argstruct->arg%d), 0, sizeof(%s));\n",
				i+1,
				df->df_type->tp_tpdef);
			}
		}
		else {
			char buf[30];
			sprintf(buf, "&(argstruct->arg%d)", i+1);
			if (panda4) {
				gen_unmarshall4_descr(buf, df->df_type);
			} else {
				gen_unmarshall_descr(buf, df->df_type);
			}
		}
	}
	if (result_type_of(oper->df_type)) {
		t_type	*tp = result_type_of(oper->df_type);
		i++;
		fprintf(fc, "\targstruct->av[%d] = &(argstruct->result);\n", i);
		if (tp->tp_fund == T_GENERIC ||
			 (tp->tp_flags & T_INIT_CODE)) {
			fprintf(fc, "\tmemset(&(argstruct->result), 0, sizeof(%s));\n",
			tp->tp_tpdef);
		}
	}
	if (! panda4) {
		fputs("\treturn p;\n", fc);
	}
	fputs("}\n", fc);
}

static void gen_sz_op_return(oper, panda4)
	t_def	*oper;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	fprintf(fc, "static int %s(void **argv) {\n",
		oper->bod_marshallnames[3]);
	fputs("\tint sz = 0;\n", fc);
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (is_out_param(df)) {
		    if (panda4) {
			gen_nbytes4_descr(i, df->df_type);
		    } else {
			gen_nbytes_descr(i, df->df_type);
		    }
		}
	}
	if (result_type_of(oper->df_type)) {
	    if (panda4) {
		gen_nbytes4_descr(i+1, result_type_of(oper->df_type));
	    } else {
		gen_nbytes_descr(i+1, result_type_of(oper->df_type));
	    }
	}
	fputs("\treturn sz;\n}\n", fc);
}

static void gen_ma_op_return(oper, panda4)
	t_def	*oper;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	if (panda4) {
	    fprintf(fc, "static pan_iovec_p %s(pan_iovec_p p, void **argv) {\n",
		oper->bod_marshallnames[4]);
	} else {
	    fprintf(fc, "static char *%s(char *p, void **argv) {\n",
		oper->bod_marshallnames[4]);
	}
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (is_out_param(df)) {
		    if (panda4) {
			gen_marshall4_descr(i, df->df_type);
		    } else {
			gen_marshall_descr(i, df->df_type);
		    }
		}
		if (! panda4) gen_free_descr(i, df->df_type);
	}
	if (result_type_of(oper->df_type)) {
		if (panda4) {
			gen_marshall4_descr(i+1, result_type_of(oper->df_type));
		} else {
			gen_marshall_descr(i+1, result_type_of(oper->df_type));
		}
		if (! panda4) gen_free_descr(i+1, result_type_of(oper->df_type));
	}
	if (! panda4) fprintf(fc, "\tm_free((void *) argv);\n");
	fputs("\treturn p;\n}\n", fc);
}

static void gen_um_op_return(oper, panda4)
	t_def	*oper;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	if (panda4) {
		fprintf(fc, "static void %s(void *p, void **argv) {\n",
			oper->bod_marshallnames[5]);
	} else {
		fprintf(fc, "static char *%s(char *p, void **argv) {\n",
			oper->bod_marshallnames[5]);
	}

	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (! is_in_param(df)) {
			char buf[20];
			sprintf(buf, "argv[%d]", i);
			gen_free_descr(i, df->df_type);
			if (panda4) {
				gen_unmarshall4_descr(buf, df->df_type);
			} else {
				gen_unmarshall_descr(buf, df->df_type);
			}
		}
	}
	if (result_type_of(oper->df_type)) {
		char buf[20];
		sprintf(buf, "argv[%d]", i+1);
		gen_free_descr(i+1, result_type_of(oper->df_type));
		if (panda4) {
			gen_unmarshall4_descr(buf, result_type_of(oper->df_type));
		} else {
			gen_unmarshall_descr(buf, result_type_of(oper->df_type));
		}
	}
	if (panda4) {
		fputs("}\n", fc);
	} else {
		fputs("\treturn p;\n}\n", fc);
	}
}

static void gen_free_op_return(oper)
	t_def	*oper;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	fprintf(fc, "static void %s(void **argv) {\n",
		oper->bod_marshallnames[6]);
	def_walklist(param_list_of(oper->df_type), l, df) {
		i++;
		if (df->df_type->tp_flags & T_DYNAMIC) {
			gen_free_descr(i, df->df_type);
		}
	}
	if (result_type_of(oper->df_type)) {
		if (result_type_of(oper->df_type)->tp_flags & T_DYNAMIC) {
			gen_free_descr(i+1, result_type_of(oper->df_type));
		}
	}
	fprintf(fc, "\tm_free((void *) argv);\n");
	fputs("}\n", fc);
}

static void gen_sz_args(proc, panda4)
	t_def	*proc;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	fprintf(fc, "static int %s(void **argv) {\n",
		proc->bod_marshallnames[0]);
	fputs("\tint sz = 0;\n", fc);
	def_walklist(param_list_of(proc->df_type), l, df) {
		i++;
		if (! is_shared_param(df)) {
		    if (panda4) {
			gen_nbytes4_descr(i, df->df_type);
		    } else {
			gen_nbytes_descr(i, df->df_type);
		    }
		}
	}
	fputs("\treturn sz;\n}\n", fc);
}

static void gen_ma_args(proc, panda4)
	t_def	*proc;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	if (panda4) {
		fprintf(fc, "static pan_iovec_p %s(pan_iovec_p p, void **argv) {\n",
			proc->bod_marshallnames[1]);
	} else {
		fprintf(fc, "static char *%s(char *p, void **argv) {\n",
			proc->bod_marshallnames[1]);
	}
	def_walklist(param_list_of(proc->df_type), l, df) {
		i++;
		if (! is_shared_param(df)) {
		    if (panda4) {
			gen_marshall4_descr(i, df->df_type);
		    } else {
			gen_marshall_descr(i, df->df_type);
		    }
		}
	}
	fputs("\treturn p;\n}\n", fc);
}

static void gen_um_args(proc, panda4)
	t_def	*proc;
	int	panda4;
{
	int	i = -1;
	t_dflst	l;
	t_def	*df;

	if (panda4) {
		fprintf(fc, "static void %s(void *p, void **argv) {\n",
			proc->bod_marshallnames[2]);
	} else {
		fprintf(fc, "static char *%s(char *p, void **argv) {\n",
			proc->bod_marshallnames[2]);
	}

	def_walklist(param_list_of(proc->df_type), l, df) {
		i++;
		if (! is_shared_param(df)) {
			char buf[20];
			sprintf(buf, "argv[%d]", i);
			fprintf(fc, "\t%s = m_malloc(sizeof(%s));\n",
				buf,
				df->df_type->tp_tpdef);
			if (panda4) {
				gen_unmarshall4_descr(buf, df->df_type);
			} else {
				gen_unmarshall_descr(buf, df->df_type);
			}
		}
	}
	if (! panda4) {
		fputs("\treturn p;\n", fc);
	}
	fputs("}\n", fc);
}


static void gen_sz_obj(obj)
	t_def	*obj;
{
	t_type	*tp = record_type_of(obj->df_type);

	fprintf(fc, "static int %s(t_object *op) {\n",
		obj->bod_marshallnames[0]);
	if (obj->df_flags & D_PARTITIONED) {
		fputs("    return 0;\n", fc);
	}
	else if (tp->tp_flags & T_DYNAMIC) {
		fprintf(fc, "    return %s(op->o_fields);\n", tp->tp_szfunc);
	}
	else {
		fputs("#ifdef PANDA4\n", fc);
			fputs("    return 1;\n", fc);
		fputs("#else\n", fc);
			fprintf(fc, "    return sizeof(%s);\n", tp->tp_tpdef);
		fputs("#endif\n", fc);
	}
	fputs("}\n", fc);
}

static void gen_ma_obj(obj)
	t_def	*obj;
{
	t_type	*tp = record_type_of(obj->df_type);

	fputs("#ifdef PANDA4\n", fc);
		fprintf(fc, "static pan_iovec_p %s(pan_iovec_p p, t_object *op) {\n",
			obj->bod_marshallnames[1]);
	fputs("#else\n", fc);
		fprintf(fc, "static char *%s(char *p, t_object *op) {\n",
			obj->bod_marshallnames[1]);
	fputs("#endif\n", fc);
	if (obj->df_flags & D_PARTITIONED) {
		fputs("    return p;\n", fc);
	}
	else if (tp->tp_flags & T_DYNAMIC) {
		fprintf(fc, "    return %s(p, op->o_fields);\n", tp->tp_mafunc);
	}
	else {
		fputs("#ifdef PANDA4\n", fc);
			fprintf(fc, "    p->data = op->o_fields;\n");
			fprintf(fc, "    p->len = sizeof(%s);\n", tp->tp_tpdef);
			fprintf(fc, "    return p+1;\n");
		fputs("#else\n", fc);
			fprintf(fc, "    memcpy(p, op->o_fields, sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    return p + sizeof(%s);\n", tp->tp_tpdef);
		fputs("#endif\n", fc);
	}
	fputs("}\n", fc);
}

static void gen_um_obj(obj)
	t_def	*obj;
{
	t_type	*tp = record_type_of(obj->df_type);

	fputs("#ifdef PANDA4\n", fc);
		fprintf(fc, "static void %s(void *p, t_object *op) {\n",
			obj->bod_marshallnames[2]);
	fputs("#else\n", fc);
		fprintf(fc, "static char *%s(char *p, t_object *op) {\n",
			obj->bod_marshallnames[2]);
	if (obj->df_flags & D_PARTITIONED) {
		fputs("    return p;\n", fc);
	}
	fputs("#endif\n", fc);
	if (! (obj->df_flags & D_PARTITIONED)) {
	    fprintf(fc, "    if (! op->o_fields) op->o_fields = m_malloc(sizeof(%s));\n",
		tp->tp_tpdef);
	    fputs("#ifdef PANDA4\n", fc);
	    	if (tp->tp_flags & T_DYNAMIC) {
		    fprintf(fc, "    %s(p, op->o_fields);\n", tp->tp_umfunc);
	    	}
	    	else {
		    fprintf(fc, "    pan_msg_consume(p, op->o_fields, sizeof(%s));\n",
			tp->tp_tpdef);
	    	}
	    fputs("#else\n", fc);
	    	if (tp->tp_flags & T_DYNAMIC) {
		    fprintf(fc, "    return %s(p, op->o_fields);\n", tp->tp_umfunc);
	    	}
	    	else {
		    fprintf(fc, "    memcpy(op->o_fields, p, sizeof(%s));\n",
			tp->tp_tpdef);
	    	    fprintf(fc, "    return p + sizeof(%s);\n", tp->tp_tpdef);
	    	}
	    fputs("#endif\n", fc);
	}
	fputs("}\n", fc);
}

static void gen_nbytes_descr(ind, tpd)
	int	ind;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\tsz += %s(argv[%d]);\n",
			tpd->tp_szfunc,
			ind);
	}
	else {
		fprintf(fc, "\tsz += sizeof(%s);\n",
			tpd->tp_tpdef);
	}
}

static void gen_nbytes4_descr(ind, tpd)
	int	ind;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\tsz += %s(argv[%d]);\n",
			tpd->tp_szfunc,
			ind);
	}
	else {
		fprintf(fc, "\tsz++;\n");
	}
}

static void gen_marshall_descr(ind, tpd)
	int	ind;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\tp = %s(p, argv[%d]);\n",
			tpd->tp_mafunc,
			ind);
	}
	else {
		fprintf(fc, "\tmemcpy(p, argv[%d], sizeof(%s));\n",
			ind,
			tpd->tp_tpdef);
		fprintf(fc, "\tp += sizeof(%s);\n", tpd->tp_tpdef);
	}
}

static void gen_marshall4_descr(ind, tpd)
	int	ind;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\tp = %s(p, argv[%d]);\n",
			tpd->tp_mafunc,
			ind);
	}
	else {
		fprintf(fc, "\tp->data = argv[%d];\n", ind);
		fprintf(fc, "\tp->len = sizeof(%s);\n",
			tpd->tp_tpdef);
		fprintf(fc, "\tp++;\n");
	}
}

static void gen_unmarshall_descr(s, tpd)
	char	*s;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\tp = %s(p, %s);\n",
			tpd->tp_umfunc,
			s);
	}
	else {
		fprintf(fc, "\tmemcpy(%s, p, sizeof(%s));\n",
			s,
			tpd->tp_tpdef);
		fprintf(fc, "\tp += sizeof(%s);\n", tpd->tp_tpdef);
	}
}

static void gen_unmarshall4_descr(s, tpd)
	char	*s;
	t_type	*tpd;
{
	if (tpd->tp_fund == T_GENERIC ||
	    (tpd->tp_flags & T_DYNAMIC)) {
		fprintf(fc, "\t%s(p, %s);\n", tpd->tp_umfunc, s);
	}
	else {
		fprintf(fc, "\tpan_msg_consume(p, %s, sizeof(%s));\n",
			s,
			tpd->tp_tpdef);
	}
}

static void gen_free_descr(ind, tpd)
	int	ind;
	t_type	*tpd;
{
	if (tpd->tp_flags & T_DYNAMIC) {
		if (tpd->tp_fund == T_GENERIC) {
			fprintf(fc, "#if dynamic_%s\n", tpd->tp_tpdef);
		}
		fprintf(fc, "\t%s(argv[%d]);\n",
			tpd->tp_freefunc,
			ind);
		if (tpd->tp_fund == T_GENERIC) {
			fputs("#endif\n", fc);
		}
	}
}

void gen_marshall_funcs(tp, exported)
	t_type	*tp;
	int	exported;
{
	if (! (tp->tp_flags & T_DYNAMIC)) {
		return;
	}
	if (exported) {
		fprintf(fh, "extern int %s(%s *);\n",
			tp->tp_szfunc,
			tp->tp_tpdef);
		fputs("#ifdef PANDA4\n", fh);
		    fprintf(fh, "extern pan_iovec_p %s(pan_iovec_p , %s *);\n",
			tp->tp_mafunc,
			tp->tp_tpdef);
		    fprintf(fh, "void %s(void *, %s *);\n",
			tp->tp_umfunc,
			tp->tp_tpdef);
		fputs("#else\n", fh);
		    fprintf(fh, "extern char *%s(char *, %s *);\n",
			tp->tp_mafunc,
			tp->tp_tpdef);
		    fprintf(fh, "extern char *%s(char *, %s *);\n",
			tp->tp_umfunc,
			tp->tp_tpdef);
		fputs("#endif\n", fh);
	}

	fputs("#ifdef PANDA4\n", fc);
		gen_sz4_func(tp, exported);
		gen_ma4_func(tp, exported);
		gen_um4_func(tp, exported);
	fputs("#else\n", fc);
		gen_sz_func(tp, exported);
		gen_ma_func(tp, exported);
		gen_um_func(tp, exported);
	fputs("#endif\n", fc);
}

static void
gen_sz_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "int %s(%s *a) {\n",
		tp->tp_szfunc,
		tp->tp_tpdef);

	fputs("    int sz;\n", fc);
	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fprintf(fc, "    int i;\n    %s *s;\n", t->tp_tpdef);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		if (tp->tp_flags & T_CONSTBNDS) {
		    fputs("    sz = 0;\n", fc);
		}
		else {
		    fprintf(fc, "    sz = %d * sizeof(a->a_dims[0].a_lwb);\n",
			tp->arr_ndim * 2);
		    fputs("    if (a->a_sz <= 0) return sz;\n", fc);
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (tp->tp_flags & T_CONSTBNDS) {
			    fprintf(fc, "    s = a->a_data;\n");
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",
				tp->arr_size);
			}
			else {
			    fprintf(fc, "    s = (%s *) a->a_data + a->a_offset;\n",
				t->tp_tpdef);
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			fprintf(fc, "\tsz += %s(s);\n", t->tp_szfunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				if (tp->tp_flags & T_CONSTBNDS) {
				    fputs("    sz += sizeof(*a);\n", fc);
				}
				else {
				    fprintf(fc, "    sz += a->a_sz * sizeof(%s);\n",
					t->tp_tpdef);
				}
				fputs("#endif\n", fc);
			}
		}
		else {
			if (tp->tp_flags & T_CONSTBNDS) {
			    fputs("    sz += sizeof(*a);\n", fc);
			}
			else {
			    fprintf(fc, "    sz += a->a_sz * sizeof(%s);\n",
				t->tp_tpdef);
			}
		}
		break;

	case T_RECORD:
		fputs("    sz = 0;\n", fc);
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
			if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
				t = deflist->df_type;
				if (t->tp_flags & T_DYNAMIC) {
					fprintf(fc, "    sz += %s(&(a->%s));\n",
						t->tp_szfunc,
						deflist->df_name);
				}
				else {
					fprintf(fc, "    sz += sizeof(%s);\n",
						t->tp_tpdef);
				}
			}
			deflist = deflist->df_nextinscope;
		}
		break;

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    sz = o_rts_nbytes(a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    sz += %s(a->o_fields);\n",
				tp->tp_szfunc);
		}
		else	fprintf(fc, "    sz += sizeof(%s);\n",
				tp->tp_tpdef);
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fprintf(fc, "    sz = sizeof(a->u_init) + sizeof(a->f_%s);\n",
			deflist->df_idf->id_text);
		fputs("    if (! a->u_init) return sizeof(a->u_init);\n", fc);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\tsz += %s(&(a->u_el.%s));\n",
					t->tp_szfunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tsz += sizeof(%s);\n",
					t->tp_tpdef);
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    t_elem *e;\n", fc);
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		fputs("    sz = sizeof(a->s_nelem);\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if dynamic_%s\n", t->tp_tpdef);
			}
			fputs("    for (e = a->s_elem; e; e = e->e_next) {\n", fc);
			fputs("\tfor (i = MAXELC-1; i >= 0; i--) if (e->e_mask & (1 << i)) {\n", fc);
			fprintf(fc, "\t    sz += %s(&((%s *)(e->e_buf))[i]);\n",
				t->tp_szfunc, t->tp_tpdef);
			fputs("\t}\n", fc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				fprintf(fc, "    sz += a->s_nelem * sizeof(%s);\n",
					t->tp_tpdef);
				fputs("#endif\n", fc);
			}
		}
		else {
			fprintf(fc, "    sz += a->s_nelem * sizeof(%s);\n",
				t->tp_tpdef);
		}
		break;

	case T_GRAPH:
		fputs("    t_mt *p = a->g_mt;\n    int i;\n", fc);
		fputs("    sz = sizeof(a->g_size);\n", fc);
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    sz += %s(&(a->g_root));\n",
					t->tp_szfunc);
			}
			else {
				fprintf(fc, "    sz += sizeof(%s);\n",
					t->tp_tpdef);
			}
		}
		fputs("    if (a->g_size <= 0) return sz;\n", fc);
		fputs("    sz += sizeof(int) + a->g_size * sizeof(t_mt);\n", fc);
		fputs("    for (i = a->g_size; i > 0; i--, p++) {\n", fc);
		t = tp->gra_node;
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\tif (! nodeisfree(p)) sz += %s(p->g_node);\n",
				t->tp_szfunc);
		}
		else {
			fprintf(fc, "\tif (! nodeisfree(p)) sz += sizeof(%s);\n",
				t->tp_tpdef);
		}
		fputs("    }\n", fc);
		break;
	}

	fputs("    return sz;\n}\n\n", fc);
}

static void
gen_ma_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;
	int	i;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "char *%s(char *p, %s *a) {\n",
		tp->tp_mafunc,
		tp->tp_tpdef);

	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		fprintf(fc, "    %s *s;\n", t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}

		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    for (i = 0; i < tp->arr_ndim; i++) {
			fprintf(fc, "    put_tp(a->a_dims[%d].a_lwb, p);\n", i);
			fprintf(fc, "    put_tp(a->a_dims[%d].a_nel, p);\n", i);
		    }
		    fputs("    if (a->a_sz <= 0) return p;\n", fc);
		    fprintf(fc, "    s = (%s *) a->a_data + a->a_offset;\n",
			t->tp_tpdef);
		}
		else {
		    fprintf(fc, "    s = a->a_data;\n");
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			else {
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",
				tp->arr_size);

			}
			fprintf(fc, "\tp = %s(p, s);\n", t->tp_mafunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				if (! (tp->tp_flags & T_CONSTBNDS)) {
				    fprintf(fc, "    memcpy(p, s, a->a_sz * sizeof(%s));\n",
					t->tp_tpdef);
				    fprintf(fc, "    p += a->a_sz * sizeof(%s);\n",
					t->tp_tpdef);
				}
				else {
				    fprintf(fc, "    memcpy(p, s, sizeof(*a));\n");
				    fprintf(fc, "    p += sizeof(*a);\n");
				}
				fputs("#endif\n", fc);
			}
		}
		else {
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fprintf(fc, "    memcpy(p, s, a->a_sz * sizeof(%s));\n",
				t->tp_tpdef);
			    fprintf(fc, "    p += a->a_sz * sizeof(%s);\n",
				t->tp_tpdef);
			}
			else {
			    fprintf(fc, "    memcpy(p, s, sizeof(*a));\n");
			    fprintf(fc, "    p += sizeof(*a);\n");
			}
		}
		break;

	case T_RECORD:
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
			if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
				t = deflist->df_type;
				if (t->tp_flags & T_DYNAMIC) {
					fprintf(fc, "    p = %s(p, &(a->%s));\n",
						t->tp_mafunc,
						deflist->df_name);
				}
				else {
					fprintf(fc, "    memcpy(p, &(a->%s), sizeof(%s));\n",
						deflist->df_name,
						t->tp_tpdef);
					fprintf(fc, "    p += sizeof(%s);\n",
						t->tp_tpdef);
				}
			}
			deflist = deflist->df_nextinscope;
		}
		break;

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    p = o_rts_marshall(p, a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    p = %s(p, a->o_fields);\n",
				tp->tp_mafunc);
		}
		else {
			fprintf(fc, "    memcpy(p, a->o_fields, sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    p += sizeof(%s);\n",
				tp->tp_tpdef);
		}
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fputs("    put_tp(a->u_init, p);\n", fc);
		fputs("    if (! a->u_init) return p;\n", fc);
		fprintf(fc, "    put_tp(a->f_%s, p);\n", deflist->df_idf->id_text);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\tp = %s(p, &(a->u_el.%s));\n",
					t->tp_mafunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tmemcpy(p, &(a->u_el.%s), sizeof(%s));\n",
					deflist->df_name,
					t->tp_tpdef);
				fprintf(fc, "\tsz += sizeof(%s);\n",
					t->tp_tpdef);
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		fputs("    t_elem *e;\n", fc);
		fputs("    int i;\n", fc);
		fputs("    put_tp(a->s_nelem, p);\n", fc);
		fputs("    for (e = a->s_elem; e; e = e->e_next) {\n", fc);
		fputs("\tfor (i = MAXELC-1; i >= 0; i--) if (e->e_mask & (1 << i)) {\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    p = %s(p, &((%s *)(e->e_buf))[i]);\n",
				t->tp_mafunc, t->tp_tpdef);
		}
		else if (t->tp_fund & T_ISSIMPLEARG) {
			fprintf(fc, "\t     nput_tp(&((%s *)(e->e_buf))[i], p, sizeof(%s));\n",
				t->tp_tpdef, t->tp_tpdef);
		}
		else {
			fprintf(fc, "\t    memcpy(p, &((%s *)(e->e_buf))[i], sizeof(%s));\n",
				t->tp_tpdef, t->tp_tpdef);
			fprintf(fc, "\t    p += sizeof(%s);\n", t->tp_tpdef);
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;

	case T_GRAPH:
		fputs("    t_mt *np = a->g_mt;\n    int i, df;\n", fc);
		fputs("    put_tp(a->g_size, p);\n", fc);
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    p = %s(p, &(a->g_root));\n",
					t->tp_mafunc);
			}
			else {
				fprintf(fc, "    memcpy(p, &(a->g_root), sizeof(%s));\n",
					t->tp_tpdef);
				fprintf(fc, "    p += sizeof(%s);\n",
					t->tp_tpdef);
			}
		}
		fputs("    if (a->g_size <= 0) return p;\n", fc);
		fputs("    memcpy(p, np, a->g_size * sizeof(t_mt));\n", fc);
		fputs("    p += a->g_size * sizeof(t_mt);\n", fc);
		fputs("    put_tp(a->g_freelist, p);\n", fc);
		fputs("    for (i = a->g_size; i > 0; i--, np++) {\n", fc);
		t = tp->gra_node;
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\tif (! nodeisfree(np)) p = %s(p, np->g_node);\n",
				t->tp_mafunc);
		}
		else {
			fprintf(fc, "\tif (! nodeisfree(np)) {\n\t    memcpy(p, np->g_node, sizeof(%s));\n",
				t->tp_tpdef);
			fprintf(fc, "\t    p += sizeof(%s);\n",
				t->tp_tpdef);
			fputs("\t}\n", fc);
		}
		fputs("    }\n", fc);
		break;
	}
	fputs("    return p;\n}\n\n", fc);
}

static void
gen_um_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;
	int	i;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "char *%s(char *p, %s *a) {\n",
		tp->tp_umfunc,
		tp->tp_tpdef);
	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		fprintf(fc, "    %s *s;\n", t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    fputs("    a->a_offset = 0;\n", fc);
		    for (i = 0; i < tp->arr_ndim; i++) {
			fprintf(fc, "    get_tp(a->a_dims[%d].a_lwb, p);\n", i);
			fprintf(fc, "    get_tp(a->a_dims[%d].a_nel, p);\n", i);
			if (i == 0) {
				fputs("    a->a_sz = a->a_dims[0].a_nel;\n", fc);
				fputs("    a->a_offset = a->a_dims[0].a_lwb;\n", fc);
			}
			else {
				fputs("    if (a->a_sz <= 0) a->a_sz = 0;\n", fc);
				fprintf(fc, "    a->a_sz *= a->a_dims[%d].a_nel;\n", i);
				fprintf(fc, "    a->a_offset *= a->a_dims[%d].a_nel;\n", i);
				fprintf(fc, "    a->a_offset += a->a_dims[%d].a_lwb;\n", i);
			}
		    }
		    fputs("    if (a->a_sz <= 0) { a->a_sz = 0; a->a_data = 0;  return p; }\n", fc);
		    fprintf(fc, "    s = (%s *) m_malloc(a->a_sz * sizeof(%s));\n",
			t->tp_tpdef,
			t->tp_tpdef);
		    fputs("    a->a_data = s - a->a_offset;\n", fc);
		}
		else {
		    fprintf(fc, "    s = a->a_data;\n");
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (!(tp->tp_flags & T_CONSTBNDS)) {
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			else {
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",				tp->arr_size);
			}
			fprintf(fc, "\tp = %s(p, s);\n", t->tp_umfunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				if (! (tp->tp_flags & T_CONSTBNDS)) {
				    fprintf(fc, "    memcpy(s, p, a->a_sz * sizeof(%s));\n",
					t->tp_tpdef);
				    fprintf(fc, "    p += a->a_sz * sizeof(%s);\n",
					t->tp_tpdef);
				} else {
				    fprintf(fc, "    memcpy(s, p, sizeof(*a));\n");
				    fprintf(fc, "    p += sizeof(*a);\n");
				}
				fputs("#endif\n", fc);
			}
		}
		else {
		    if (! (tp->tp_flags & T_CONSTBNDS)) {
			fprintf(fc, "    memcpy(s, p, a->a_sz * sizeof(%s));\n",
				t->tp_tpdef);
			fprintf(fc, "    p += a->a_sz * sizeof(%s);\n",
				t->tp_tpdef);
		    } else {
			fprintf(fc, "    memcpy(s, p, sizeof(*a));\n");
			fprintf(fc, "    p += sizeof(*a);\n");
		    }
		}
		break;

	case T_RECORD:
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
			if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
				t = deflist->df_type;
				if (t->tp_flags & T_DYNAMIC) {
					fprintf(fc, "    p = %s(p, &(a->%s));\n",
						t->tp_umfunc,
						deflist->df_name);
				}
				else {
					fprintf(fc, "    memcpy(&(a->%s), p, sizeof(%s));\n",
						deflist->df_name,
						t->tp_tpdef);
					fprintf(fc, "    p += sizeof(%s);\n",
						t->tp_tpdef);
				}
			}
			deflist = deflist->df_nextinscope;
		}
		break;

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    p = o_rts_unmarshall(p, a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		fprintf(fc, "    a->o_fields = m_malloc(sizeof(%s));\n",
			tp->tp_tpdef);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    p = %s(p, a->o_fields);\n",
				tp->tp_umfunc);
		}
		else {
			fprintf(fc, "    memcpy(a->o_fields, p, sizeof(%s));\n",
				tp->tp_tpdef);
			fprintf(fc, "    p += sizeof(%s);\n",
				tp->tp_tpdef);
		}
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fputs("    get_tp(a->u_init, p);\n", fc);
		fputs("    if (! a->u_init) return p;\n", fc);
		fprintf(fc, "    get_tp(a->f_%s, p);\n", deflist->df_idf->id_text);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\tp = %s(p, &(a->u_el.%s));\n",
					t->tp_umfunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tmemcpy(&(a->u_el.%s), p, sizeof(%s));\n",
					deflist->df_name,
					t->tp_tpdef);
				fprintf(fc, "\tsz += sizeof(%s);\n",
					t->tp_tpdef);
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		fputs("    t_elem *e;\n", fc);
		fputs("    int i, j;\n", fc);
		fputs("    void *ep;\n", fc);
		fputs("    a->s_elem = 0;\n", fc);
		fputs("    get_tp(a->s_nelem, p);\n", fc);
		fputs("    for (i = a->s_nelem; i > 0; i -= MAXELC) {\n", fc);
		fprintf(fc, "\te = m_malloc(sizeof(t_elemhdr) + MAXELC * sizeof(%s));\n", t->tp_tpdef);
		fputs("\tep = e->e_buf;\n", fc);
		fputs("\te->e_next = a->s_elem;\n", fc);
		fputs("\ta->s_elem = e;\n", fc);
		fputs("\tj = MAXELC; if (j > i) j = i;\n", fc);
		fputs("\te->e_count = j;\n", fc);
		fputs("\te->e_mask = 0;\n", fc);
		fputs("\twhile (j-- > 0) {\n", fc);
		fputs("\t    e->e_mask |= (1 << j);\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    p = %s(p, &((%s *)(e->e_buf))[j]);\n",
				t->tp_umfunc, t->tp_tpdef);
		}
		else if (t->tp_fund & T_ISSIMPLEARG) {
			fprintf(fc, "\t     nget_tp(&((%s *)(e->e_buf))[j], p, sizeof(%s));\n",
				t->tp_tpdef, t->tp_tpdef);
		}
		else {
			fprintf(fc, "\t    memcpy(&((%s *)(e->e_buf))[j], p, sizeof(%s));\n",
				t->tp_tpdef, t->tp_tpdef);
			fprintf(fc, "\t    p += sizeof(%s);\n", t->tp_tpdef);
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;

	case T_GRAPH:
		fputs("    t_mt *np;\n    int i, df;\n", fc);
		fputs("    get_tp(a->g_size, p);\n", fc);
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    p = %s(p, &(a->g_root));\n",
					t->tp_umfunc);
			}
			else {
				fprintf(fc, "    memcpy(&(a->g_root), p, sizeof(%s));\n",
					t->tp_tpdef);
				fprintf(fc, "    p += sizeof(%s);\n",
					t->tp_tpdef);
			}
		}
		fputs("    a->g_freelist = 0; a->g_freenodes = 0; a->g_ndlist = 0;\n", fc);
		fputs("    if (a->g_size == 0) { a->g_mt = 0; return p; }\n", fc);
		fputs("    a->g_mt = np = m_malloc(a->g_size * sizeof(t_mt));\n", fc);
		fputs("    memcpy(a->g_mt, p, a->g_size * sizeof(t_mt));\n", fc);
		fputs("    p += a->g_size * sizeof(t_mt);\n", fc);
		fputs("    get_tp(a->g_freelist, p);\n", fc);
		fputs("    np = a->g_mt;\n", fc);
		fputs("    for (i = a->g_size; i > 0; i--, np++) {\n", fc);
		fputs("\tif (! nodeisfree(np)) {\n", fc);
		t = tp->gra_node;
		fprintf(fc, "\t    np->g_node = g_getnode(a, sizeof(%s));\n",
			t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    p = %s(p, np->g_node);\n",
				t->tp_umfunc);
		}
		else {
			fprintf(fc, "\t    memcpy(np->g_node, p, sizeof(%s));\n",
				t->tp_tpdef);
			fprintf(fc, "\t    p += sizeof(%s);\n",
				t->tp_tpdef);
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;
	}
	fputs("    return p;\n}\n\n", fc);
}

static void
gen_sz4_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "int %s(%s *a) {\n",
		tp->tp_szfunc,
		tp->tp_tpdef);

	fputs("    int sz = 0;\n", fc);
	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fprintf(fc, "    int i;\n    %s *s;\n", t->tp_tpdef);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    fputs("    if (a->a_sz <= 0) return 1;\n", fc);
		    fputs("    sz = 1;\n", fc);
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (tp->tp_flags & T_CONSTBNDS) {
			    fprintf(fc, "    s = a->a_data;\n");
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",
				tp->arr_size);
			}
			else {
			    fprintf(fc, "    s = (%s *) a->a_data + a->a_offset;\n",
				t->tp_tpdef);
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			fprintf(fc, "\tsz += %s(s);\n", t->tp_szfunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				fputs("    sz ++;\n", fc);
				fputs("#endif\n", fc);
			}
		}
		else {
			fputs("    sz ++;\n", fc);
		}
		break;

	case T_RECORD: {
		int cnt = 0;

		deflist = tp->rec_scope->sc_def;
		while (deflist) {
			if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
				t = deflist->df_type;
				if (t->tp_flags & T_DYNAMIC) {
					if (cnt) {
						cnt = 0;
						fputs("    sz++;\n", fc);
					}
					fprintf(fc, "    sz += %s(&(a->%s));\n",
						t->tp_szfunc,
						deflist->df_name);
				}
				else cnt++;
			}
			deflist = deflist->df_nextinscope;
		}
		if (cnt) {
			fputs("    sz++;\n", fc);
		}
		break;
		}

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    sz = o_rts_nbytes(a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    sz += %s(a->o_fields);\n",
				tp->tp_szfunc);
		}
		else	fprintf(fc, "    sz ++;\n");
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fputs("    sz = 1;\n    if (! a->u_init) return sz;\n", fc);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\tsz += %s(&(a->u_el.%s));\n",
					t->tp_szfunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tsz ++;\n");
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    t_elem *e;\n", fc);
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		fputs("    sz = 1;\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if dynamic_%s\n", t->tp_tpdef);
			}
			fputs("    for (e = a->s_elem; e; e = e->e_next) {\n", fc);
			fputs("\tfor (i = MAXELC-1; i >= 0; i--) if (e->e_mask & (1 << i)) {\n", fc);
			fprintf(fc, "\t    sz += %s(&((%s *)(e->e_buf))[i]);\n",
				t->tp_szfunc, t->tp_tpdef);
			fputs("\t}\n", fc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				fprintf(fc, "    sz += a->s_nelem;\n");
				fputs("#endif\n", fc);
			}
		}
		else {
			fprintf(fc, "    sz += a->s_nelem;\n");
		}
		break;

	case T_GRAPH:
		fputs("    t_mt *p = a->g_mt;\n    int i;\n", fc);
		fputs("    sz = 1;\n", fc);	/* for size and freelist */
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    sz += %s(&(a->g_root));\n",
					t->tp_szfunc);
			}
			else {
				fprintf(fc, "    sz ++;\n");
			}
		}
		fputs("    if (a->g_size <= 0) return sz;\n", fc);
		fputs("    sz ++;\n", fc);	/* mt */
		t = tp->gra_node;
		fputs("    for (i = a->g_size; i > 0; i--, p++) {\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\tif (! nodeisfree(p)) sz += %s(p->g_node);\n",
				t->tp_szfunc);
		}
		else {
			fprintf(fc, "\tif (! nodeisfree(p)) sz++;\n");
		}
		fputs("    }\n", fc);
		break;
	}

	fputs("    return sz;\n}\n\n", fc);
}

static void
gen_ma4_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "pan_iovec_p %s(pan_iovec_p p, %s *a) {\n",
		tp->tp_mafunc,
		tp->tp_tpdef);

	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		fprintf(fc, "    %s *s;\n", t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}

		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    fprintf(fc, "    p->data = (void *) a;\n");
		    fprintf(fc, "    p->len = sizeof(*a);\n");
		    fprintf(fc, "    p++;\n");
		    fputs("    if (a->a_sz <= 0) return p;\n", fc);
		    fprintf(fc, "    s = (%s *) a->a_data + a->a_offset;\n",
			t->tp_tpdef);
		}
		else {
		    fprintf(fc, "    s = a->a_data;\n");
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			else {
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",
				tp->arr_size);

			}
			fprintf(fc, "\tp = %s(p, s);\n", t->tp_mafunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				fprintf(fc, "    p->data = (void *)s;\n");
				if (! (tp->tp_flags & T_CONSTBNDS)) {
				    fprintf(fc, "    p->len = a->a_sz * sizeof(%s);\n",
                                        t->tp_tpdef);
				}
				else {
				    fprintf(fc, "    p->len = sizeof(*a);\n");
				}
				fprintf(fc, "    p++;\n");
				fputs("#endif\n", fc);
			}
		}
		else {
			fprintf(fc, "    p->data = (void *)s;\n");
			if (! (tp->tp_flags & T_CONSTBNDS)) {
			    fprintf(fc, "    p->len = a->a_sz * sizeof(%s);\n",
                                       t->tp_tpdef);
			}
			else {
			    fprintf(fc, "    p->len = sizeof(*a);\n");
			}
			fprintf(fc, "    p++;\n");
		}
		break;

	case T_RECORD: {
		char *start = 0;
		int   cnt = 0;
		
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
		    if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
			if (! start) start = deflist->df_name;
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
			    if (cnt) {
				cnt = 0;
				fprintf(fc, "    p->data = &(a->%s);\n", start);
				fprintf(fc, "    p->len = offsetof(%s, %s) - offsetof(%s, %s);\n", tp->tp_tpdef, deflist->df_name, tp->tp_tpdef, start);
				fputs("    p++;\n", fc);
			    }
			    fprintf(fc, "    p = %s(p, &(a->%s));\n",
					t->tp_mafunc,
					deflist->df_name);
			} else {
			    if (! cnt) start = deflist->df_name;
			    cnt++;
			}
		    }
		    deflist = deflist->df_nextinscope;
		}
		if (cnt) {
		    fprintf(fc, "    p->data = &(a->%s);\n", start);
		    fprintf(fc, "    p->len = sizeof(%s) - offsetof(%s, %s);\n",
			    tp->tp_tpdef, tp->tp_tpdef, start);
		    fputs("    p++;\n", fc);
		}
		break;
		}

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    p = o_rts_marshall(p, a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    p = %s(p, a->o_fields);\n",
				tp->tp_mafunc);
		}
		else {
			fprintf(fc, "    p->data = a->o_fields;\n");
			fprintf(fc, "    p->len = sizeof(%s);\n",
				tp->tp_tpdef);
			fprintf(fc, "    p++;\n");
		}
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fputs("    p->data = (void *)a;\n", fc);
		fputs("    p->len = sizeof(t_union);\n", fc);
	        fputs("    p++;\n", fc);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\tp = %s(p, &(a->u_el.%s));\n",
					t->tp_mafunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tiov->data = (void *)&(a->u_el.%s);\n",
					deflist->df_name);
				fprintf(fc, "\tiov->len = sizeof(%s);\n",
					t->tp_tpdef);
				fprintf(fc, "\tiov++;\n");
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		fputs("    t_elem *e;\n", fc);
		fputs("    int i;\n", fc);
		fputs("    p->data = (void *)&a->s_nelem;\n", fc);
		fputs("    p->len = sizeof(a->s_nelem);\n", fc);
		fputs("    p++;\n", fc);
		fputs("    for (e = a->s_elem; e; e = e->e_next) {\n", fc);
		fputs("\tfor (i = MAXELC-1; i >= 0; i--) if (e->e_mask & (1 << i)) {\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    p = %s(p, &((%s *)(e->e_buf))[i]);\n",
				t->tp_mafunc, t->tp_tpdef);
		}
		else {
			fprintf(fc, "\t    p->data = (void *)&(((%s *)(e->e_buf))[i]);\n",
				t->tp_tpdef);
			fprintf(fc, "\t    p->len = sizeof(%s);\n",
				t->tp_tpdef);
			fprintf(fc, "\t    p++;\n");
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;

	case T_GRAPH:
		fputs("    t_mt *np = a->g_mt;\n    int i, df;\n", fc);
		fputs("    p->data = (void *)&(a->g_size);\n", fc);
		fputs("    p->len = 2*sizeof(a->g_size);\n", fc);
					/* also g_freelist. */
		fputs("    p++;\n", fc);
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    p = %s(p, &(a->g_root));\n",
					t->tp_mafunc);
			}
			else {
				fputs("    p->data = (void *)&(a->g_root);\n", fc);
				fputs("    p->len = sizeof(a->g_root);\n", fc);
				fputs("    p++;\n", fc);
			}
		}
		fputs("    if (a->g_size <= 0) return p;\n", fc);
		fputs("    p->data = (void *)np;\n", fc);
		fputs("    p->len = a->g_size * sizeof(t_mt);\n", fc);
		fputs("    p++;\n", fc);

		t = tp->gra_node;
		fputs("    for (i = a->g_size; i > 0; i--, np++) {\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\tif (! nodeisfree(np)) p = %s(p, np->g_node);\n",
				t->tp_mafunc);
		}
		else {
			fprintf(fc, "\tif (! nodeisfree(np)) {\n");
			fprintf(fc, "\t    p->data = (void *)(np->g_node);\n");
			fprintf(fc, "\t    p->len = sizeof(%s);\n",
				t->tp_tpdef);
			fprintf(fc, "\t    p++;\n");
			fputs("\t}\n", fc);
		}
		fputs("    }\n", fc);
		break;
	}
	fputs("    return p;\n}\n\n", fc);
}

static void
gen_um4_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	t_type	*t;
	t_def	*deflist;

	if (! exported) {
		fputs("static ", fc);
	}
	fprintf(fc, "void %s(void *p, %s *a) {\n",
		tp->tp_umfunc,
		tp->tp_tpdef);
	switch(tp->tp_fund) {
	case T_ARRAY:
		t = element_type_of(tp);
		fprintf(fc, "    %s *s;\n", t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			fputs("    int i;\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#endif\n", fc);
			}
		}
		if (! (tp->tp_flags & T_CONSTBNDS)) {
		    fputs("    pan_msg_consume(p, a, sizeof(*a));\n", fc);
		    fputs("    if (a->a_sz <= 0) { a->a_sz = 0; a->a_data = 0;  return; }\n", fc);
		    fprintf(fc, "    s = (%s *) m_malloc(a->a_sz * sizeof(%s));\n",
			t->tp_tpdef,
			t->tp_tpdef);
		    fputs("    a->a_data = s - a->a_offset;\n", fc);
		}
		else {
		    fprintf(fc, "    s = a->a_data;\n");
		}
		if (t->tp_flags & T_DYNAMIC) {
			if (t->tp_fund == T_GENERIC) {
				fprintf(fc, "#if ch%s(0,1) == 1\n", t->tp_tpdef);
			}
			if (!(tp->tp_flags & T_CONSTBNDS)) {
			    fputs("    for (i = a->a_sz; i > 0; i--, s++) {\n", fc);
			}
			else {
			    fprintf(fc, "    for (i = %d; i > 0; i--, s++) {\n",				tp->arr_size);
			}
			fprintf(fc, "\t%s(p, s);\n", t->tp_umfunc);
			fputs("    }\n", fc);
			if (t->tp_fund == T_GENERIC) {
				fputs("#else\n", fc);
				if (! (tp->tp_flags & T_CONSTBNDS)) {
				    fprintf(fc, "    pan_msg_consume(p, s, a->a_sz * sizeof(%s));\n",
					t->tp_tpdef);
				} else {
				    fprintf(fc, "    pan_msg_consume(p, s, sizeof(*a));\n");
				}
				fputs("#endif\n", fc);
			}
		}
		else {
		    if (! (tp->tp_flags & T_CONSTBNDS)) {
			fprintf(fc, "    pan_msg_consume(p, s, a->a_sz * sizeof(%s));\n",
				t->tp_tpdef);
		    } else {
			fprintf(fc, "    pan_msg_consume(p, s, sizeof(*a));\n");
		    }
		}
		break;

	case T_RECORD: {
		char *start = 0;
		int   cnt = 0;
		
		deflist = tp->rec_scope->sc_def;
		while (deflist) {
		    if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
			if (! start) start = deflist->df_name;
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
			    if (cnt) {
				cnt = 0;
				fprintf(fc, "    pan_msg_consume(p, &(a->%s), offsetof(%s, %s) - offsetof(%s, %s));\n",
					start,
				  	tp->tp_tpdef,
				  	deflist->df_name,
				  	tp->tp_tpdef,
				  	start);
			    }
			    fprintf(fc, "    %s(p, &(a->%s));\n",
					t->tp_umfunc,
					deflist->df_name);
			} else {
			    if (! cnt) start = deflist->df_name;
			    cnt++;
			}
		    }
		    deflist = deflist->df_nextinscope;
		}
		if (cnt) {
		    fprintf(fc, "    pan_msg_consume(p, &(a->%s), sizeof(%s) - offsetof(%s, %s));\n",
			    start,
			    tp->tp_tpdef,
			    tp->tp_tpdef,
			    start);
		}
		break;
		}

	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) break;
		fprintf(fc, "    o_rts_unmarshall(p, a, &%s);\n",
			tp->tp_descr);
		tp = record_type_of(tp);
		fprintf(fc, "    a->o_fields = m_malloc(sizeof(%s));\n",
			tp->tp_tpdef);
		if (tp->tp_flags & T_DYNAMIC) {
			fprintf(fc, "    %s(p, a->o_fields);\n",
				tp->tp_umfunc);
		}
		else {
			fprintf(fc, "    pan_msg_consume(p, a->o_fields, sizeof(%s));\n",
				tp->tp_tpdef);
		}
		break;

	case T_UNION:
		deflist = tp->rec_scope->sc_def;
		fputs("    pan_msg_consume(p, a, sizeof(t_union));\n", fc);
		fputs("    if (! a->u_init) return;\n", fc);
		fprintf(fc, "    switch(a->f_%s) {\n", deflist->df_idf->id_text);
		while ((deflist = deflist->df_nextinscope)) {
			fprintf(fc, "    case %ld:\n",
				deflist->fld_tagvalue->nd_int);
			t = deflist->df_type;
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "\t%s(p, &(a->u_el.%s));\n",
					t->tp_umfunc,
					deflist->df_name);
			}
			else {
				fprintf(fc, "\tpan_msg_consume(p, &(a->u_el.%s), sizeof(%s));\n",
					deflist->df_name,
					t->tp_tpdef);
			}
			fputs("\tbreak;\n", fc);
		}
		fputs("    }\n", fc);
		break;

	case T_SET:
	case T_BAG:
		t = element_type_of(tp);
		fputs("    t_elem *e;\n", fc);
		fputs("    int i, j;\n", fc);
		fputs("    void *ep;\n", fc);
		fputs("    a->s_elem = 0;\n", fc);
		fputs("    pan_msg_consume(p, &a->s_nelem, sizeof(a->s_nelem));\n", fc);
		fputs("    for (i = a->s_nelem; i > 0; i -= MAXELC) {\n", fc);
		fprintf(fc, "\te = m_malloc(sizeof(t_elemhdr) + MAXELC * sizeof(%s));\n", t->tp_tpdef);
		fputs("\tep = e->e_buf;\n", fc);
		fputs("\te->e_next = a->s_elem;\n", fc);
		fputs("\ta->s_elem = e;\n", fc);
		fputs("\tj = MAXELC; if (j > i) j = i;\n", fc);
		fputs("\te->e_count = j;\n", fc);
		fputs("\te->e_mask = 0;\n", fc);
		fputs("\twhile (j-- > 0) {\n", fc);
		fputs("\t    e->e_mask |= (1 << j);\n", fc);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    %s(p, &((%s *)(e->e_buf))[j]);\n",
				t->tp_umfunc, t->tp_tpdef);
		}
		else {
			fprintf(fc, "\t    pan_msg_consume(p, &((%s *)(e->e_buf))[j], sizeof(%s));\n",
				t->tp_tpdef, t->tp_tpdef);
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;

	case T_GRAPH:
		fputs("    t_mt *np;\n    int i, df;\n", fc);
		fputs("    pan_msg_consume(p, &a->g_size, 2*sizeof(a->g_size));\n", fc);
		if ((t = tp->gra_root)) {
			if (t->tp_flags & T_DYNAMIC) {
				fprintf(fc, "    %s(p, &(a->g_root));\n",
					t->tp_umfunc);
			}
			else {
				fprintf(fc, "    pan_msg_consume(p, &(a->g_root), sizeof(%s));\n",
					t->tp_tpdef);
			}
		}
		fputs("    a->g_freenodes = 0; a->g_ndlist = 0;\n", fc);
		fputs("    if (a->g_size == 0) { a->g_mt = 0; return; }\n", fc);
		fputs("    a->g_mt = np = m_malloc(a->g_size * sizeof(t_mt));\n", fc);
		fputs("    pan_msg_consume(p, np, a->g_size * sizeof(t_mt));\n", fc);
		fputs("    for (i = a->g_size; i > 0; i--, np++) {\n", fc);
		fputs("\tif (! nodeisfree(np)) {\n", fc);
		t = tp->gra_node;
		fprintf(fc, "\t    np->g_node = g_getnode(a, sizeof(%s));\n",
			t->tp_tpdef);
		if (t->tp_flags & T_DYNAMIC) {
			fprintf(fc, "\t    %s(p, np->g_node);\n",
				t->tp_umfunc);
		}
		else {
			fprintf(fc, "\t    pan_msg_consume(p, np->g_node, sizeof(%s));\n",
				t->tp_tpdef);
		}
		fputs("\t}\n", fc);
		fputs("    }\n", fc);
		break;
	}
	fputs("}\n\n", fc);
}
