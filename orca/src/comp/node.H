/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __NODE_H__
#define __NODE_H__

/* N O D E   O F   A N	 A B S T R A C T   P A R S E T R E E */

/* $Id: node.H,v 1.25 1997/07/10 12:34:14 ceriel Exp $ */

#include	"ansi.h"
#include	"LLlex.h"

typedef struct node	t_node, *p_node;
typedef struct nd_optim	t_optim, *p_optim;

/* The node structure is the central datastructure in the compiler.
   Statements as well as expression nodes are expressed through nodes.

   The fields in a node structure are:

   nd_prev, nd_next
		(p_node)
			links nodes together in a doubly linked list.
   nd_type	(p_type)
			indicates the type of an (expression) node.
   nd_symb	(short)	indicates the token number of this node. It has one
			the values as assigned by LLgen.
   nd_class	(short)	indicates the kind of node. Its possible values are
			described below.
   nd_flags	(int)	flags set for this node. Its possible values are
			described below.
   nd_nodenum	(int)	node numbering for the optimizer.
   nd_pos	(t_pos)	ths position in the compilation unit corresponding to
			this node.
   nd_o_info	(p_optim)
			some optimizer information, currently only some extra
			place holders.
   nd_value	...	a union containing several fields, depending on the
			value of nd_class.
*/

/* The possible values of nd_class are: */

#define Value	0x0001		/* a constant */
#define Arrsel	0x0002		/* an array or graph selection */
#define Oper	0x0004		/* a binary operator */
#define Uoper	0x0008		/* an unary operator */
#define Call	0x0010		/* a function/operation/FORK call */
#define Name	0x0020		/* an (unidentified) identifier */
#define Aggr	0x0040		/* an aggregate */
#define Row	0x0080		/* a row within an array aggregate */
#define Def	0x0100		/* an identified name */
#define Stat	0x0200		/* a statement */
#define Link	0x0400		/* for UPTO */
#define Select	0x0800		/* graph, record, or union selection */
#define Tmp	0x1000		/* a temporary variable */
#define Check	0x2000		/* a run-time check */
#define Ofldsel	0x4000		/* an object field selection of a partitioned
				   object
				*/

/* The possible values of nd_flags are: */

#define ND_LASTGUARD	0x0001	/* set in Stat nodes with nd_symb = GUARD, when
				   it is the last read or write guard of an
				   operation.
				*/
#define ND_ALIAS	0x0001	/* set in expression nodes representing an IN
				   parameter that is an alias of another
				   pararmeter (so that it probably must be
				   copied.
				*/
#define ND_CONST	0x0002	/* for constant expressions */
#define ND_WRITES	0x0004	/* for writing alts in operation */
#define ND_BLOCKS	0x0008	/* for blocking alternatives: set in the
				   GUARD node if the body may block,
				   and in its nd_expr node if the guard
				   expression may block.
				*/
#define ND_NODESIG	0x0010	/* set if unary operators '(' and/or '+'
				   are removed, so that we can still see
				   that it was not a designator.
				*/
#define ND_HAS_ELSEPART	0x0020	/* for CASE statements with an ELSE part */
#define ND_LHS		0x0020	/* For Tmp, set if it is the LHS of an
				   assignment.
				*/
#define ND_LARGE_RANGE	0x0040	/* for cases that indicate a large range */
#define ND_RETVAR	0x0040	/* a Tmp that stands for the return variable.
				   It has no nd_tmpvar!
				*/
#define ND_GENERICPAR	0x0080	/* for constant generic formal parameters, to
				   indicate that although the thing is constant,
				   we don't know the value.
				*/
#define ND_FORDONE	0x0080	/* for a FOR-statement, set if it is
				   executed at least once.
				*/
#define ND_EXIT_OR_RET	0x0100	/* For loops, set if they contain an exit
				   or return statement.
				*/
#define ND_SUITABLE_SR	0x0200	/* Set for expressions that definately are
				   suitable for strength reduction.
				*/
#define ND_NO_SR	0x0400	/* Set for expressions that definately are not
				   suitable for strength reduction.
				*/
#define ND_OFLD_SELECTED \
			0x0800	/* Set for partitioned object fields for which
				   the indexing turned out to be the current
				   operation index and thus was removed.
				*/

/* For nd_class = Stat, the following fields exist:
   nd_expr	(p_node)
			for assignment operators: the expression to be assigned.
			for RETURN: the expression to be returned.
			for COND_EXIT: the boolean expression on which the
			loop is to be exited.
			for IF: the boolean if-expression.
			for CASE: the case-expression.
			for FOR: the for-loop range/set expression.
			for GUARD: the guard expression.
   nd_desig	(p_node)
			for assignment operators: the designator assigned to.
			for FOR: the for-loop variable.
   nd_list1	(p_node)
			for FOR, DO: the loop body.
			for GUARD: the guard body.
			for IF: the if-part.
			for CASE: the list of case parts (which is a list of
			"ARROW" entries.
			for ARROW: the list of case labels belonging to this
			case entry.
			for ANDBECOMES, ORBECOMES: code to be executed
			when the RHS must be evaluated. The RHS itself is
			evaluated in nd_list1, so if nd_list1 is non-zero,
			nd_expr is (and vice versa).
   nd_list2	(p_node)
			for ARROW: the list of statements belonging to this
			case entry.
			for IF: the else-part.
			for CASE: the else-part.
			for FOR: after optimization: code that has to be
			executed once, when we know that the loop is executed
			at least once.
   Note: for most statements, the corresponding node structure directly reflects
   the source statement. However, REPEAT and WHILE statements are internally
   represented as a DO loop with a COND_EXIT (conditional exit) in the loop
   body. A COND_EXIT has a boolean expression associated with it. The effect
   is an EXIT if the expression evaluates to true.
   Another exception is the FOR statement, which has an UPDATE appended to the
   end of the statement list.
*/

struct nd_Stat {
    p_node	ndx_expr,
		ndx_desig,
		ndx_list1,
		ndx_list2;
#define nd_expr		nd_u.nd_Stat.ndx_expr
#define nd_desig	nd_u.nd_Stat.ndx_desig
#define nd_list1	nd_u.nd_Stat.ndx_list1
#define nd_list2	nd_u.nd_Stat.ndx_list2
};

/* For nd_class = Call, the following fields exist:
   nd_callee	(p_node)
			the function/process/operation being called.
   nd_parlist	(p_node)
			the parameter list.
   nd_target	(p_node)
			only exists if the return value of the call is
			assigned to a designator: the designator.
			For FORKs: the target processor.
   nd_obj	(p_node)
			for an operation call: the object on which the operation
			is applied.
*/

struct nd_Call {
    p_node	ndx_callee,
		ndx_parlist,
		ndx_target,
		ndx_obj;
#define nd_callee	nd_u.nd_Call.ndx_callee
#define nd_parlist	nd_u.nd_Call.ndx_parlist
#define nd_target	nd_u.nd_Call.ndx_target
#define nd_obj		nd_u.nd_Call.ndx_obj
};

/* For nd_class = Oper, Uoper, Arrsel, Select, Ofldsel, Link, and Check,
   the following fields exist:
   nd_left	(p_node)
			The LHS of the expression, or the designator from
			which the selection is done. Does not exist for Uoper.
   nd_right	(p_node)
			The RHS of the expression, or the selector.
			For Uper, nd_right contains the operand.
   nd_dimno	(int)
			This field only exists for the internal operators
			ARR_SIZE and ARR_INDEX. For an expression A[i, j], the
			index expression is converted to
			ARR_INDEX(i,0) * ARR_SIZE(A,1) + ARR_INDEX(j,1).
			ARR_INDEX is an binary operator with two operands:
			the array and the index, and nd_dimno the dimension
			number (starting at 0).
			The ARR_INDEX operator is remmoved in prepare.c
			(after adding an array bound check).
			ARR_SIZE is an unary operator with one operand: the
			array, and nd_dimno the dimension number. It returns
			the number of elements in dimension nd_dimno of the
			array.
  nd_count	(int)
			This field only exists for Link. It may indicate the
			number of elements in a range, in case it is known.
*/

struct nd_Oper {
    p_node	ndx_left,
		ndx_right;
    int		ndx_dimno;
#define nd_left		nd_u.nd_Oper.ndx_left
#define nd_right	nd_u.nd_Oper.ndx_right
#define nd_dimno	nd_u.nd_Oper.ndx_dimno
#define nd_count	nd_u.nd_Oper.ndx_dimno
};

/* For nd_class = Aggr and Row, the following fields exist:
   nd_memlist	(p_node)
			The list of aggregate members.
   nd_dummynam	(char *)
			The C-identifier produced for the aggregate, in case
			it is anonymous.
   nd_nelements	(int)	The number of elements in the aggregate.
*/

struct nd_Aggr {
    p_node	ndx_memlist;
    char	*ndx_dummynam;
    int		ndx_nelements;
#define nd_memlist	nd_u.nd_Aggr.ndx_memlist
#define nd_dummynam	nd_u.nd_Aggr.ndx_dummynam
#define nd_nelements	nd_u.nd_Aggr.ndx_nelements
};

/* For nd_class = Def, the following field exists:
   nd_def	(p_def)	indicates the def structure for this identified name.
*/

struct nd_Def {
    struct def	*ndx_def;
#define nd_def		nd_u.nd_Def.ndx_def
};

/* For nd_class = Value, the following fields exist:
   nd_int	(long)	the value of the node, in case it is a discrete type.
   nd_str	(p_string)
			indicates the string structure associated with the
			string, in case it is one.
   nd_string	(char *)
			short-hand to get the s_str field of nd_str.
   nd_slen	(unsigned)
			short-hand to get the s_length field of nd_str.
   nd_real	(p_real)
			the value of the node, in case it is a floating point
			type.
*/

union nd_Value {
    long	ndx_int;
    p_string	ndx_str;
    p_real 	ndx_real;
#define nd_int		nd_u.nd_Value.ndx_int
#define nd_str		nd_u.nd_Value.ndx_str
#define nd_real		nd_u.nd_Value.ndx_real
#define nd_string	nd_str->s_str
#define nd_slen		nd_str->s_length
};

/* For nd_class = Name, the following field exists:
   nd_idf	(p_idf)	indicates the idf structure for this unidentified name.
			This node type should not occur after preparing the
			tree.
*/

struct nd_Name {
    struct idf	*ndx_idf;
#define nd_idf		nd_u.nd_Name.ndx_idf
};

/* For nd_class = Tmp, the following field exists:
   nd_tmpvar	(p_tmp)	indicates the tmp structure belonging to this
			temporary.
*/

struct nd_Tmp {
    struct tmp	*ndx_tmpvar;
#define nd_tmpvar	nd_u.nd_Tmp.ndx_tmpvar
};

struct node {
    p_node	nd_prev,
		nd_next;
    struct type	*nd_type;
    short	nd_symb;
    short	nd_class;	/* kind of node */
    int		nd_flags;
    int		nd_nodenum;	/* For numbering nodes. */
    p_optim	nd_o_info;
    t_pos	nd_pos;		/* position where this node occurred */
#define nd_lineno	nd_pos.pos_lineno
#define nd_filename	nd_pos.pos_filename
    union {
	struct nd_Stat	nd_Stat;
	struct nd_Oper	nd_Oper;
	struct nd_Call	nd_Call;
	struct nd_Aggr	nd_Aggr;
	struct nd_Def	nd_Def;
	struct nd_Name	nd_Name;
	struct nd_Tmp	nd_Tmp;
	union nd_Value	nd_Value;
    } nd_u;
};

/* ALLOCDEF "node" 100 */

struct nd_optim {
    int		o_ndnum[2];	/* Some extra place holders. */
};

/* ALLOCDEF "nd_optim" 20 */

#define node_enlist(p, ep) \
	do { \
		p_node	_e = (ep); \
		(_e)->nd_next = 0; \
		if (*p) { \
			(_e)->nd_prev = (*p)->nd_prev; \
			(*p)->nd_prev->nd_next = (_e); \
			(*p)->nd_prev = (_e); \
		} \
		else { \
			(_e)->nd_prev = (_e); \
			(*p) = (_e); \
		} \
	} while (0)

#define node_fromlist(p, ep) \
	do { \
		p_node	_e = (ep); \
		if ((_e)->nd_next) (_e)->nd_next->nd_prev = (_e)->nd_prev; \
		else if ((_e)->nd_prev) (_e)->nd_prev->nd_next = 0; \
		if ((_e)->nd_prev && (_e)->nd_prev->nd_next) { \
			(_e)->nd_prev->nd_next = (_e)->nd_next; \
		} \
		if (*(p) == _e) *(p) = (_e)->nd_next; \
	} while (0)

#define node_insert(p, ep) \
	do { \
		p_node	_e = (ep); \
		if (*p) { \
			(_e)->nd_prev = (*p)->nd_prev; \
			(_e)->nd_next = (*p); \
			if ((*p)->nd_prev->nd_next) { \
				(*p)->nd_prev->nd_next = (_e); \
			} \
			(*p)->nd_prev = (_e); \
		} \
		else { \
			(_e)->nd_prev = (_e); \
			(_e)->nd_next = 0; \
			(*p) = (_e); \
		} \
	} while (0)

#define node_initlist(p)	(*(p) = 0)
#define node_emptylist(p)	((p) == 0)
#define node_getlistel(a)	(a)
#define node_nextlistel(a)	((a) ? (a)->nd_next : 0)
#define node_prevlistel(a)	((a) ? (a)->nd_prev : 0)
#define node_killlist(a)

#define node_walklist(a,b,c) \
	for (b = (a); \
	     (c = b, b = node_nextlistel(b)), c != 0; \
	    )

#define node_reversewalklist(a,b,c) \
	for (b = node_prevlistel(a); \
	     (c = b, b = node_prevlistel(b)), c != 0; \
	    )

_PROTOTYPE(p_node mk_expr, (int class, int symb, p_node left, p_node right));
	/*	Creates an expression node of class 'class', with symbol
		'symb', and 'left' and 'right' pointers. The position is
		inherited from dot.
	*/

_PROTOTYPE(p_node mk_leaf, (int class, int symb));
	/*	Creates a leaf node of class 'class', with symbol 'symb'.
		The position is inherited from dot.
	*/

_PROTOTYPE(p_node dot2leaf, (int class));
	/*	Creates a leaf node of class 'class', with all attributes
		inherited from dot.
	*/

_PROTOTYPE(void add_row, (p_node *memlist, p_node list));
	/*	Adds the row indicated by 'list' to the aggregate member list
		indicated by 'memlist'.
	*/

_PROTOTYPE(p_node node_copy, (p_node nd));
	/*	Creates a copy of the node tree indicated by 'nd'.
	*/

_PROTOTYPE(void kill_node, (p_node nd));
	/*	Removes the node tree indicated by nd.
	*/

_PROTOTYPE(void kill_nodelist, (p_node nd));
	/*	Removes all node trees in the list of nodes indicated by nd.
	*/

_PROTOTYPE(p_node select_base, (p_node nd));
	/*	Finds the base of a Select or Arrsel chain and returns it.
		(The base is the left hand side (of the left hand side ...)
		of the selection).
	*/

#ifdef DEBUG
_PROTOTYPE(int dump_node, (p_node nd, int lvl));
	/*	Produce a somewhat readable dump of node nd with indentation
		level lvl.
	*/

_PROTOTYPE(int dump_nodelist, (p_node l, int lvl));
	/*	Produce a somewhat readable dump of all nodes in nodelist l
		with indentation level lvl.
	*/

#endif

#define NULLNODE ((p_node) 0)

#define const_available(nd) \
		((nd->nd_flags & (ND_CONST|ND_GENERICPAR)) == ND_CONST)
#endif /* __NODE_H__ */
