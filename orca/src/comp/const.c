/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* C O N S T A N T   E X P R E S S I O N   H A N D L I N G */

/* $Id: const.c,v 1.22 1998/03/06 15:46:12 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<assert.h>
#include	<alloc.h>
#include	<stdio.h>
#include	<flt_arith.h>
#include	<em_arith.h>

#include	"const.h"
#include	"def.h"
#include	"type.h"
#include	"oc_stds.h"
#include	"error.h"
#include	"main.h"

long full_mask[sizeof(long)+1];/* full_mask[1] == 0xFF, full_mask[2] == 0xFFFF, .. */
long max_int[sizeof(long)+1];	/* max_int[1] == 0x7F, max_int[2] == 0x7FFF, .. */
long min_int[sizeof(long)+1];	/* min_int[1] == 0xFFFFFF80, min_int[2] = 0xFFFF8000,
			   ...
			*/

_PROTOTYPE( static void overflow, (t_pos *pos) );
_PROTOTYPE( static void cut_size, (p_node expr) );

static int
	word_size = sizeof(long);

int
get_wordsize()
{
	return word_size;
}

void
set_wordsize(sz)
	int	sz;
{
	if (sz > sizeof(long)) {
		sz = sizeof(long);
		warning("maximum word size is %d", sizeof(long));
	}
	word_size = sz;
}

static void
overflow(pos)
	t_pos	*pos;
{
	pos_warning(pos, "overflow in constant expression");
}

p_node
cst_unary(nd)
	p_node	nd;
{
	/*	The unary operation in "nd" is performed on the constant
		expression below it, and the result returned.
	*/
	p_node	nd_res;

	assert(nd->nd_class == Uoper);

	if (nd->nd_symb == '+' || nd->nd_symb == '(') {
		nd_res = nd->nd_right;
		nd_res->nd_flags |= ND_NODESIG;
		nd->nd_right = 0;
		kill_node(nd);
		return nd_res;
	}

	assert(nd->nd_right->nd_class == Value);

	nd_res = mk_leaf(Value, INTEGER);
	nd_res->nd_int = nd->nd_right->nd_int;

	switch(nd->nd_symb) {
	case '-':
		if (nd_res->nd_int == min_int[word_size])
			overflow(&nd->nd_pos);

		nd_res->nd_int = -nd_res->nd_int;
		break;

	case NOT:
		nd_res->nd_int = !nd_res->nd_int;
		break;

	case '~':
		nd_res->nd_int = ~nd_res->nd_int;
		break;

	default:
		crash("cst_unary");
	}

	nd_res->nd_type = nd->nd_right->nd_type;
	nd_res->nd_pos = nd->nd_pos;
	kill_node(nd);
	cut_size(nd_res);
	return nd_res;
}

p_node
cst_relational(nd)
	p_node	nd;
{
	long	o1 = nd->nd_left->nd_int,
		o2 = nd->nd_right->nd_int;
	p_node	nd_res = mk_leaf(Value, INTEGER);

	assert(nd->nd_class == Oper);
	assert(nd->nd_left->nd_class == Value);
	assert(nd->nd_right->nd_class == Value);

	switch(nd->nd_symb) {
	case '<':
		o1 = (o1 < o2);
		break;

	case '>':
		o1 = (o1 > o2);
		break;

	case LESSEQUAL:
		o1 = (o1 <= o2);
		break;

	case GREATEREQUAL:
		o1 = (o1 >= o2);
		break;

	case '=':
		o1 = (o1 == o2);
		break;

	case NOTEQUAL:
		o1 = (o1 != o2);
		break;

	default:
		crash("cst_relational");
	}

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = nd->nd_type;
	nd_res->nd_int = o1;
	kill_node(nd);

	return nd_res;
}

p_node
cst_arithop(nd)
	p_node	nd;
{
	/*	The binary operation in "nd" is performed on the constant
		expressions below it, and the result returned.
		This version is for INTEGER expressions.
	*/
	long	o1 = nd->nd_left->nd_int;
	long	o2 = nd->nd_right->nd_int;
	long	tmp;
	p_node	nd_res = mk_leaf(Value, INTEGER);

	assert(nd->nd_class == Oper);
	assert(nd->nd_left->nd_class == Value);
	assert(nd->nd_right->nd_class == Value);

	switch (nd->nd_symb)	{
	case '*':
		if (o1 > 0 && o2 > 0) {
			if (max_int[word_size] / o1 < o2) overflow(&nd->nd_pos);
		}
		else if (o1 < 0 && o2 < 0) {
			if (o1 == min_int[word_size]
			    || o2 == min_int[word_size]
			    || max_int[word_size] / (-o1) < (-o2)) {
				overflow(&nd->nd_pos);
			}
		}
		else if (o1 > 0) {
			if (min_int[word_size] / o1 > o2) overflow(&nd->nd_pos);
		}
		else if (o2 > 0) {
			if (min_int[word_size] / o2 > o1) overflow(&nd->nd_pos);
		}
		o1 *= o2;
		break;

	case '/':
		if (o2 == 0)	{
			pos_error(&nd->nd_pos, "division by 0");
			break;
		}
		if (o1 == 0) break;
		tmp = o1 % o2;
		if (tmp && (o1 > 0) != (tmp > 0)) {
			o1 = o1/o2 + 1;
		}
		else o1 /= o2;
		break;

	case '%':
		if (o2 <= 0)	{
			pos_error(&nd->nd_pos, "modulo by <= 0");
			break;
		}
		if (o1 == 0) break;
		tmp = o1 % o2;
		if (tmp && (o1 > 0) != (tmp > 0)) {
			o1 = tmp - o2;
		}
		else	o1 = tmp;
		break;

	case '+':
		if (o1 > 0 && o2 > 0) {
			if (max_int[word_size] - o1 < o2) overflow(&nd->nd_pos);
		}
		else if (o1 < 0 && o2 < 0) {
			if (min_int[word_size] - o1 > o2) overflow(&nd->nd_pos);
		}
		o1 += o2;
		break;

	case '-':
		if (o1 >= 0 && o2 < 0) {
			if (max_int[word_size] + o2 < o1) overflow(&nd->nd_pos);
		}
		else if (o1 < 0 && o2 >= 0) {
			if (min_int[word_size] + o2 > o1) overflow(&nd->nd_pos);
		}
		o1 -= o2;
		break;

	case '|':
		o1 |= o2;
		break;

	case '&':
		o1 &= o2;
		break;

	case '^':
		o1 ^= o2;
		break;

	case LEFTSHIFT:
		o1 = o1 << o2;
		break;

	case RIGHTSHIFT:
		o1 = o1 >> o2;
		break;

	case AND:
	case OR:
		nd_res->nd_int = (nd->nd_symb == OR) ? (o1 || o2) : (o1 && o2);
		nd_res->nd_pos = nd->nd_pos;
		nd_res->nd_type = bool_type;
		kill_node(nd);
		return nd_res;

	default:
		crash("cst_arithop");
	}

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = univ_int_type;
	nd_res->nd_int = o1;
	cut_size(nd_res);
	kill_node(nd);

	return nd_res;
}

p_node
cst_boolop(nd)
	p_node	nd;
{
	/* Boolean operator AND or OR or NOT with one or more constant
	   operator.
	*/
	p_node	nd_res = 0;

	assert(nd->nd_class == Oper);
	switch (nd->nd_symb)	{
	case NOT:
		assert(nd->nd_right->nd_class == Value);
		nd_res = mk_leaf(Value, INTEGER);
		nd_res->nd_int = ! nd->nd_right->nd_int;
		break;

	case OR:
		if (nd->nd_left->nd_class == Value) {
			if (nd->nd_left->nd_int != 0) {
				nd_res = nd->nd_left;
				nd->nd_left = 0;
				break;
			}
			nd_res = nd->nd_right;
			nd->nd_right = 0;
			break;
		}

		assert(nd->nd_right->nd_class == Value);
		if (nd->nd_right->nd_int == 0) {
			nd_res = nd->nd_left;
			nd->nd_left = 0;
			break;
		}
		return nd;

	case AND:
		if (nd->nd_left->nd_class == Value) {
			if (nd->nd_left->nd_int == 0) {
				nd_res = nd->nd_left;
				nd->nd_left = 0;
				break;
			}
			nd_res = nd->nd_right;
			nd->nd_right = 0;
			break;
		}

		assert(nd->nd_right->nd_class == Value);
		if (nd->nd_right->nd_int != 0) {
			nd_res = nd->nd_left;
			nd->nd_left = 0;
			break;
		}
		return nd;

	default:
		crash("cst_boolop");
	}

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = bool_type;
	kill_node(nd);
	return nd_res;
}

p_node
cst_call(nd, call)
	p_node	nd;
	int	call;
{
	/*	a standard procedure call is found that can be evaluated
		compile time, so do so.
	*/
	p_node	nd_res = mk_leaf(Value, INTEGER);
	p_node	arg = node_getlistel(nd->nd_parlist);

	assert(nd->nd_class == Call);

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = nd->nd_type;

	nd_res->nd_int = arg->nd_int;
	switch(call) {
	case S_ABS:
		if (nd_res->nd_int < 0) {
			if (nd_res->nd_int <= min_int[word_size]) {
				overflow(&nd_res->nd_pos);
			}
			nd_res->nd_int = - nd_res->nd_int;
		}
		nd_res->nd_type = univ_int_type;
		break;

	case S_ORD:
		break;

	case S_VAL:
		nd_res->nd_int = node_getlistel(node_nextlistel(nd->nd_parlist))->nd_int;
		/* fall through */
	case S_CHR:
		if (nd_res->nd_int >= nd->nd_type->enm_ncst) {
			pos_warning(&nd->nd_pos, "range bound error in %s",
				call == S_VAL ? "VAL" : "CHR");
			free_node(nd_res);
			return nd;
		}
		break;

	case S_CAP:
		if (nd_res->nd_int >= 'a' && nd_res->nd_int <= 'z') {
			nd_res->nd_int += ('A' - 'a');
		}
		break;

	case S_MAX:
		if (nd_res->nd_type->tp_fund == T_INTEGER) {
			if (nd_res->nd_type == int_type) {
				nd_res->nd_int = max_int[sizeof(int)];
			}
			else {
				nd_res->nd_int = max_int[sizeof(long)];
			}
		}
		else	nd_res->nd_int = nd_res->nd_type->enm_ncst - 1;
		break;

	case S_MIN:
		if (nd_res->nd_type->tp_fund == T_INTEGER) {
			if (nd_res->nd_type == int_type) {
				nd_res->nd_int = min_int[sizeof(int)];
			}
			else {
				nd_res->nd_int = min_int[sizeof(long)];
			}
		}
		else	nd_res->nd_int = 0;
		break;

	case S_ODD:
		nd_res->nd_int &= 1;
		break;

	case S_LB:
	case S_UB: {
		p_node	arg2 =
			node_getlistel(node_nextlistel(nd->nd_parlist));
		int i = 0;
		t_type *tp = arg->nd_type;

		if (arg->nd_class == Value) {
			assert(arg->nd_type == string_type);
			if (call == S_LB) nd_res->nd_int = 1;
			else nd_res->nd_int = arg->nd_str->s_length;
			break;
		}
		if (tp->tp_flags & T_CONSTBNDS) {
			int	j = arg2 ? arg2->nd_int-1 : 0;
			if (call == S_LB) {
			    nd_res->nd_int = tp->arr_bounds(j)->nd_left->nd_int;
			}
			else {
			    nd_res->nd_int = tp->arr_bounds(j)->nd_right->nd_int;
			}
			break;
		}
		assert(arg->nd_class == Def && arg->nd_def->df_kind == D_CONST);
		arg = arg->nd_def->con_const;
		if (arg2) {
			int j = arg2->nd_int-1;
			i = j;
			while (j > 0) {
				arg = node_getlistel(arg->nd_parlist);
				j--;
			}
		}
		if (call == S_UB) nd_res->nd_int = arg->nd_nelements;
		else nd_res->nd_int = 1;
		if (tp->arr_index(i)->tp_fund == T_ENUM) {
			nd_res->nd_int--;
		}
		}
		break;

	case S_SIZE:
		if (arg->nd_class == Value) {
			assert(arg->nd_type == string_type);
			nd_res->nd_int = arg->nd_str->s_length;
			break;
		}
		nd_res->nd_int = 1;
		if (arg->nd_type->tp_flags & T_CONSTBNDS) {
		    nd_res->nd_int = arg->nd_type->arr_size;
		    break;
		}
		if (arg->nd_class == Def) arg = arg->nd_def->con_const;
		for (;;) {
			nd_res->nd_int *= arg->nd_nelements;
			if (arg->nd_symb == '[') break;
			arg = node_getlistel(arg->nd_parlist);
		}
		break;

	default:
		crash("cst_call");
	}

	kill_node(nd);
	cut_size(nd_res);
	return nd_res;
}

static void
cut_size(expr)
	p_node	expr;
{
	/*	The constant value of the expression expr is made to
		conform to the size of the type of the expression.
	*/
	int	nbits = (int) (sizeof(long) - word_size) * 8;

	expr->nd_int = (expr->nd_int << nbits) >> nbits;
}

void
init_cst()
{
	int	i = 0;
	long	bt = (long)0;

	while (!(bt < 0))	{
		i++;
		bt = (bt << 8) + 0377;
		full_mask[i] = bt;
		max_int[i] = bt & ~(1L << ((i << 3) - 1));
		min_int[i] = - max_int[i];
		min_int[i]--;
	}
}

p_node
fcst_relational(nd)
	p_node	nd;
{
	int	cmpval = flt_cmp(&nd->nd_left->nd_real->r_val, &nd->nd_right->nd_real->r_val);
	p_node	nd_res = mk_leaf(Value, INTEGER);

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = nd->nd_type;

	switch(nd->nd_symb) {
	case '<':		cmpval = (cmpval < 0); break;
	case '>':		cmpval = (cmpval > 0); break;
	case LESSEQUAL:		cmpval = (cmpval <= 0); break;
	case GREATEREQUAL:	cmpval = (cmpval >= 0); break;
	case '=':		cmpval = (cmpval == 0); break;
	case NOTEQUAL:		cmpval = (cmpval != 0); break;
	default:
		crash("fcst_relational");
	}

	kill_node(nd);

	nd_res->nd_int = cmpval;
	return nd_res;
}

p_node
fcst_arithop(nd)
	p_node	nd;
{
	flt_arith
		*o1 = &nd->nd_left->nd_real->r_val,
		*o2 = &nd->nd_right->nd_real->r_val;
	struct real
		*result = (p_real) Malloc(sizeof(t_real));
	p_node	nd_res = mk_leaf(Value, REAL);

	result->r_real = 0;

	assert(nd->nd_class == Oper);
	assert(nd->nd_left->nd_class == Value);
	assert(nd->nd_right->nd_class == Value);

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = univ_real_type;
	nd_res->nd_real = result;

	switch (nd->nd_symb)	{
	case '*':
		flt_mul(o1, o2, &result->r_val);
		break;

	case '/':
		flt_div(o1, o2, &result->r_val);
		break;

	case '+':
		flt_add(o1, o2, &result->r_val);
		break;

	case '-':
		flt_sub(o1, o2, &result->r_val);
		break;

	default:
		crash("fcst_arithop");
	}

	switch(flt_status) {
	case FLT_OVFL:
		pos_warning(&nd->nd_pos, "floating point overflow on %s",
				symbol2str(nd->nd_symb));
		break;
	case FLT_DIV0:
		pos_error(&nd->nd_pos, "division by 0.0");
		break;
	}

	kill_node(nd);
	return nd_res;
}

p_node
fcst_unary(nd)
	p_node	nd;
{
	struct real
		*result = (p_real) Malloc(sizeof(t_real));
	p_node	nd_res = mk_leaf(Value, REAL);

	result->r_real = 0;

	assert(nd->nd_class == Uoper);
	assert(nd->nd_right->nd_class == Value);
	assert(nd->nd_symb == '-');

	nd_res->nd_pos = nd->nd_pos;
	nd_res->nd_type = nd->nd_right->nd_type;
	nd_res->nd_real = result;

	result->r_val = nd->nd_right->nd_real->r_val;
	flt_umin(&result->r_val);
	kill_node(nd);
	return nd_res;
}

p_node
fcst_call(nd, call)
	p_node	nd;
	int	call;
{
	p_node	nd_res = mk_leaf(Value, REAL);
	p_node	arg = node_getlistel(nd->nd_parlist);

	switch(call) {
	case S_ABS:
		nd_res->nd_real = (p_real) Malloc(sizeof(t_real));
		nd_res->nd_real->r_real = 0;
		nd_res->nd_real->r_val = arg->nd_real->r_val;
		if (nd_res->nd_real->r_val.flt_sign != 0) {
			flt_umin(&nd_res->nd_real->r_val);
		}
		else {
			nd_res->nd_real->r_real = arg->nd_real->r_real;
			arg->nd_real->r_real = 0;
			/* to prevent de-allocation */
		}
		nd_res->nd_type = univ_real_type;
		break;

	case S_TRUNC:
		nd_res->nd_symb = INTEGER;
		nd_res->nd_int = flt_flt2arith(&arg->nd_real->r_val, 0);
		if (flt_status == FLT_OVFL
		    || nd_res->nd_int < min_int[word_size]
		    || nd_res->nd_int > max_int[word_size]) {
			pos_warning(&nd->nd_pos, "overflow in TRUNC");
		}
		nd_res->nd_type = univ_int_type;
		cut_size(nd_res);
		break;

	case S_FLOAT:
		nd_res->nd_real = (p_real) Malloc(sizeof(t_real));
		nd_res->nd_real->r_real = 0;
		flt_arith2flt((arith) arg->nd_int, &nd_res->nd_real->r_val, 0);
		nd_res->nd_type = univ_real_type;
		break;

	default:
		crash("fcst_call");
	}

	nd_res->nd_pos = nd->nd_pos;
	kill_node(nd);
	return nd_res;
}
