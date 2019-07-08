/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S T A T E M E N T   G R A M M A R */

/* $Id: statement.g,v 1.24 1998/02/27 12:03:23 ceriel Exp $ */

{
#include <alloc.h>

#include "ansi.h"
#include "debug.h"

#include "LLlex.h"
#include "idf.h"
#include "scope.h"
#include "node.h"
#include "def.h"
#include "type.h"
#include "chk.h"
#include "case.h"
#include "options.h"
#include "main.h"
#include "error.h"

}

StatementSeq
	(
	p_node	*st
	)
	{
	p_node	nd;
	}
:
			{ node_initlist(st); }
	[ Statement(&nd)
			{ if (nd) {
				node_enlist(st, nd);
			  }
			}
	]*
;

Statement
	(
	p_node	*st
	)
	{
	p_node	nd;
	p_node	list;
	p_node	expr;
	int	oper;
	t_idf	*opid;
	}
:
	[ /* There's an LL(1) conflict between
	   * assignment, FunctionCallStatement, and
	   * OperationCallStatement.
	   */
	  Designator(st)
	  [
	    /* FunctionCallStatement */
	    ActualParams(&list)
			{ *st = chk_proccall(*st, list); }
	  | /* OperationCallStatement */
	    '$' IDENT	{ opid = dot.tk_idf; }
	    ActualParams(&list)
			{ *st = chk_procopcall(*st, opid, list); }
	  | /* partition or distribution */
	    DOLDOL IDENT
			{ opid = dot.tk_idf;
			  /* Enable special casing in parameter checking:
			     operation names are now allowed, for dependencies.
			     This will be phased out again when the DEPENDENCIES
			     section works to satisfaction.
			  */
			  in_doldol = 1;
			}
	    ActualParams(&list)
			{ *st = chk_doldol(*st, opid, list);
			  in_doldol = 0;
			}
	  | /* assignment */
	    AssignOperator(&oper) Expression(&nd)
			{ *st = chk_assign(*st, nd, oper); }
	  ]
	| ForkStatement(st)
	| EXIT		{ *st = chk_exit(); }
	| ReturnStatement(st)
	| IfStatement(st)
	| CaseStatement(st)
	| REPEAT	{ nd = start_loop(REPEAT); }
	  StatementSeq(&list)
	  UNTIL Expression(&expr)
			{ *st = end_loop(nd, expr, list); }
	| [ WHILE Expression(&expr)
	  |		{ expr = NULLNODE; }
	  ]
	  DO		{ nd = start_loop(DO); }
	  StatementSeq(&list)
	  OD		{ *st = end_loop(nd, expr, list); }
	|
	  ForStatement(st)
	| ACCESS
	  '[' ExpList(&list) ']'
			{ *st = chk_access(list); }
	| /* empty statement */
			{ *st = NULLNODE; }
	]
	';'
;

AssignOperator
	(
	int	*op
	)
:
	BECOMES		{ *op = DOT; }
|	PLUSBECOMES	{ *op = DOT; }
|	MINBECOMES	{ *op = DOT; }
|	TIMESBECOMES	{ *op = DOT; }
|	DIVBECOMES	{ *op = DOT; }
|	MODBECOMES	{ *op = DOT; }
|	B_ORBECOMES	{ *op = DOT; }
|	B_ANDBECOMES	{ *op = DOT; }
|	B_XORBECOMES	{ *op = DOT; }
|	'+' BECOMES	{ *op = PLUSBECOMES; }
|	'|' BECOMES	{ *op = B_ORBECOMES; }
|	'&' BECOMES	{ *op = B_ANDBECOMES; }
|	'^' BECOMES	{ *op = B_XORBECOMES; }
|	'-' BECOMES	{ *op = MINBECOMES; }
|	'*' BECOMES	{ *op = TIMESBECOMES; }
|	'/' BECOMES	{ *op = DIVBECOMES; }
|	'%' BECOMES	{ *op = MODBECOMES; }
|	OR BECOMES	{ *op = ORBECOMES; }
|	AND BECOMES	{ *op = ANDBECOMES; }
|	LEFTSHIFT BECOMES
			{ *op = LSHBECOMES; }
|	RIGHTSHIFT BECOMES
			{ *op = RSHBECOMES; }
;

ForkStatement
	(
	p_node	*st
	)
	{
	p_node	callee;
	p_node	args;
	p_node	target = 0;
	}
:
	FORK
	Qualident(&callee)
	ActualParams(&args)
	[ ON Expression(&target)
	]?
			{ *st = chk_fork(callee, args, target); }
;

IfStatement
	(
	p_node	*st
	)
	{
	p_node	expr;
	p_node	list;
	p_node	nd;
	}
:
	IF
	Expression(&expr)
	THEN
	StatementSeq(&list)
			{ *st = nd = chk_ifstat(expr, list); }
	[ ELSIF Expression(&expr)
	  THEN StatementSeq(&list)
			{ nd = chk_elsifpart(nd, expr, list); }
	]*
	[ ELSE StatementSeq(&list)
			{ add_elsepart(nd, list); }
	| /* empty */
	]
	FI
;

CaseStatement
	(
	p_node	*st
	)
	{
	p_node	expr;
	p_node	list,
		n;
	t_type	*tp;
	int	elsepart = 0;
	}
:
	CASE Expression(&expr)
			{ tp = must_be_discrete_type(expr, "CASE expression"); }
	OF Case(&n, tp)
			{ node_initlist(&list);
			  if (n) node_enlist(&list, n);
			}
	[ '|' Case(&n, tp)
			{ if (n) node_enlist(&list, n); }
	]*
	[ ELSE StatementSeq(&n)
			{ elsepart = 1; }
	| /* empty */
			{ n = 0; }
	]		/* Note that n may be NULL and there still may
			   be an else-part.
			*/
			{ *st = case_analyze(expr, list, n, elsepart); }
	ESAC
;

Case	(
	p_node	*st;
	t_type	*tp
	)
	{
	p_node	ndl;
	p_node	statlist;
	}
:
	CaseLabelList(tp, &ndl) ARROW
	StatementSeq(&statlist)
			{ *st = chk_case(ndl, statlist); }
|
	/* empty */
			{ *st = 0; }
;

CaseLabelList
	(
	t_type	*tp;
	p_node	*exps
	)
	{
	p_node	nd;
	}
:
			{ node_initlist(exps); }
	CaseLabels(tp, &nd)
			{ node_enlist(exps, nd); }
	[ ',' CaseLabels(tp, &nd)
			{ node_enlist(exps, nd); }
	]*
;

CaseLabels
	(
	t_type	*tp;
	p_node	*exps
	)
	{
	p_node	rhs;
	}
:
	ConstExpr(exps)
			{ chk_type_equiv(tp, *exps, "case label"); }
	[ UPTO
	  ConstExpr(&rhs)
			{ *exps = chk_upto(*exps, rhs, tp); }
	| /* empty */
	]
;

ForStatement
	(
	p_node	*st
	)
	{
	p_node	expr,
		rght = 0;
	p_node	list;
	t_idf	*id;
	}
:
	FOR
	IDENT		{ id = dot.tk_idf; }
	IN Expression(&expr)
	[
	  UPTO Expression(&rght)
	| /* empty */
	]
			{ *st = chk_forloopheader(id, expr, rght);
			  /* chk_forloopheader calls start_loop. */
			}
	DO StatementSeq(&list) OD
			{ *st = end_loop(*st, (p_node) 0, list); }
;

ReturnStatement
	(
	p_node	*st
	)
	{
	p_node	expr = 0;
	}
:
	RETURN
	[ Expression(&expr)
	| /* empty */
	]
			{ *st = chk_return(expr); }
;
