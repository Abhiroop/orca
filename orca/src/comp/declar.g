/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* D E C L A R A T I O N S   G R A M M A R */

/* $Id: declar.g,v 1.22 1997/05/15 12:01:51 ceriel Exp $ */

{

#include "debug.h"

#include <stdio.h>
#include <assert.h>

#include "LLlex.h"
#include "idf.h"
#include "scope.h"
#include "node.h"
#include "def.h"
#include "type.h"
#include "options.h"
#include "error.h"
#include "chk.h"
#include "flexarr.h"

}

Qualtype
	(
	p_type	*ptp
	)
	{
	p_node	nd;
	}
:
	/* Qualtype is used when a Qualident must indicate a type.
	*/
	Qualident(&nd)	{ *ptp = qualified_type(nd); }
;

Subtype	(
	p_type	*ptp;
	p_node	*bounds_and_tag
	)
	{
	p_type	tp;
	p_node	nd;
	}
:
			{ node_initlist(bounds_and_tag); }
[
	Qualtype(ptp)	{ tp = *ptp; }
	[
	  Bounds(&nd, tp)
			{ node_enlist(bounds_and_tag, nd);
			  tp = get_element_type(tp);
			}
	]*
	[ '(' BoundsOrTagExpr(&nd) ')'
			{ node_enlist(bounds_and_tag, nd);
			  (void) get_union_variant(tp, nd);
			}
	]?
|
	NonScalarType(ptp)
|
	Enumeration(ptp)
]
;

Bounds	(
	p_node	*bounds;
	p_type	tp
	)
	{
	int	i = 0;
	p_node	nd;
	p_type	itp;
	}
:
	'['		{ itp = get_index_type(tp, i); i++; }
	ArrayBounds(bounds, itp)
	[
			{ itp = get_index_type(tp, i); i++; }
	   ',' ArrayBounds(&nd, itp)
			{ *bounds = mk_expr(Link, ',', *bounds, nd);
			  bounds = &((*bounds)->nd_right);
			}
	 ]*
	']'
			{ chk_enough_bounds(tp, i); }
;

ArrayBounds
	(
	p_node	*bounds;
	p_type	itp
	)
	{
	p_node	lb, ub;
	}
:
	BoundsOrTagExpr(&lb) UPTO BoundsOrTagExpr(&ub)
			{ *bounds = chk_upto(lb, ub, itp); }
;

BoundsOrTagExpr
	(
	p_node	*pnd
	)
:
	/* BoundsOrTagExpr is used whenever an Expression is required
	   that must obey the bounds-or-tag expression rules.
	*/
	Expression(pnd)	{ *pnd = chk_bat(*pnd); }
;

Declaration
:
	ConstantDecl
|
	TypeDecl(0)
|
	VariableDecl(D_VARIABLE)
;

ConstantDecl
	{
	p_idf	id;
	p_node	cst;
	}
:
	CONST IDENT	{ id = dot.tk_idf; }
	'=' ConstExpr(&cst) ';'
			{ (void) declare_const(id, cst); }
;

TypeDecl(
	int	allow_opaque
	)
	{
	p_type	tp = 0;
	p_idf	id;
	}
:
	TYPE IDENT	{ id = dot.tk_idf; }
	[ '=' Type(&tp)
	| /* opaque type, empty */
	]
			{ declare_type(id, tp, allow_opaque); }
	';'
;

Type	(
	p_type	*ptp
	)
:
	Qualtype(ptp)
|
	Enumeration(ptp)
|
	NonScalarType(ptp)
;

NonScalarType
	(
	p_type	*ptp
	)
:
			{ start_nonscalar(); }
[
	RecordType(ptp)
  |
	UnionType(ptp)
  |
	ArrayType(ptp)
  |
	SetType(ptp)
  |
	BagType(ptp)
  |
	GraphType(ptp)
  |
	NodeNameType(ptp)
  |
	FunctionType(ptp)
]
			{ end_nonscalar(*ptp); }
;

FunctionType
	(
	p_type *ptp
	)
	{
	t_dflst	param;
	p_type	tp = 0;
	}
:
	FUNCTION
	FormalTypeList(&param)
	[ ':' Qualtype(&tp)
	]?
			{ *ptp = funcaddr_type(tp, param); }
;

FormalTypeList
	(
	t_dflst	*param
	)
:
	'('		{ def_initlist(param); }
	[ ModeFormalType(param)
	  [ ',' ModeFormalType(param)
	  ]*
	]?
	')'
			{ def_endlist(param); }
;

ModeFormalType
	(
	t_dflst	*pparam
	)
	{
	p_type	tp;
	int	mode;
	t_idlst	idlist;
	}
:
	[ IN		{ mode = D_INPAR; }
	| OUT		{ mode = D_OUTPAR; }
	| SHARED	{ mode = D_SHAREDPAR; }
	| /* empty */	{ mode = D_INPAR; }
	]
	Qualtype(&tp)
			{ idf_dummylist(&idlist);
			  declare_paramlist(D_FUNCTION, pparam, idlist,
					   tp, mode, (p_node) 0, (p_def) 0);
			}
;

SetType	(
	p_type	*ptp
	)
:
	SET OF Type(ptp)
			{ *ptp = set_type(*ptp); }
;

BagType	(
	p_type	*ptp
	)
:
	BAG OF Type(ptp)
			{ *ptp = bag_type(*ptp); }
;

Enumeration
	(
	p_type	*ptp
	)
	{
	t_idlst	id_list;
	}
:
	'(' IdentList(&id_list) ')'
			{ *ptp = enum_type(id_list); }
;


IdentList
	(
	t_idlst	*id_list
	)
:
			{ idf_initlist(id_list); }
	IDENT		{ idf_enlist(id_list, dot.tk_idf); }
	[ ',' IDENT	{ idf_enlist(id_list, dot.tk_idf); }
	]*
			{ idf_endlist(id_list); }
;

RecordType
	(
	p_type	*ptp
	)
	{
	p_scope	scope = open_and_close_scope(OPENSCOPE);
	}
:
	RECORD FieldListSeq(scope) END
			{ *ptp = record_type(scope); }
;

FieldListSeq
	(
	p_scope	scope
	)
:
	[ FieldList(scope) ';'
	]*
			{ DO_DEBUG(options['S'], dump_scope(scope)); }
;

FieldList
	(
	p_scope	scope
	)
	{
	p_type	tp;
	t_idlst	id_list;
	p_node	bounds_and_tag;
	}
:
	IdentList(&id_list) ':' Subtype(&tp, &bounds_and_tag)
			{ declare_fieldlist(id_list, tp, bounds_and_tag, scope);
			}
;

UnionType
	(
	p_type	*ptp
	)
	{
	p_scope	scope;
	p_type	tp;
	p_idf	id;
	p_node	nd = 0;
	}
:
	UNION		{ scope = open_and_close_scope(OPENSCOPE); }
	'(' IDENT	{ id = dot.tk_idf; }
	':' Qualtype(&tp)
	[ '(' BoundsOrTagExpr(&nd) ')'
	]?
	')'
			{ declare_unionel(id, tp, (p_node) 0,
					 scope, 1, NULLNODE, (p_type) 0);
			}
	Alternative(scope, tp)+
	END
			{ *ptp = union_type(scope, nd); }
;

Alternative
	(
	p_scope	scope;
	p_type	tagtype
	)
	{
	p_idf	id;
	p_type	tp;
	p_node	cst;
	p_node	bounds_and_tag;
	}
:
	ConstExpr(&cst) ARROW IDENT
			{ id = dot.tk_idf; }
	':' Subtype(&tp, &bounds_and_tag) ';'
			{ declare_unionel(id, tp, bounds_and_tag,
					 scope, 0, cst, tagtype);
			}
;

ArrayType
	(
	p_type	*ptp
	)
	{
	p_ardim	dims;
	unsigned int
		ndim;
	}
:
	ARRAY IndexSpec(&dims, &ndim) OF Type(ptp)
			{ *ptp = array_type(*ptp, (int) ndim, dims); }
;

bound	(
	p_ardim	dim
	)
	{
	t_type	*tp;
	p_node	nd = 0;
	}
:
	Qualtype(&tp)
	ArrayBounds(&nd, tp)?
			{ fill_ardim(dim, tp, nd); }
;

IndexSpec
	(
	p_ardim	*pdims;
	unsigned int
		*ndim
	)
	{
	p_ardim a;
	p_flex	f;
	}
:
			{ f = flex_init(sizeof(t_ardim), 2);
			  a = flex_next(f);
			}
	'[' bound(a)
	 [		{ a = flex_next(f); }
	   ',' bound(a)
	 ]*
	']'
			{ *pdims = flex_finish(f, ndim); }
;

GraphType
	(
	p_type	*ptp
	)
	{
	p_scope	gscope,
		nscope;
	}
:
	GRAPH		{ gscope = open_and_close_scope(OPENSCOPE); }
	FieldListSeq(gscope)
	NODES		{ nscope = open_and_close_scope(OPENSCOPE); }
	FieldListSeq(nscope)
	END		{ *ptp = graph_type(gscope, nscope); }
;

NodeNameType
	(
	p_type	*ptp
	)
:
	NODENAME OF IDENT
			{ *ptp = nodename_of_ident(dot.tk_idf); }
;

VariableDecl
	(
	int	kind
	)
	{
	p_type	tp;
	p_node	bounds_and_tag;
	t_idlst	id_list;
	}
:
	IdentList(&id_list) ':' Subtype(&tp, &bounds_and_tag) ';'
			{ declare_varlist(id_list, tp, bounds_and_tag, kind); }
;
