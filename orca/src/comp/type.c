/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*	T Y P E	  D E F I N I T I O N	M E C H A N I S M	 */

/* $Id: type.c,v 1.35 1998/01/21 10:58:16 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<stdio.h>
#include	<assert.h>
#include	<alloc.h>

#include	"type.h"
#include	"node.h"
#include	"scope.h"
#include	"error.h"
#include	"misc.h"
#include	"options.h"
#include	"chk.h"
#include	"main.h"

p_type
	bool_type,
	char_type,
	int_type,
	longint_type,
	shortint_type,
	univ_int_type,
	string_type,
	real_type,
	longreal_type,
	shortreal_type,
	univ_real_type,
	std_type,
	nil_type,
	error_type;

int
	T_CONSTRUCTED = T_ISCONSTRUCTED;

_PROTOTYPE(static p_type standard_type, (int, char *, char *));
_PROTOTYPE(static p_type construct_type, (int, p_type));
_PROTOTYPE(static p_type remove_equal, (p_type));
_PROTOTYPE(static void enter_type, (char *, p_type));
_PROTOTYPE(static void enter_enum, (p_idf, p_type));

static p_type
construct_type(fund, tp)
	int	fund;
	p_type	tp;
{
	/*	fund must be a type constructor.
		The pointer to the constructed type is returned.
	*/
	p_type	dtp = new_type();

	switch (dtp->tp_fund = fund)	{
	case T_FUNCTION:
	case T_NODENAME:
	case T_ARRAY:
	case T_SET:
	case T_BAG:
	case T_OBJECT:
		break;
	default:
		crash("funny type constructor");
	}

	dtp->tp_next = tp;
	return dtp;
}

static p_type
standard_type(fund, str, dscr)
	int	fund;
	char	*str,
		*dscr;
{
	p_type	tp = new_type();

	tp->tp_fund = fund;
	tp->tp_tpdef = str;
	tp->tp_descr = dscr;
	tp->tp_flags = T_NEEDS_INIT;
	if (str) tp->tp_flags |= T_DECL_DONE;

	return tp;
}

static void
enter_type(name, type)
	char	*name;
	p_type	type;
{
	/*	Enter a type definition for "name"  and type
		"type" in the Current Scope.
	*/
	p_def	df = define(str2idf(name, 0), CurrentScope, D_TYPE);

	df->df_type = type;
	df->df_flags |= D_DEFINED;
}

static void
enter_enum(id, type)
	p_idf	id;
	p_type	type;
{
	/*	Put an enumeration literal in the symbol table.
		Also assign numbers to them, and link them together.
		We must link them together because an enumeration type may
		be exported, in which case its literals must also be exported.
		Thus, we need an easy way to get to them.
	*/
	p_def	df;

	df = define(id, CurrentScope, D_ENUM);
	if (df->df_kind != D_ERROR) {
		df->df_type = type;
		df->enm_val = (type->enm_ncst)++;
	}
	df->enm_next = type->enm_enums;
	type->enm_enums = df;
}

void
init_types()
{
	/*	Initialize the predefined types
	*/
	p_ardim	str_dims = (p_ardim) Malloc(sizeof(t_ardim));

	if (options['a']) {
		/* nodename becomes simple type: no age field. */
		T_CONSTRUCTED &= ~T_NODENAME;
	}

	/* character type
	*/
	char_type = standard_type(T_ENUM, "t_char", "td_char");
	char_type->tp_comparefunc = "cmp_enum";
	char_type->tp_assignfunc = "ass_enum";
	char_type->enm_ncst = 256;

	/* boolean type
	*/
	bool_type = standard_type(T_ENUM, "t_boolean", "td_boolean");
	bool_type->tp_comparefunc = "cmp_enum";
	bool_type->tp_assignfunc = "ass_enum";

	/* integer types
	*/
	int_type = standard_type(T_INTEGER, "t_integer", "td_integer");
	int_type->tp_comparefunc = "cmp_integer";
	int_type->tp_assignfunc = "ass_integer";
	longint_type = standard_type(T_INTEGER, "t_longint", "td_longint");
	longint_type->tp_comparefunc = "cmp_longint";
	longint_type->tp_assignfunc = "ass_longint";
	shortint_type = standard_type(T_INTEGER, "t_shortint", "td_shortint");
	shortint_type->tp_comparefunc = "cmp_shortint";
	shortint_type->tp_assignfunc = "ass_shortint";
	univ_int_type = standard_type(T_INTEGER, "t_integer", "td_integer");

	/* floating types
	*/
	real_type = standard_type(T_REAL, "t_real", "td_real");
	real_type->tp_comparefunc = "cmp_real";
	real_type->tp_assignfunc = "ass_real";
	longreal_type = standard_type(T_REAL, "t_longreal", "td_longreal");
	longreal_type->tp_comparefunc = "cmp_longreal";
	longreal_type->tp_assignfunc = "ass_longreal";
	shortreal_type = standard_type(T_REAL, "t_shortreal", "td_shortreal");
	shortreal_type->tp_comparefunc = "cmp_shortreal";
	shortreal_type->tp_assignfunc = "ass_shortreal";
	univ_real_type = standard_type(T_REAL, "t_real", "td_real");

	/* a unique type for standard procedures and functions
	*/
	std_type = construct_type(T_FUNCTION, NULLTYPE);

	/* string type
	 */
	str_dims->ar_index = int_type;
	str_dims->ar_bounds = 0;
	string_type = array_type(char_type, 1, str_dims);
	string_type->tp_tpdef = "t_string";
	string_type->tp_descr = "td_string";
	string_type->tp_init = "a_initialize";
	string_type->tp_szfunc = "sz_string";
	string_type->tp_mafunc = "ma_string";
	string_type->tp_umfunc = "um_string";
	string_type->tp_comparefunc = "cmp_string";
	string_type->tp_assignfunc = "ass_string";
	string_type->tp_freefunc = "free_string";
	string_type->tp_batinit = mk_str("batinit_", "string", (char *) 0);
	string_type->tp_flags |= T_DECL_DONE|T_INIT_DONE;

	/* NIL type
	*/
	nil_type = construct_type(T_NODENAME, NULLTYPE);
	nil_type->tp_tpdef = "t_nodename";
	nil_type->tp_descr = "td_nodename";
	nil_type->tp_comparefunc = "cmp_nodename";
	nil_type->tp_assignfunc = "ass_nodename";
	nil_type->tp_flags |= T_DECL_DONE;

	/* a unique type indicating an error
	*/
	error_type = new_type();
	*error_type = *char_type;
	error_type->tp_next = error_type;

	enter_type("char", char_type);
	enter_type("integer", int_type);
	enter_type("string", string_type);
	enter_type("real", real_type);
	enter_type("boolean", bool_type);
	enter_type("longreal", longreal_type);
	enter_type("shortreal", shortreal_type);
	enter_type("longint", longint_type);
	enter_type("shortint", shortint_type);

	enter_enum(str2idf("false", 0), bool_type);
	enter_enum(str2idf("true", 0), bool_type);
}

p_type
enum_type(EnumList)
	t_idlst	EnumList;
{
	p_type	tp = standard_type(T_ENUM, (char *) 0, (char *) 0);
	t_idlst	l;
	p_idf	id;

	idf_walklist(EnumList, l, id) {
		enter_enum(id, tp);
	}
	if (ufit(tp->enm_ncst-1, 1)) {
		tp->tp_comparefunc = "cmp_enum";
		tp->tp_assignfunc = "ass_enum";
	}
	else {
		tp->tp_comparefunc = "cmp_longenum";
		tp->tp_assignfunc = "ass_longenum";
	}
	idf_killlist(&EnumList);
	tp->tp_flags = T_NEEDS_INIT;
	return tp;
}

/* no_compare will be set by qualified_type when it replaces a generic formal
   type by an actual type. In this case, comparison on the non-scalar type
   being built using this generic formal type is not allowed.
*/

static int
	no_compare;

void
start_nonscalar()
{
	no_compare = 0;
}

void
end_nonscalar(tp)
	p_type	tp;
{
	if (no_compare) {
		tp->tp_flags |= T_NOEQ;
		no_compare = 0;
	}
}

p_type
qualified_type(nd)
	p_node	nd;
{
	/*	nd indicates a qualified identifier that must indicate a
		type. nd is removed and the type is returned.
	*/

	p_def	df;

	if (nd->nd_class != Def) {
		/*	The selection was not identified. */
		error("illegal qualified type");
		kill_node(nd);
		return error_type;
	}
	df = nd->nd_def;
	df->df_flags |= D_USED;
	kill_node(nd);
	if (df->df_kind & (D_ISTYPE|D_ERROR)) {
		if (! df->df_type) {
			error("type \"%s\" not (yet) declared",
				df->df_idf->id_text);
			return error_type;
		}
		if ((df->df_flags & D_GENERICPAR)
		    && generic_actual_of(df->df_type)) {
			if (df->df_type->tp_fund == T_GENERIC) {
				no_compare = 1;
			}
			return generic_actual_of(df->df_type);
		}
		return df->df_type;
	}
	error("identifier \"%s\" is not a type", df->df_idf->id_text);
	return error_type;
}

p_type
proc_type(result_type, parameters)
	p_type	result_type;
	t_dflst	parameters;
{
	p_type	tp = construct_type(T_FUNCTION, result_type);
	t_dflst	d = parameters;
	int	i = 0;

	/* Count number of parameters. */
	while (! def_emptylist(d)) {
		i++;
		d = def_nextlistel(d);
	}

	tp->prc_nparams = i;
	tp->prc_params = parameters;
	tp->tp_flags = T_NEEDS_INIT;
	tp->tp_comparefunc = "cmp_integer";
	tp->tp_assignfunc = "ass_integer";
	return tp;
}

p_type
funcaddr_type(result_type, parameters)
	p_type	result_type;
	t_dflst	parameters;
{
	p_type	tp = proc_type(result_type, parameters);

	tp->tp_flags |= T_FUNCADDR;
	return tp;
}

p_type
set_type(tp)
	p_type	tp;
{
	/*	Construct a set type with base type "tp"
	*/
	p_type	settp;

	/* Comparison must be allowed on set element type. */
	if (!tst_equality_allowed(tp)) error("illegal base type of set type");

	settp = construct_type(T_SET, tp);
	settp->tp_flags |= T_DYNAMIC|T_INIT_CODE;
	return settp;
}

p_type
bag_type(tp)
	p_type	tp;
{
	/*	Construct a bag type with base type "tp"
	*/
	p_type	bagtp;

	/* Comparison must be allowed on bag element type. */
	if (!tst_equality_allowed(tp)) error("illegal base type of bag type");

	bagtp = construct_type(T_BAG, tp);
	bagtp->tp_flags |= T_DYNAMIC|T_INIT_CODE;
	return bagtp;
}

p_type
array_type(etp, ndim, dims)
	p_type	etp;
	int	ndim;
	p_ardim	dims;
{
	p_type	tp;
	int	i;
	int	has_bounds = 0;
	int	const_bounds = 1;
	int	sz = 1;
	int	e_given = 0;

	tp = construct_type(T_ARRAY, etp);
	tp->arr_ind = dims;
	tp->arr_ndim = ndim;

	if (etp->tp_flags & T_NEEDS_INIT) {
		tp->tp_flags |= T_NEEDS_INIT;
	}

	for (i = 0; i < ndim; i++) {
		p_node	bounds = tp->arr_bounds(i);

		if (bounds) {
		    if (! (bounds->nd_flags & ND_CONST)) {
			const_bounds = 0;
		    }
		    else sz *= (bounds->nd_right->nd_int - bounds->nd_left->nd_int + 1);
		    if (i == 0) has_bounds = 1;
		}
		if (has_bounds != (bounds != 0) && ! e_given) {
			e_given = 1;
			error("type must have bounds for all dimensions (or none)");
		}
	}
	if (has_bounds) {
		tp->tp_flags |= T_HASBNDS;
		if (! options['x'] && const_bounds &&
		    etp->tp_fund != T_OBJECT) {
			/* Don't make arrays of objects into constant-sized
			   arrays, as long as the runtime systems don't
			   recognize this case.
			*/
			tp->tp_flags |= T_CONSTBNDS;
			tp->arr_size = sz;
		}
	}
	if (tp->tp_flags & T_CONSTBNDS) {
		tp->tp_flags |= etp->tp_flags &
		    (T_UNION_INIT|T_NOEQ|T_HASOBJ|T_DYNAMIC|T_INIT_CODE);
	}
	else {
		tp->tp_flags = T_DYNAMIC|T_INIT_CODE|
			(etp->tp_flags&(T_UNION_INIT|T_NOEQ|T_HASOBJ));
	}
	return tp;
}

p_type
get_index_type(tp, dim)
	p_type	tp;
	int	dim;
{
	switch(tp->tp_fund) {
	case T_ARRAY:
		if (tp->tp_flags & T_HASBNDS) {
			if (dim == 0) {
				error("bounds already specified in the array type declaration");
			}
		}
		break;
	case T_OBJECT:
		if (tp->tp_def->df_flags & D_PARTITIONED) {
			break;
		}
		/* fall through */
	default:
		if (tp != error_type) {
			if (dim == 0) {
				error("bounds only allowed for arrays%s",
					dp_flag ?
					" and partitioned objects" : "");
			}
			tp = error_type;
		}
	}
	if (tp == error_type) return tp;

	if (dim >= tp->arr_ndim) {
		if (dim == tp->arr_ndim) error("too many bounds specified");
		return error_type;
	}
	return tp->arr_index(dim);
}

p_type
nodename_type(tp)
	p_type	tp;
{
	tp = construct_type(T_NODENAME, tp);
	tp->tp_comparefunc = "cmp_nodename";
	tp->tp_assignfunc = "ass_nodename";
	tp->tp_flags |= T_INIT_CODE;
	return tp;
}

p_type
object_type(tp)
	p_type	tp;
{
	tp = construct_type(T_OBJECT, tp);
	tp->tp_flags |= T_DYNAMIC|T_NOEQ|T_INIT_CODE|T_HASOBJ;
	tp->tp_flags &= ~T_NEEDS_INIT;
	return tp;
}

p_type
graph_type(sc_root, sc_nodes)
	p_scope	sc_root,
		sc_nodes;
{
	p_type	tp = new_type();

	tp->tp_fund = T_GRAPH;
	tp->gra_root = record_type(sc_root);
	tp->tp_flags = tp->gra_root->tp_flags;
	tp->gra_node = record_type(sc_nodes);
	tp->gra_name = nodename_type(tp);
	tp->tp_flags |= tp->gra_root->tp_flags | T_DYNAMIC | T_NOEQ |
		T_INIT_CODE | (tp->gra_node->tp_flags & T_HASOBJ);
	return tp;
}

p_type
generic_type(kind)
	int	kind;
{
	p_type	tp = new_type();

	tp->tp_fund = kind;
	tp->tp_flags = T_NEEDS_INIT;
	if (kind == T_GENERIC) tp->tp_flags |= T_INIT_CODE|T_DYNAMIC|T_NOEQ;

	return tp;
}

p_type
record_type(sc)
	p_scope	sc;
{
	p_type	tp = new_type();
	p_def	df = sc->sc_def;

	tp->tp_fund = T_RECORD;
	tp->rec_scope = sc;
	while (df) {
		if (df->df_type
		    && (df->df_kind & (D_FIELD|D_OFIELD))) {
			tp->tp_flags |= df->df_type->tp_flags &
				(T_NEEDS_INIT|T_DYNAMIC|T_NOEQ|
				 T_INIT_CODE|T_HASOBJ|T_UNION_INIT);
		}
		if (df->fld_bat) tp->tp_flags |= T_INIT_CODE;
		df = df->df_nextinscope;
	}
	return tp;
}

p_type
union_type(sc, init)
	p_scope	sc;
	p_node	init;
{
	p_type	tp = new_type();
	p_def	df;

	tp->tp_fund = T_UNION;
	tp->rec_scope = sc;
	tp->rec_init = init;
	tp->tp_flags = T_INIT_CODE|T_UNION_INIT;
	if (! init) {
		tp->tp_flags |= T_NEEDS_INIT;
	}
	else {
		/* if init is constant, check corresponding field type;
		   else check all fields.
		*/
		df = get_union_variant(tp, init);
		if (df) {
			tp->tp_flags |= df->df_type->tp_flags & T_NEEDS_INIT;
		}
		else {
		    df = sc->sc_def;
		    df = df->df_nextinscope;	/* skip tag field */
		    while (df) {
			tp->tp_flags |= df->df_type->tp_flags & T_NEEDS_INIT;
			df = df->df_nextinscope;
		    }
		}
	}
	df = sc->sc_def;
	while (df) {
		tp->tp_flags |= df->df_type->tp_flags &
					(T_DYNAMIC|T_NOEQ|T_HASOBJ);
		df = df->df_nextinscope;
	}
	return tp;
}

void
remove_type(tp)
	p_type	tp;
{
	/*	Release type structures indicated by "tp".
		This procedure is only called for types, constructed with
		T_FUNCTION.
	*/
	t_dflst	pr,
		pr1;

	assert(tp->tp_fund == T_FUNCTION);

	pr = param_list_of(tp);
	while (! def_emptylist(pr)) {
		p_def	df = def_getlistel(pr);

		pr1 = pr;
		pr = def_nextlistel(pr);
		remove_from_id_list(df);
		free_def(df);
		free_list(pr1);
	}

	free_type(tp);
}

static p_type
remove_equal(tp)
	p_type	tp;
{
	while (tp && tp->tp_next == nil_type && tp->tp_equal) {
		tp = tp->tp_equal;
	}
	return tp;
}

p_type
named_type_of(tp)
	p_type	tp;
{
	while (tp && tp->tp_next == nil_type && tp->tp_equal) {
		tp = tp->tp_equal;
	}
	return tp->tp_next;
}

p_type
nodename_of_ident(id)
	p_idf	id;
{
	/*	NODENAME OF IDENTIFIER construction. The IDENTIFIER resides
		in "dot". This routine handles the different cases.
	*/
	struct forw
		*nd;
	p_def	df;
	p_type	tp = nodename_type(NULLTYPE);

	if ((df = lookup(id, CurrentScope, D_IMPORTED))) {
		/* Defined in this scope, so this must be the correct
		   identification.
		*/
		df->df_flags |= D_USED;
		if (df->df_kind == D_TYPE) {
			if (! df->df_type) {
				/* We have seen the name before, but the type
				   has not been declared yet. Add an entry
				   to the forwards-references list.
				*/
				nd = new_forw();
				nd->f_next = df->tdf_forw_list;
				df->tdf_forw_list = nd;
				nd->f_nodename = tp;
				nd->f_pos = dot.tk_pos;
			}
			else {
				if (df->df_type->tp_fund != T_GRAPH) {
					error("\"%s\" is not a GRAPH type",
						df->df_idf->id_text);
				}
				tp->tp_next = df->df_type;
			}
		}
		else {
			error("\"%s\" is not a type", df->df_idf->id_text);
			tp->tp_next = df->df_type;
		}
		return tp;
	}
	nd = new_forw();
	/*	Enter a forward reference into a list belonging to the
		current scope. This is used for NODENAME declarations, which
		may have forward references that must howewer be declared in the
		same scope.
	*/
	df = define(id, CurrentScope, D_TYPE);
	df->df_type = 0;
	df->df_flags |= D_USED | D_DEFINED;
	df->tdf_forw_list = nd;
	nd->f_nodename = tp;
	nd->f_pos = dot.tk_pos;
	return tp;
}

#ifdef DEBUG
int
dump_type(tp, nested)
	p_type	tp;
	int	nested;
{
	p_def	df;
	int	i;

	if (!tp) return 0;
	if (tp == error_type) {
		printf("ERROR TYPE");
		return 0;
	}

	if (nested && tp->tp_def) {
		printf("%s", tp->tp_def->df_idf->id_text);
		return 0;
	}

	switch(tp->tp_fund) {
	case T_RECORD:
		printf("RECORD ");
		df = tp->rec_scope->sc_def;
		while (df) {
			printf("%s: ", df->df_idf->id_text);
			dump_type(df->df_type, 1);
			printf("; ");
			df = df->df_nextinscope;
		}
		printf("END");
		break;
	case T_UNION:
		printf("UNION (");
		df = tp->rec_scope->sc_def;
		printf("%s: ", df->df_idf->id_text);
		dump_type(df->df_type, 1);
		printf(") ");
		df = df->df_nextinscope;
		while (df) {
			printf("%ld => %s: ",
				df->fld_tagvalue->nd_int,
				df->df_idf->id_text);
			dump_type(df->df_type, 1);
			printf("; ");
			df = df->df_nextinscope;
		}
		printf("END");
		break;
	case T_GENERIC:
		printf("GENERIC TYPE");
		break;
	case T_SCALAR:
		printf("SCALAR");
		break;
	case T_NUMERIC:
		printf("NUMERIC");
		break;
	case T_ENUM:
		if (tp == char_type) printf("char");
		else if (tp == bool_type) printf("boolean");
		else printf("ENUMERATION(%ld)", (long) tp->enm_ncst);
		break;
	case T_INTEGER:
		if (tp == longint_type) printf("longint");
		else if (tp == shortint_type) printf("shortint");
		else printf("integer");
		break;
	case T_REAL:
		if (tp == longreal_type) printf("longreal");
		else if (tp == shortreal_type) printf("shortreal");
		else printf("real");
		break;
	case T_SET:
		printf("SET OF ");
		return dump_type(element_type_of(tp), 1);
	case T_BAG:
		printf("BAG OF ");
		return dump_type(element_type_of(tp), 1);
	case T_FUNCTION:
		{
		t_dflst	par = param_list_of(tp);

		printf("FUNCTION");
		if (! def_emptylist(par)) {
			printf("(");
			while (! def_emptylist(par)) {
				p_def df = def_getlistel(par);
				if (is_in_param(df)) {
					printf("IN ");
				} else if (is_out_param(df)) {
					printf("OUT ");
				} else {
					printf("SHARED ");
				}
				(void) dump_type(df->df_type, 1);
				par = def_nextlistel(par);
				if (! def_emptylist(par)) printf(", ");
			}
			printf(")");
		}
		if (result_type_of(tp)) printf(":");
		break;
		}
	case T_ARRAY:
		printf("ARRAY[");
		for (i = 0; i < tp->arr_ndim; i++) {
			(void) dump_type(tp->arr_index(i), 1);
			if (i < tp->arr_ndim-1) putchar(',');
		}
		printf("] OF ");
		return dump_type(element_type_of(tp), 1);
	case T_NODENAME:
		printf("NODENAME OF ");
		break;
	case T_GRAPH:
		printf("GRAPH ");
		df = tp->gra_root->rec_scope->sc_def;
		while (df) {
			printf("%s: ", df->df_idf->id_text);
			dump_type(df->df_type, 1);
			printf("; ");
			df = df->df_nextinscope;
		}
		printf("NODES ");
		df = tp->gra_node->rec_scope->sc_def;
		while (df) {
			printf("%s: ", df->df_idf->id_text);
			dump_type(df->df_type, 1);
			printf("; ");
			df = df->df_nextinscope;
		}
		printf("END");
		break;
	case T_OBJECT:
		printf("OBJECT ");
		break;
	default:
		crash("dump_type");
	}
	if (tp->tp_next) {
		/* Avoid printing recursive types!
		*/
		(void) dump_type(tp->tp_next, 1);
	}
	return 0;
}
#endif

/* Some routines for testing type equivalence, compatibility, and other
   type-related things
*/

int
tst_equality_allowed(tp)
	p_type	tp;
{
	/*	 Check if items of type "tp" may be tested for (in)equality
	*/

	assert(tp != 0);

	if ((tp->tp_flags & T_NOEQ)
	    || is_opaque_type(tp)) return 0;

	switch(tp->tp_fund) {
	case T_ARRAY:
		return tst_equality_allowed(element_type_of(tp));
	case T_UNION:
	case T_RECORD:
		{	p_def	df;
			for (df = tp->rec_scope->sc_def;
			     df;
			     df = df->df_nextinscope) {
				if (! tst_equality_allowed(df->df_type)) {
					return 0;
				}
			}
			break;
		}
	}
	return 1;
}

int
tst_type_equiv(tp1, tp2)
	p_type	tp1, tp2;
{
	/*	test if two types are equivalent.
	*/

	if ((tp1->tp_fund & T_GENPAR) && generic_actual_of(tp1) != 0) {
		tp1 = generic_actual_of(tp1);
	}
	if ((tp2->tp_fund & T_GENPAR) && generic_actual_of(tp2) != 0) {
		tp2 = generic_actual_of(tp2);
	}
	return	tp1 == tp2
	    ||	(tp1 == univ_int_type && tp2->tp_fund == T_INTEGER)
	    ||	(tp2 == univ_int_type && tp1->tp_fund == T_INTEGER)
	    ||	(tp1 == univ_real_type && tp2->tp_fund == T_REAL)
	    ||	(tp2 == univ_real_type && tp1->tp_fund == T_REAL)
	    ||	remove_equal(tp1) == remove_equal(tp2)
	    ||	tp1 == error_type
	    ||	tp2 == error_type;
}

int
tst_proc_equiv(tp1, tp2)
	p_type tp1, tp2;
{
	/*	Test if two function/process types are equivalent. This routine
		may also be used for the testing of assignment compatibility
		between procedure variables and procedures.
	*/
	t_dflst p1, p2;

	/* First check if the result types are equivalent
	*/
	if (result_type_of(tp1) != 0) {
		if (result_type_of(tp2) == 0) return 0;
		if (! tst_type_equiv(result_type_of(tp1),
				     result_type_of(tp2))) {
			return 0;
		}
	}
	else if (result_type_of(tp2) != 0) return 0;

	/* Now check the parameters
	*/
	p1 = param_list_of(tp1);
	p2 = param_list_of(tp2);
	while (! def_emptylist(p1) && ! def_emptylist(p2)) {
		p_def	df1 = def_getlistel(p1);
		p_def	df2 = def_getlistel(p2);
		p_type	tp1 = df1->df_type;
		p_type	tp2 = df2->df_type;

		if (is_in_param(df1) != is_in_param(df2)
		    || is_out_param(df1) != is_out_param(df2)) {
			return 0;
		}
		if (df1->df_flags & D_GATHERED) tp1 = df1->var_gathertp;
		if (df2->df_flags & D_GATHERED) tp2 = df2->var_gathertp;

		if (!tst_type_equiv(tp1, tp2)) {
			return 0;
		}
		p1 = def_nextlistel(p1);
		p2 = def_nextlistel(p2);
	}

	/* Here, at least one of the parameterlists is exhausted.
	   Check that they are both.
	*/
	return def_emptylist(p1) && def_emptylist(p2);
}

int
tst_compat(tp1, tp2)
	p_type	tp1, tp2;
{
	/*	Test if two types are compatible.
	*/

	return	tst_type_equiv(tp1, tp2)
	    ||	(tp1->tp_fund == T_INTEGER && tp2->tp_fund == T_INTEGER)
	    ||	(tp1->tp_fund == T_REAL && tp2->tp_fund == T_REAL)
	    ||	(tp2 == nil_type && tp1->tp_fund == T_NODENAME)
	    ||	(tp1 == nil_type && tp2->tp_fund == T_NODENAME)
	;
}

int
tst_ass_compat(tp1, tp2)
	p_type	tp1, tp2;
{
	/*	Test if two types are assignment compatible.
	*/

	return	tst_compat(tp1, tp2)
	    ||	(tp1->tp_fund == T_NODENAME
		 && named_type_of(tp1)->gra_name == tp2)
	    ||	(tp1->tp_fund == T_FUNCTION
		 && tp2->tp_fund == T_FUNCTION
		 && tst_proc_equiv(tp1, tp2));
}

char *
incompat(tp1, tp2)
	p_type	tp1, tp2;
{

	if (is_opaque_type(tp1)
	    || is_opaque_type(tp2)) {
		return "properties of opaque type are hidden; illegal use";
	}
	return "type incompatibility";
}

void
chk_par_compat(parno, is_value_param, formaltype, actualtype, edf)
	int	parno;
	int	is_value_param;
	p_type	formaltype,
		actualtype;
	p_def	edf;
{
	/*	Check type compatibility for a parameter in a procedure call.
	*/
	if (is_value_param) {
		if (tst_ass_compat(formaltype, actualtype)) return;
	}
	else {
		if (tst_type_equiv(formaltype, actualtype)) return;
		if (formaltype->tp_fund == T_FUNCTION
		    && actualtype->tp_fund == T_FUNCTION
		    && tst_proc_equiv(formaltype, actualtype)) return;
	}
	if (edf) {
		error("\"%s\", parameter %d: %s",
			edf->df_idf->id_text,
			parno,
			incompat(formaltype, actualtype));
	}
	else error("parameter %d: %s", parno, incompat(formaltype, actualtype));
}

int
pure_write(df)
	p_def	df;
{
	/*	Check that the operation indicated by df is a pure write. */

	p_type	tp = df->df_type;
	t_dflst	l;

	if (result_type_of(tp)) return 0;
	def_walklist(tp->prc_params, l, df) {
		if (! is_in_param(df)) return 0;
	}
	return 1;
}

p_type
must_be_discrete_type(nd, s)
	p_node	nd;
	char	*s;
{
	p_type	tp = nd->nd_type;

	if (tp != error_type && ! (tp->tp_fund & T_DISCRETE)) {
		pos_error(&nd->nd_pos, "non-discrete type in %s", s);
		tp = error_type;
	}
	return tp;
}

p_type
must_be_gather_type(tp)
	p_type	tp;
{
	/*	The result type in a GATHER must have the same shape as the
		partitioned object, so it must be an array with the
		same number of dimensions, with compatible index types.
	*/
	int	i;

	if (tp == error_type) return tp;
	if (tp->tp_fund != T_ARRAY) {
		error("GATHER should give an array type");
		return error_type;
	}
	if (CurrDef->df_type) {
	    if (tp->arr_ndim != CurrDef->df_type->arr_ndim) {
		error("GATHER result type has wrong number of dimensions");
		return error_type;
	    }
	    for (i = 0; i < tp->arr_ndim; i++) {
		if (! tst_ass_compat(tp->arr_index(i),
				     CurrDef->df_type->arr_index(i))) {
			error("type incompatibility in bounds of GATHER type");
		}
	    }
	}
	return tp;
}

p_type
must_be_aggregate_type(nd)
	p_node	nd;
{
	p_type	tp = error_type;

	if (nd->nd_class != Def || nd->nd_def->df_kind != D_TYPE) {
		pos_error(&nd->nd_pos, "illegal type specifier in aggregate");
	}
	else {
		tp = nd->nd_type;
		nd->nd_def->df_flags |= D_USED;
	}
	kill_node(nd);
	return tp;
}

p_def
get_reduction_func(funcid, valtp)
	p_idf	funcid;
	p_type	valtp;
{
	/*	Check that the function with name funcid may be used as a
		reduction function for values of type valtp.
		Criteria: the function must have two SHARED-parameters of a type
		equivalent with valtp.
		Return the def structure associated with the function.
	*/

	p_def	funcdf = lookfor(funcid, CurrentScope, 1);
	p_type	functp = funcdf->df_type;
	t_dflst	l;

	if (functp == error_type) return funcdf;
	if (funcdf->df_kind != D_FUNCTION) {
		error("'%s' is not a function", funcdf->df_idf->id_text);
		return funcdf;
	}
	funcdf->df_flags |= D_USED;
	l = param_list_of(functp);
	if (functp->prc_nparams != 2 ||
	    ! is_shared_param(def_getlistel(l)) ||
	    ! is_shared_param(def_getlistel(def_nextlistel(l)))) {
		error("'%s': a reduction function must have 2 SHARED parameters",
		      funcdf->df_idf->id_text);
		return funcdf;
	}
	if (! tst_ass_compat(valtp, def_getlistel(l)->df_type) ||
	    ! tst_ass_compat(valtp, def_getlistel(def_nextlistel(l))->df_type)){
		error("'%s': type incompatibility in reduction function",
		      funcdf->df_idf->id_text);
	}

	def_enlist(&CurrDef->mod_reductionfuncs, funcdf);
	return funcdf;
}

p_def
get_union_variant(tp, nd)
	p_type	tp;
	p_node	nd;
{
	/* Check that the expression indicated by "nd" is a legal initial
	   value for the tag of the union indicated by "tp". If the
	   expression is not constant, we can only check the type. If it
	   is, we can also check that it corresponds to one of the
	   alternatives. If it does, return it, otherwise return 0.
	*/
	p_def	df;

	mark_defs(nd, D_USED);
	if (tp->tp_fund != T_UNION) {
		if (tp != error_type) {
			error("initial tag value only allowed for union types");
		}
		return 0;
	}

	df = tp->rec_scope->sc_def;

	assert(df->df_flags & D_TAG);
	if (! tst_ass_compat(df->df_type, nd->nd_type)) {
		pos_error(&nd->nd_pos, "type incompatibility in initial union tag value");
	}
	else if (nd->nd_class == Value) {
		while ((df = df->df_nextinscope)) {
			if (df->fld_tagvalue->nd_int == nd->nd_int) {
				return df;
			}
		}
		pos_error(&nd->nd_pos, "initial tag value does not match union alternative");
	}
	return (p_def) 0;
}
