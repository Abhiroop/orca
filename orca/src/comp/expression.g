/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* E X P R E S S I O N	 G R A M M A R */

/* $Id: expression.g,v 1.20 1997/05/15 12:01:58 ceriel Exp $ */

{
#include "ansi.h"
#include "debug.h"

#include <stdio.h>
#include <assert.h>
#include <alloc.h>

#include "LLlex.h"
#include "scope.h"
#include "node.h"
#include "type.h"
#include "main.h"
#include "chk.h"
#include "error.h"
#include "oc_stds.h"
#include "options.h"
#include "def.h"

_PROTOTYPE(static int is_MINMAXVAL, (p_node));
}

Qualident
	(
	p_node	*pnd
	)
:
	IDENT		{ *pnd = id2defnode(dot.tk_idf); }
	[ '.' IDENT	{ *pnd = chk_selection(*pnd, dot.tk_idf); }
	]*
;

ConstExpr
	(
	p_node	*exp
	)
:
	Expression(exp)	{ *exp = chk_is_const_expression(*exp); }
;

ExpList	(
	p_node	*explist
	)
	{
	p_node	exp;
	}
:
			{ node_initlist(explist); }
	Expression(&exp)
			{ node_enlist(explist, exp); }
	[ ',' Expression(&exp)
			{ node_enlist(explist, exp); }
	]*
;

Indexor	(
	p_node	lhs;
	p_node	*exp
	)
	{
	p_node	index_list;
	}
:
	'['
	ExpList(&index_list)
	']'
			{ *exp = chk_arrayselection(lhs, index_list); }
|
	'!' IDENT
			{ *exp = chk_graphrootsel(lhs, dot.tk_idf); }
;

Expression
	(
	p_node	*exp
	)
	{
	p_node	left;
	p_node	right;
	int	oper;
	}
:
	SimpleExpr(&left)
	[ [ '='
	  | NOTEQUAL
	  | '<'
	  | '>'
	  | LESSEQUAL
	  | GREATEREQUAL
	  | IN
	  ]		{ oper = DOT; }
	  SimpleExpr(&right)
			{ *exp = chk_relational(left, right, oper); }
	| /* empty */
			{ *exp = left; }
	]
;

SimpleExpr
	(
	p_node	*exp
	)
	{
	int	oper = 0;
	p_node	right;
	}
:
	[ '+'
			{ oper = '+'; }
	| '-'
			{ oper = '-'; }
	| /* empty */
	]
	Term(exp)
			{ if (oper) *exp = chk_unary(*exp, oper); }
	[ [ '+'
	  | '-'
	  | OR
	  | '|'
	  | '^'
	  | LEFTSHIFT
	  | RIGHTSHIFT
	  ]		{ oper = DOT; }
	  Term(&right)
			{ *exp = chk_arithop(*exp, right, oper); }
	]*
;

Term	(
	p_node	*exp
	)
	{
	int	oper;
	p_node	right;
	}
:
	Factor(exp)
	[ [ '*'
	  | '/'
	  | '%'
	  | AND
	  | '&'
	  ]		{ oper = DOT; }
	  Factor(&right)
			{ *exp = chk_arithop(*exp, right, oper); }
	]*
;

Factor	(
	p_node	*exp
	)
	{
	p_node	nd,
		parlist;
	t_type	*tp;
	t_idf	*opid;
	}
:
	/* There's an LL(1) conflict bewteen designator,
	 * FunctionCall, and OperationCall.
	 */
	Designator(exp)
	[ /* MIN, MAX, or VAL */
	  /* These are treated especially because they are the only
	     built-ins that have a type-argument. All other built-ins
	     have arguments that could also be arguments to an ordinary
	     function (and thus can be checked as such).
	  */
	  %if (is_MINMAXVAL(*exp))
	  '(' Qualident(&nd)
			{ node_initlist(&parlist);
			  node_enlist(&parlist, nd);
			}
	  [ ',' Expression(&nd)
			{ node_enlist(&parlist, nd); }
	  ]*
	  ')'
			{ *exp = chk_funcall(*exp, parlist); }
	| /* FunctionCall */
	  ActualParams(&parlist)
			{ *exp = chk_funcall(*exp, parlist); }
	| /* OperationCall */
	  '$' IDENT	{ opid = dot.tk_idf; }
	  ActualParams(&parlist)
			{ *exp = chk_funopcall(*exp, opid, parlist); }
	| /* Aggregate */
	  ':'		{ tp = must_be_aggregate_type(*exp); }
	  [ Aggregate(exp, tp)
	  | ArrayAggregate(exp, tp)
	  ]
	| /* designator */
			{ *exp = chk_designator(*exp); }
	]
|
	/* This alternative is only here because FROM happens to be a
	   keyword as well ...
	*/
	FROM		{ *exp = dot2leaf(Def); }
	ActualParams(&parlist)
			{ *exp = chk_funcall(*exp, parlist); }
|
	INTEGER		{ *exp = dot2leaf(Value); }
|
	REAL		{ *exp = dot2leaf(Value); }
|
	CHARACTER	{ *exp = dot2leaf(Value); }
|
	STRING		{ *exp = dot2leaf(Value); }
|
	'('
	Expression(exp)			/* we leave the '(' as an unary
					   operator so that we can see that we
					   don't have a designator here.
					*/
	')'
			{ *exp = chk_unary(*exp, '('); }
|
	NOT Factor(exp)
			{ *exp = chk_unary(*exp, NOT); }
|
	'~' Factor(exp)
			{ *exp = chk_unary(*exp, '~'); }
;

Aggregate
	(
	p_node	*exp;
	t_type	*tp
	)
	{
	p_node	memlist;
	}
:
	/* for a set, bag, record, or union */
			{ node_initlist(&memlist); }
	'{' ExpList(&memlist)? '}'
			{ *exp = chk_aggregate(memlist, tp); }
;

ArrayAggregate
	(
	p_node	*exp;
	t_type	*tp
	)
	{
	p_node	memlist;
	}
:
	'[' ArrayElList(&memlist) ']'
			{ *exp = chk_array_aggregate(memlist, tp); }
;

ArrayElList
	(
	p_node	*memlist
	)
	{
	p_node	l;
	}
:
			{ node_initlist(memlist); }
[
	ExpList(memlist)
|
	'[' ArrayElList(&l) ']'
			{ add_row(memlist, l); }
	[
	  ','
	  '[' ArrayElList(&l) ']'
			{ add_row(memlist, l); }
	]*
|
	/* empty */
]
;

ActualParams
	(
	p_node	*list
	)
:
			{ node_initlist(list); }
	'(' ExpList(list)? ')'
;

/* some ambiguity here */

Designator
	(
	p_node	*nd
	)
:
	Qualident(nd)
	[
		Indexor(*nd, nd)
		[
			'.' IDENT
			{ *nd = chk_selection(*nd, dot.tk_idf); }
		|
			Indexor(*nd, nd)
		]*
	]?
;

{

static int
is_MINMAXVAL(nd)
	p_node	nd;
{
	return nd->nd_class == Def
	       && nd->nd_type == std_type
	       && (nd->nd_def->df_stdname == S_MIN
		   || nd->nd_def->df_stdname == S_MAX
		   || nd->nd_def->df_stdname == S_VAL);
}

}
