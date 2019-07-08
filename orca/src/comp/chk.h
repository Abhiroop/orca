/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __CHK_H__
#define __CHK_H__

/* E X P R E S S I O N	 C H E C K I N G */

/* $Id: chk.h,v 1.18 1997/05/15 12:01:40 ceriel Exp $ */

#include	"ansi.h"
#include	"idf.h"
#include	"node.h"
#include	"type.h"
#include	"def.h"

/* Prototypes for functions of chk.c
*/

_PROTOTYPE(void mark_defs, (p_node nd, int flags));
	/*	D_USED|D_DEFINED marking. The designator indicated by 'nd' is
		used or defined, depending on the 'flags' value.
		Propagates this fact down the 'nd' tree, marking any definitions
		found along the way.
	*/

_PROTOTYPE(p_node chk_is_const_expression, (p_node exp));
	/*	Checks that the expression indicated by 'exp' is a constant
		expression. If it is not, an error message is produced.
		Returns the expression, or, in case of an error, a suitable
		substitute.
	*/

_PROTOTYPE(p_node chk_selection, (p_node nd, p_idf id));
	/*	Checks that 'id' is a valid selection of the expression node
		indicated by 'nd'. Returns the resulting selection expression
		tree.
	*/

_PROTOTYPE(p_node chk_arrayselection, (p_node lhs, p_node index_list));
	/*	Checks a construction that looks like an array selection.
		It could be either that or a graph node selection.
		Returns the resulting selection expression tree.
	*/

_PROTOTYPE(p_node chk_graphrootsel, (p_node nd, p_idf id));
	/*	Checks that 'id' is a valid ! selection of a graph root:
		It must be a field of the root of the graph, it must be a
		nodename of this graph type.
		(Note: g!x is a short-hand for g[g.x]).
		Returns the resulting selection expression tree.
	*/

_PROTOTYPE(p_node chk_relational, (p_node left, p_node right, int oper));
	/*	Checks a relational expression and performs it when possible.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_unary, (p_node opnd, int oper));
	/*	Checks an unary operator and performs it when possible.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_arithop, (p_node left, p_node right, int oper));
	/*	Checks a binary operator and performs it when possible.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_designator, (p_node desig));
	/*	Checks a designator, occuring in an expression.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_aggregate, (p_node memlist, p_type aggr_type));
	/*	Checks an aggregate: the members must have the appropriate
		type, the number of members must be correct.
		Also, if all members are constants, the aggregate is marked
		constant. (Currently not for sets and bags).
		Returns the expression tree for the aggregate.
	*/

_PROTOTYPE(p_node chk_array_aggregate, (p_node memlist, p_type aggr_type));
	/*	Like chk_aggregate, but for array aggregates.
	*/

_PROTOTYPE(p_node chk_funcall, (p_node fundesig, p_node args));
	/*	Checks a function call (with a result).
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_proccall, (p_node procdesig, p_node args));
	/*	Checks a procedure call (without a result).
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_funopcall, (p_node obj, p_idf operation, p_node args));
	/*	Checks an operation call which should return a result.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_procopcall, (p_node obj, p_idf operation, p_node args));
	/*	Checks an operation call which should not return a result.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_assign, (p_node dsg, p_node rhs, int oper));
	/*	Checks an assignment statement. The lhs (designator) must be
		"assignable". For an ordinary assignment, the type of the rhs
		must be assigmnent compatible with that of the lhs. For an
		assignment operator, chk_assign calls chk_arithop to do
		the work. Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_upto, (p_node left, p_node right, p_type tp));
	/*	Checks the left and right side of an UPTO expression,
		in a specification of bounds in an array or partitioned object
		declaration, or in a FOR loop.
		The supposed type of the bounds is in tp.
		Returns an UPTO link to both expressions.
	*/

_PROTOTYPE(p_node chk_fork, (p_node desig, p_node args, p_node on_expr));
	/*	Checks a FORK statement. The designator must indicate a process,
		parameters must be correct, and if there is an ON expression,
		it must be an integer.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_exit, (void));
	/*	Checks an exit statement. It must reside in a loop.
		The statement tree is returned.
	*/

_PROTOTYPE(p_node chk_guard, (p_node expr, p_node statlist));
	/*	Checks a guard statement. The resulting statement tree is
		returned.
	*/

_PROTOTYPE(p_node chk_ifstat, (p_node expr, p_node statlist));
	/*	Checks the if-then part of an IF statement. The resulting
		statement tree is returned.
	*/

_PROTOTYPE(p_node chk_elsifpart, (p_node ifpart, p_node expr, p_node statlist));
	/*	Makes the ELSIF part look like a nested IF. Note that a possible
		ELSE or ELSIF part later on belongs to this IF, not to the
		original one. The place-holder for the new IF is returned.
		It should only be used to connect later parts.
	*/

_PROTOTYPE(void add_elsepart, (p_node ifpart, p_node statlist));
	/*	Connects the else-part to the ifpart.
	*/

_PROTOTYPE(p_node chk_case, (p_node explist, p_node statlist));
	/*	Returns the case entry that associates the cases indicated
		with explist with the statement list statlist.
	*/

_PROTOTYPE(p_node chk_return, (p_node expr));
	/*	Performs a number of checks on a RETURN:
		- the presence/absence of an expression;
		- the result type for compatibility.
		Returns the resulting statement tree.
	*/

_PROTOTYPE(p_node chk_bat, (p_node expr));
	/*	Checks that the expression in "expr" may be used as a bounds-
		or tag-expression. This means that it must either be a
		constant expression or only use shared or input parameters.
		Returns the resulting expression tree.
	*/

_PROTOTYPE(p_node chk_forloopheader, (p_idf id, p_node left, p_node right));
	/*	Indicates that identifier indicated by 'id' is used as a
		FOR-loop control variable. Checks that it does not hide any
		other declarations of the same name.
		Also checks the FOR-loop expression (indicated by 'left' and,
		in case of a range, 'right'.
		Declares the FOR-loop variable.
		Calls start_loop() and returns the resulting statement tree.
	*/

_PROTOTYPE(p_node start_loop, (int kind));
	/*	Notifies the start of a loop. kind must be either DO, REPEAT,
		or FOR.	 Returns the loop statement tree.
	*/

_PROTOTYPE(p_node end_loop, (p_node loop, p_node expr, p_node statlist));
	/*	Notifies the end of a loop. The body of the loop is indicated
		by 'statlist', the loop-expression (in case of a WHILE ... DO
		or REPEAT UNTIL ....) is indicated by 'expr'.
		The resulting statement tree is returned.
	*/

_PROTOTYPE(void start_body, (p_def df));
	/*	Indicates the start of the body of module/object/operation/
		function/process 'df'.
	*/

_PROTOTYPE(void end_body, (p_def df, p_node body));
	/*	Indicates the end of the body of module/object/operation/
		function/process 'df'. The body itself is indicated by 'body'.
	*/

_PROTOTYPE(void add_guard, (p_def operation_def, p_node stat));
	/*	Adds the guard-statement indicated by 'stat' to the body
		of the operation indiciated by 'operation_def'.
	*/

#define DIFFERENT	0
#define SIMILAR		1
#define SAME		2

_PROTOTYPE(int cmp_designators, (p_node arg1, p_node arg2));
	/*	Compares two designators. Returns DIFFERENT if they cannot be
		aliases, SIMILAR if determining that would require a runtime
		test, and SAME if they are aliases.
	*/

_PROTOTYPE(int chk_aliasing, (p_node arg1, p_node arg2,
			      p_def df1, p_def df2,
			      int give_warning));
	/*	Checks if the two parameters are possible aliases of each other;
		arg1 and arg2 are the actual parameters, df1 and df2 the
		formal ones.  If they are aliases, generate an error message.
		If a run-time check is required, gives a warning if give_warning
		is set. Return like cmp_designators().
	*/

/* Some routines for the data-parallel extension of Orca. */

_PROTOTYPE(p_node chk_access, (p_node access_list));
	/*	Checks that the ACCESS statement resides in a dependency
		section. Returns the resulting statement tree.
	*/

extern int	in_doldol;
	/*	Flag, to be set by the parser when processing a $$ call.
		In this case, parameters may be operations as well (to specify
		dependencies).
	*/

_PROTOTYPE(p_node chk_doldol, (p_node objdesig, p_idf id, p_node args));
	/*	Check a $$ call on partitioned object 'objdesig', with
		arguments indicated by 'args'. Currently recognized are:
		'partition', 'distribute', 'distribute_on_n',
		'distribute_on_list', clear_dependencies', 'set_dependencies',
		'add_dependency', and 'remove_dependency'.
		Returns the resulting statement tree.
	*/

_PROTOTYPE(void chk_shape, (p_def df, p_ardim a, int ndim, int impl));
	/*	Indicates that the partitioned object indicated by 'df'
		has number of dimensions 'ndim' and dimension types and
		bound names as indicated by 'a'.
		The dimension types are specified in the implementation as well
		as in the specification. 'impl' is set if the shape-spec
		was found in the implementation. If it is set, chk_shape()
		checks that the shape_spec matches with the one found in the
		specification.
	*/

_PROTOTYPE(void start_dependency_section, (p_def operation));
	/*	Indicates the start of a dependency section for the parallel
		operation indicated by 'operation'.
	*/

_PROTOTYPE(void end_dependency_section, (p_def operation, p_node statlist));
	/*	Indicates the end of a dependency section for the parallel
		operation indicated by 'operation'. The dependency section tree
		is indicated by 'statlist'.
	*/

#endif /* __CHK_H__ */
