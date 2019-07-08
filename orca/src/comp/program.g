/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* P R O G R A M   G R A M M A R */

/* $Id: program.g,v 1.43 1997/05/15 12:02:51 ceriel Exp $ */

{
#include "ansi.h"
#include "debug.h"

#include <alloc.h>
#include <stdio.h>

#include "idf.h"
#include "LLlex.h"
#include "scope.h"
#include "node.h"
#include "def.h"
#include "type.h"
#include "error.h"
#include "instantiate.h"
#include "chk.h"
#include "flexarr.h"

_PROTOTYPE(static int starts_const_expr, (t_def *));
}

%lexical LLlex;
%start CompUnit, CompilationUnit;
%start UnitSpecification, UnitSpec;
%start GenericSpecification, GenericUnitSpec;

ModuleImpl
	(
	int	generic
	)
	{
	t_def	*df;
	}
:
	MODULE IMPLEMENTATION IDENT
	%substart UnitSpec;
			{ df = start_impl(dot.tk_idf, D_MODULE, generic); }
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(0)
	]*
			{ chk_forwards(); }
	[ FunctionImpl
	| ProcessImpl
	]*
	END		{ end_impl(df); }
;

DataModuleImpl
	(
	int	generic
	)
	{
	t_def	*df;
	t_node	*nd;
	}
:
	DATA MODULE IMPLEMENTATION IDENT
	%substart UnitSpec;
			{ df = start_impl(dot.tk_idf, D_DATA, generic); }
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(0)
	]*
			{ chk_forwards(); }
	FunctionImpl*
	[ BEGIN
			{ start_body(df); }
	  StatementSeq(&nd)
			{ end_body(df, nd); }
	| /* empty */
	]
	END		{ end_impl(df); }
;

GenericUnitImpl
:
	GENERIC
	[ /* Accept generic parameters here too, otherwise the
	     error recovery fails horribly.
	  */
	  %erroneous
	  '('
	  GenericParameter
	  [ ';' GenericParameter
	  ]*
	  ')'
			{ error("generic parameters may only be specified in specification"); }
	]?
	[ ModuleImpl(1)
	| DataModuleImpl(1)
	| ObjectImpl(1)
	]
	GENERIC? /* optional for backwards compatibility */
;

GenericActual
	{
	t_def	*df;
	t_node	*nd;
	}
:
	%if ((df = lookfor(dot.tk_idf, CurrentScope, 1)),
	     starts_const_expr(df))
	ConstExpr(&nd)
			{ add_generic_actual(nd); }
|
	IDENT
			{ add_generic_actual(dot2leaf(Name)); }
;

Import
	{
	t_idlst	idlist;
	t_idf	*id = 0;
	}
:
	[ FROM IDENT
			{ id = dot.tk_idf; }
	| /* empty */
	]
	IMPORT IdentList(&idlist) ';'
	%substart UnitSpec;
			{ handle_imports(id, idlist); }
;

ObjectImpl
	(
	int	generic
	)
	{
	t_def	*df;
	t_node	*nd;
	}
:
	OBJECT IMPLEMENTATION IDENT
	%substart UnitSpec;
			{ df = start_impl(dot.tk_idf, D_OBJECT, generic); }
	shape(df, 1)
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(0)
	| VariableDecl(D_OFIELD)
	]*
			{ chk_forwards(); }
	[ OperationImpl
	| FunctionImpl
	]*
	[ BEGIN
			{ start_body(df); }
	  StatementSeq(&nd)
			{ end_body(df, nd); }
	| /* empty */
	]
	END		{ end_impl(df); }
;

shape
	(
	t_def	*df;
	int	impl
	)
	{
	p_flex	f;
	unsigned int
		dim = 0;
	t_ardim	*a;
	}
:
			{ f = flex_init(sizeof(t_ardim), 2); }
[
			  { a = flex_next(f); }
	'[' dimension(a, impl)
	[		{ a = flex_next(f); }
	  ',' dimension(a, impl)
	]*
	']'
|
	/* Empty */
]
			{ a = flex_finish(f, &dim);
			  chk_shape(df, a, dim, impl);
			}
;

dimension
	(
	t_ardim	*a;
	int	impl
	)
	{
	t_type	*tp;
	p_node	nd1, nd2 = 0;
	}
:
	Qualtype(&tp)
	[
	  IDENT
			{ if (impl) {
				nd1 = dot2leaf(Name);
			  }
			  else {
				error("bound identifier(s) in specification ignored");
			  }
			}
	  UPTO IDENT
			{ if (impl) {
				nd2 = mk_expr(Link, UPTO, nd1, dot2leaf(Name));
			  }
			}
	|
			{ if (impl) {
				error("missing dimension bound identifiers");
			  }
			}
	]
			{ fill_ardim(a, tp, nd2); }
;

GenericInstantiation
	{
	int	kind;
	t_idf	*id;
	}
:
	[ MODULE	{ kind = D_MODULE; }
	| OBJECT	{ kind = D_OBJECT; }
	]
	IDENT		{ id = dot.tk_idf; }
	'=' NEW IDENT	{ start_instantiate(kind, id, dot.tk_idf); }
	'(' GenericActual
	[ %persistent
	  ',' GenericActual
	]*
	')' ';'
	%substart UnitSpec;
			{ finish_instantiate(); }
;

OperationSpec
	(
	t_def	**pdf;
	int	impl
	)
	{
	t_type	*tp = 0;
	t_dflst	param;
	}
:
	OPERATION IDENT	{ *pdf = start_proc(D_OPERATION, dot.tk_idf, impl); }
	FormalParams(D_OPERATION, &param)
	[ ':' Qualtype(&tp)
	]?
	';'		{ check_with_earlier_defs(*pdf, param, tp); }
;

ParallelOperationSpec
	(
	t_def	**pdf;
	)
	{
	t_type	*tp = 0;
	t_def	*reducef = 0;
	t_dflst	param;
	t_idlst	idlist;
	}
:
	PARALLEL OPERATION
	'[' IdentList(&idlist) ']'
	IDENT		{ *pdf = start_proc(D_OPERATION, dot.tk_idf, 1); }
	ParallelFormalParams(&param)
	[ ':' ParallelTypeSpec(&tp, &reducef)
	]?
	';'		{ check_parallel_operation_spec(*pdf, idlist,
						      param, tp, reducef);
			}
;

ParallelTypeSpec
	(
	t_type	**ptp;
	t_def	**df
	)
:
	GATHER Qualtype(ptp)
			{ *ptp = must_be_gather_type(*ptp); }
|
	REDUCE Qualtype(ptp) WITH IDENT
			{ *df = get_reduction_func(dot.tk_idf, *ptp); }
;

OperationImpl
	{
	t_def	*df;
	}
:
	[
	  OperationSpec(&df, 1)
	|
	  ParallelOperationSpec(&df)
	]
	Declaration*
			{ chk_forwards(); }
	OperationBody(df)
			{ end_proc(df, 1); }
;

OperationBody
	(
	t_def	*df
	)
	{
	p_node	nd;
	}
:
	[ DEPENDENCIES
			{ start_dependency_section(df); }
	  StatementSeq(&nd)
	  END ';'
			{ end_dependency_section(df, nd); }
	]?
	BEGIN		{ start_body(df); }
	[ StatementSeq(&nd)
	|
	  [ Guard(&nd)
			{ add_guard(df, nd); }
	  ]+
			{ nd = 0; }
	]
			{ end_body(df, nd); }
	END ';'
;

Guard	(
	p_node	*st
	)
	{
	p_node	expr;
	p_node	list;
	}
:
	GUARD
	Expression(&expr)
	DO StatementSeq(&list)
			{ *st = chk_guard(expr, list); }
	OD ';'
;

FunctionSpec
	(
	t_def	**pdf;
	int impl
	)
	{
	t_type	*tp = 0;
	t_dflst	param;
	}
:
	FUNCTION IDENT	{ *pdf = start_proc(D_FUNCTION, dot.tk_idf, impl); }
	FormalParams(D_FUNCTION, &param)
	[ ':' Qualtype(&tp)
	]?
	';'		{ check_with_earlier_defs(*pdf, param, tp); }
;

FunctionImpl
	{
	t_def	*df;
	}
:
	FunctionSpec(&df, 1)
	[ %prefer
	  Block(df)
			{ end_proc(df, 1); }
	|
	  /* forward declaration; useful for mutually recursive functions
	     that are not mentioned in the specification.
	  */
			{ end_proc(df, 0); }
	]
;

FormalParams
	(
	int	type;
	t_dflst	*pparam
	)
:
			{ def_initlist(pparam); }
	'('
	[ FPSection(type, pparam)
	  [ ';' FPSection(type, pparam)
	  ]*
	]?
	')'
			{ def_endlist(pparam); }
;

ParallelFormalParams
	(
	t_dflst	*pparam
	)
:
			{ def_initlist(pparam); }
	'('
	[ ParallelFPSection(pparam)
	  [ ';' ParallelFPSection(pparam)
	  ]*
	]?
	')'
			{ def_endlist(pparam); }
;

FPSection
	(
	int	type;
	t_dflst	*pparam
	)
	{
	t_idlst	idlist;
	t_type	*tp;
	p_node	bounds_and_tag = 0;
	int	mode;
	}
:
	IdentList(&idlist) ':' FormalSpec(&tp, &mode, &bounds_and_tag)
			{ declare_paramlist(type, pparam, idlist,
				tp, mode, bounds_and_tag, (t_def *) 0);
			}
;

ParallelFPSection
	(
	t_dflst	*pparam;
	)
	{
	t_idlst	idlist;
	t_type	*tp;
	int	mode;
	t_def	*reduction_f = 0;
	}
:
	IdentList(&idlist) ':'
	ParallelFormalSpec(&tp, &mode, &reduction_f)
			{ declare_paramlist(D_OPERATION|D_PARALLEL, pparam,
				idlist, tp, mode, (p_node) 0, reduction_f);
			}
;

FormalSpec
	(
	t_type	**ptp;
	int	*mode;
	t_node	**bounds_and_tag;
	)
:
	IN?	Qualtype(ptp)
			{ *mode = D_INPAR; }
|
	SHARED	Qualtype(ptp)
			{ *mode = D_SHAREDPAR; }
|
	OUT Subtype(ptp, bounds_and_tag)
			{ *mode = D_OUTPAR; }
;

ParallelFormalSpec
	(
	t_type	**ptp;
	int	*mode;
	t_def	**reduction_f;
	)
:
	IN?	Qualtype(ptp)
			{ *mode = D_INPAR; }
|
	OUT ParallelTypeSpec(ptp, reduction_f)
			{ *mode = D_OUTPAR; }
;

Block	(
	t_def	*df
	)
	{
	t_node	*nd;
	}
:
	Declaration*
			{ chk_forwards(); }
	BEGIN
			{ start_body(df); }
	StatementSeq(&nd)
			{ end_body(df, nd); }
	END ';'
;

ProcessSpec
	(
	t_def	**pdf;
	int	impl
	)
	{
	t_dflst	param;
	}
:
	PROCESS IDENT	{ *pdf = start_proc(D_PROCESS, dot.tk_idf, impl); }
	FormalParams(D_PROCESS, &param) ';'
			{ check_with_earlier_defs(*pdf, param, (t_type *) 0); }
;

ProcessImpl
	{
	t_def	*df;
	}
:
	ProcessSpec(&df, 1)
	[ Block(df)
			{ end_proc(df, 1); }
	| /* forward declaration; useful for mutually recursive functions
	     that are not mentioned in the specification.
	  */
			{ end_proc(df, 0); }
	]
;

CompilationUnit
:
	ModuleImpl(0) ';'
|
	DataModuleImpl(0) ';'
|
	ObjectImpl(0) ';'
|
	GenericUnitImpl ';'
;

GenericParameter
	{
	int	kind = T_GENERIC;
	t_def	*df;
	t_type	*tp = 0;
	}
:
[
	[ NUMERIC	{ kind = T_NUMERIC; }
	| SCALAR	{ kind = T_SCALAR; }
	|
	]
	TYPE IDENT	{ df = declare_type(dot.tk_idf, generic_type(kind), 0); }
|
	FunctionParam(&df)
|
	CONST IDENT	{ df = define(dot.tk_idf, CurrentScope, D_CONST); }
	':' Qualtype(&tp)
]
			{ add_generic_formal(df, tp); }
;

FunctionParam
	(
	t_def	**pdf
	)
	{
	t_type	*tp = 0;
	t_dflst	param;
	}
:
	FUNCTION IDENT	{ *pdf = start_proc(D_FUNCTION, dot.tk_idf, 0); }
	FormalParams(D_FUNCTION, &param)
	[ ':' Qualtype(&tp)
	]?
			{ check_with_earlier_defs(*pdf, param, tp);
			  end_proc(*pdf, 0);
			}
;

ModuleSpec
	(
	int	generic
	)
	{
	t_def	*df;
	t_def	*p;
	}
:
	MODULE SPECIFICATION IDENT
			{ df = start_spec(dot.tk_idf, D_MODULE, generic); }
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(1)
	| FunctionSpec(&p, 0)
			{ end_proc(p, 0); }
	| ProcessSpec(&p, 0)
			{ end_proc(p, 0); }
	]*
	END
			{ end_spec(df); }
;

DataModuleSpec
	(
	int	generic
	)
	{
	t_def	*df;
	}
:
	DATA MODULE SPECIFICATION IDENT
			{ df = start_spec(dot.tk_idf, D_DATA, generic); }
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(1)
	| VariableDecl(D_VARIABLE)
	]*
	END
			{ end_spec(df); }
;

ObjectSpec
	(
	int	generic
	)
	{
	t_def	*df;
	t_def	*p;
	}
:
	OBJECT SPECIFICATION IDENT
			{ df = start_spec(dot.tk_idf, D_OBJECT, generic); }
	shape(df, 0)
	';'
	[ Import
	| GenericInstantiation
	| ConstantDecl
	| TypeDecl(1)
	| FunctionSpec(&p, 0)
			{ end_proc(p, 0); }
	]*
	[ OperationSpec(&p, 0)
			{ end_proc(p, 0); }
	]*
	END
			{ end_spec(df); }
;

GenericUnitSpec
:
	GENERIC '('	{ start_generic_formals(); }
	GenericParameter
	[
	  ';' GenericParameter
	]*
	')'		{ end_generic_formals(); }
	[ ModuleSpec(1)
	| DataModuleSpec(1)
	| ObjectSpec(1)
	]
	GENERIC? /* optional for backwards compatibility */
	';'
;

UnitSpec
:
	ModuleSpec(0) ';'
|
	DataModuleSpec(0) ';'
|
	ObjectSpec(0) ';'
|
	GenericUnitSpec
;

{

#if ! defined(__STDC__) || __STDC__ == 0
extern char *strrchr();
#endif

static int
starts_const_expr(df)
	t_def	*df;
{
	/*	This routine is used to solve the LL(1) conflict between
		ConstExpr and IDENT in GenericActual. It returns 1 in case
		of a ConstExpr.
	*/

	t_token	savdot;
	int	retval = 0;

	switch(df->df_kind) {
	case D_CONST:
	case D_ENUM:
		return 1;
	case D_FUNCTION:
		if (df->df_type == std_type) return 1;
		break;
	case D_TYPE:
		/* Could start an aggregate. */
		savdot = dot;
		if (LLlex() == ':') retval = 1;
		aside = dot;
		dot = savdot;
		break;
	}
	return retval;
}
}
