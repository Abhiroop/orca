/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __TYPE_H__
#define __TYPE_H__

/* T Y P E   D E S C R I P T O R   S T R U C T U R E */

/* $Id: type.H,v 1.27 1998/01/21 10:58:16 ceriel Exp $ */

#include	"ansi.h"
#include	"idf.h"
#include	"def.h"
#include	"const.h"

typedef struct type	t_type, *p_type;

/* The fields of a type structure are:
   tp_next	(p_type)
			indicates a member type, for instance for arrays, set,
			bag.  For functions, it indicates the result type.
			For object types, it indicates the type of the record
			representing the object fields. For nodenames, it
			indicates the graph type to which the nodename type
			refers. For generic parameter types, it refers to the
			actual type in case of an instantiation.
   tp_fund	(int)
			indicates the fundamental type or constructor of this
			type.  See below for a list of its possible values.
   tp_flags	(int)	the possible flags are listed below.
   tp_tpdef	(char *)
			the C-identifier indicating the typedef produced for
			this type.
   tp_descr	(char *)
			the C-identifier indicating the type descriptor
			produced for this type.
   tp_init	(char *)
			the C-identifier indicating the initialization macro
			produced for this type.
   tp_def	(p_def)	the def structure indicating the definition of this type
			(if it exists, the type could be anonymous as well, in
			which case it has a count:)
   tp_batinit	(char *)
			the C-identifier indicating the initialization macro
			name to be used when there is a bounds-and-tag
			expression.
   tp_anon	(int)	the counter for anonymous types.
   tp_szfunc	(char *)
			for marshalling: the C-identifier indicating the
			function computing the size of the buffer needed to
			marshall something of this type.
   tp_mafunc	(char *)
			for marshalling: the C-identifier indicating the
			function doing the marshalling.
   tp_umfunc	(char *)
			for marshalling: the C-identifier indicating the
			function doing the unmarshalling.
   tp_comparefunc
		(char *)
			the C-identifier indicating the function implementing
			comparison for this type.
   tp_assignfunc
		(char *)
			the C-identifier indicating the function implememting
			assignment for this type.
   tp_freefunc	(char *)
			the C-identifier indicating the function implenting
			cleaning up for this type.
   tp_equal	(p_type)
			For generic formal type parameters: the actual
			parameter. May also be used for types which are declared
			opaque and later are declared with
				TYPE a = b;
			where b is a nodename type.
   tp_value		a union containing several fields, depending on tp_fund.
			These fields are accessed through macros as if they
			belong to the type structure itself.
*/

/* Possible values for tp_fund: */

/* Free: 0x0001 */
#define T_ENUM		0x0002
#define	T_INTEGER	0x0004
#define T_REAL		0x0008
#define T_RECORD	0x0010
#define	T_UNION		0x0020
#define T_FUNCTION	0x0040
#define T_NODENAME	0x0080
#define T_OBJECT	0x0100
#define T_SET		0x0200
#define T_BAG		0x0400
#define T_GRAPH		0x0800
#define T_ARRAY		0x1000
#define T_GENERIC	0x2000
#define T_NUMERIC	0x4000
#define T_SCALAR	0x8000

/* Possible values for tp_flags */

#define T_DYNAMIC	0x0001	/* Set if type has dynamic component */
#define T_NOEQ		0x0002	/* Set if comparison is not allowed */
#define T_INIT_CODE	0x0004	/* Set if compiler must initialize */
#define T_HASOBJ	0x0008	/* Set if type contains object type */
#define T_PART_OBJ	0x0010	/* Set if type is partitioned object */
#define T_NEEDS_INIT	0x0020	/* Set if program must initialize */
#define T_FUNCADDR	0x0040	/* Set for type of function variables */
#define T_DECL_DONE	0x0100	/* Set if typedef declaration has been done */
#define T_UNION_INIT	0x0200	/* Set if aggregate of this type would need
				   union initialization.
				*/
#define T_INIT_DONE	0x0400	/* Set if initialization macro is produced */
#define T_HASBNDS	0x0800	/* Set if the type declaration contains
				   bounds.
				*/
#define T_CONSTBNDS	0x1000	/* Set if the type declaration has constant
				   bounds.
				*/

/* For tp_fund = T_ENUM, the following fields exist:
   enm_ncst	(int)	the number of enumeration literals.
   enm_enums	(p_def)	a list of def structures representing the enumeration
			literals, linked through the enm_next field of the
			def structure.
*/

typedef struct {
    p_def	en_enums;
    int		en_ncst;
#define enm_enums	tp_value.tp_enum.en_enums
#define enm_ncst	tp_value.tp_enum.en_ncst
} t_enum;

/* For tp_fund = T_ARRAY, the following fields exist:
   arr_size	(int)	the number of elements in the array in case it has a
			constant size.
   arr_ndim	(int)	the number of dimensions of the array type.
   arr_ind	(t_ardim *)
			pointer to an array of t_ardim structures, one element
			for each dimension. The arr_ind fields are accessible
			through macros, as if they are members of the type
			structure, through the following two macros:
   arr_index(n)	(p_type)
			the index type of the n-th dimension of the array type.
   arr_bounds(n)
		(p_node)
			the bounds expressions for the index of the n-th
			dimension of the array type.
*/

typedef struct {
    p_type	ar_index;
    p_node	ar_bounds;
} t_ardim, *p_ardim;

#define fill_ardim(a, tp, nd) \
	do { \
		(a)->ar_index = tp; \
		(a)->ar_bounds = nd; \
	} while (0)

typedef struct {
    int		ar_ndim;
    p_ardim	ar_ind;
    int		ar_size;
#define arr_size	tp_value.tp_arr.ar_size
#define arr_ndim	tp_value.tp_arr.ar_ndim
#define arr_ind		tp_value.tp_arr.ar_ind
#define arr_index(n)	tp_value.tp_arr.ar_ind[n].ar_index
#define arr_bounds(n)	tp_value.tp_arr.ar_ind[n].ar_bounds
} t_array;

/* For tp_fund = T_RECORD or T_UNION the following fields exist:
   rec_scope	(p_scope)
			the scope in which the record/union fields reside.
   rec_init	(p_node)
			the default tag value of the union.
*/

typedef struct {
    struct scope
		*rc_scope;
    p_node	rc_init;
#define rec_scope	tp_value.tp_record.rc_scope
#define rec_init	tp_value.tp_record.rc_init
} t_recrd;

/* For tp_fund = T_FUNCTION, the following fields exist:
   prc_params	(t_dflst)
			a list of def structures, one for each parameter.
   prc_nparams	(int)	the number of parameters.
   prc_objtype	(p_type)
			for an operation, the type of the object on which the
			operation applies.
   prc_ftp	(char *)
			the C-identifier indicating the function typedef,
			for function variables.
*/

typedef struct {
    t_dflst	pr_params;
    union {
	p_type	pr_objtype;
	char	*pr_ftp;
    }	pr_xxx;
    int		pr_nparams;
#define prc_params	tp_value.tp_proc.pr_params
#define prc_nparams	tp_value.tp_proc.pr_nparams
#define prc_objtype	tp_value.tp_proc.pr_xxx.pr_objtype
#define prc_ftp		tp_value.tp_proc.pr_xxx.pr_ftp
} t_proc;

/* For tp_fund = T_GRAPH, the following fields exist:
   gra_root	(p_type)
			the record type of the graph root.
   gra_node	(p_type)
			the record type of the graph nodes.
   gra_name	(p_type)
			an anonymous nodename type. This is the type returned
			by ADDNODE, and is compatible with all nodename types
			referencing this graph type.
*/

typedef struct {
    p_type	gr_root,
		gr_node,
		gr_name;
#define gra_root	tp_value.tp_graph.gr_root
#define gra_node	tp_value.tp_graph.gr_node
#define gra_name	tp_value.tp_graph.gr_name
} t_graph;

struct type {
    p_type	tp_next;
    int		tp_fund;
    int		tp_flags;
    char	*tp_tpdef;
    char	*tp_descr;
    char	*tp_szfunc;
    char	*tp_mafunc;
    char	*tp_umfunc;
    char	*tp_comparefunc;
    char	*tp_assignfunc;
    char	*tp_freefunc;
    char	*tp_init;
    p_def	tp_def;
    int		tp_anon;
    p_type	tp_equal;
    char	*tp_batinit;
    union {
	t_enum	tp_enum;
	t_array	tp_arr;
	t_recrd	tp_record;
	t_proc	tp_proc;
	t_graph	tp_graph;
    }	tp_value;
};

#define T_DISCRETE	(T_ENUM|T_INTEGER)
#define	T_ISCONSTRUCTED	(T_ARRAY|T_SET|T_RECORD|T_BAG|T_UNION| \
			 T_NODENAME|T_GRAPH|T_GENERIC|T_OBJECT)
#define T_NUMERICARG	(T_NUMERIC|T_INTEGER|T_REAL)
#define T_SCALARARG	(T_NUMERICARG|T_ENUM|T_SCALAR)
#define T_GENPAR	(T_GENERIC|T_NUMERIC|T_SCALAR)
#define T_ISSIMPLEARG	(T_SCALARARG|T_FUNCTION)

#define T_RTS_FLAGS	(T_DYNAMIC|T_NOEQ|T_INIT_CODE|T_HASOBJ|T_PART_OBJ)
				/* Passed to runtime type descriptors */

/* ALLOCDEF "type" 50 */

extern p_type
	bool_type,
	char_type,
	int_type,
	longint_type,
	shortint_type,
	univ_int_type,
	real_type,
	longreal_type,
	shortreal_type,
	univ_real_type,
	string_type,
	std_type,
	nil_type,
	error_type;		/* All from type.c */

extern int
	T_CONSTRUCTED;		/* Not a constant, because when not using
				   age fields, a nodename is a simple
				   type.
				*/

_PROTOTYPE(void init_types, (void));
	/*	Initialization. Adds all predefined types to the symboltable.
	*/

_PROTOTYPE(p_type enum_type, (t_idlst idlist));
	/*	Creates and returns an enumeration type consisting of the
		enumeration literals in 'idlist'.
	*/

_PROTOTYPE(p_type qualified_type, (p_node nd));
	/*	A qualified identifier (which has already been identified)
		is indicated by 'nd'. The identifier must indicate a type.
		This routine performs checks, and returns the type.
	*/

_PROTOTYPE(p_type proc_type, (p_type resulttp, t_dflst params));
	/*	Creates and returns a function type with result type resulttp
		and parameters params.
	*/

_PROTOTYPE(p_type funcaddr_type, (p_type resulttp, t_dflst params));
	/*	Creates and returns a function type with result type resulttp
		and parameters params. It also sets a flag indicating that it
		is a type for a function variable.
	*/

_PROTOTYPE(p_type set_type, (p_type tp));
	/*	Creates and returns a set type with element type 'tp'.
	*/

_PROTOTYPE(p_type bag_type, (p_type tp));
	/*	Creates and returns a bag type with element type 'tp'.
	*/

_PROTOTYPE(p_type array_type, (p_type tp, int ndims, p_ardim dims));
	/*	Creates and returns an array type with number of dimensions
		'ndims', with index type(s) and possibly bounds indicated by
		'dims', and element type 'tp'.
	*/

_PROTOTYPE(p_type get_index_type, (p_type tp, int dimno));
	/*	Returns the index type of the 'dimno'th dimension of array or
		partitioned-object type tp, for the purpose of adding bounds.
		An error message is produced if the array already has bounds
		in the array declaration.
	*/

_PROTOTYPE(p_type nodename_type, (p_type tp));
	/*	Creates and returns a nodename type belonging to graph type
		'tp'.
	*/

_PROTOTYPE(p_type object_type, (p_type tp));
	/*	Creates and returns an object type of the (internal) record
		type created for the object fields.
	*/

_PROTOTYPE(p_type graph_type, (struct scope *sc_root, struct scope *sc_node));
	/*	Creates and returns a graph type. the fields of the graph
		root reside in scope 'sc_root', the fields of the graph nodes
		reside in scope 'sc_node'.
	*/

_PROTOTYPE(p_type record_type, (struct scope *sc));
	/*	Creates and returns a record type. The fields of the record
		reside in scope 'sc'.
	*/

_PROTOTYPE(p_type generic_type, (int kind));
	/*	Creates and returns a generic (formal) type. 'kind' is either
		T_GENERIC, T_NUMERIC, or T_SCALAR.
	*/

_PROTOTYPE(p_type union_type, (struct scope *sc, p_node init));
	/*	Creates and returns a union type. The fields of the union reside
		in scope 'sc'. If 'init' is not NULL, it indicates the
		initial value for the union tag.
	*/

_PROTOTYPE(void remove_type, (p_type tp));
	/*	Release the type indicated by 'tp'. This function should only
		be called with T_FUNCTION types, when a definition of this
		type was already seen (in a specification).
 		The other type structs are needed for the entire compilation.
	*/

_PROTOTYPE(p_type nodename_of_ident, (p_idf id));
	/*	Handles the occurrence of a 'NODENAME OF 'id'.
		A reference to the resulting, possibly incomplete type is
		returned.
	*/

_PROTOTYPE(int tst_equality_allowed, (p_type tp));
	/*	Returns non-zero if items of type 'tp' may be tested for
		(in)equality, 0 otherwise.
	*/

#define chk_type_equiv(tp, nd, s) \
	do { \
		if (! tst_type_equiv(tp, (nd)->nd_type)) { \
			error("type incompatibility in %s", s); \
		} \
	} while (0)

#define chk_enough_bounds(tp, i) \
	do { \
		if ((tp->tp_fund & (T_ARRAY|T_OBJECT)) && i < tp->arr_ndim) { \
			error("too few bounds specified"); \
		} \
	} while (0)

_PROTOTYPE(int tst_type_equiv, (p_type tp1, p_type tp2));
	/*	Tests if two types are equivalent, returns 1 if they are,
		0 if they are not.
	*/

_PROTOTYPE(int tst_proc_equiv, (p_type tp1, p_type tp2));
        /*      Test if two function/process types are equivalent. This routine
		may also be used for the testing of assignment compatibility
		between procedure variables and procedures.
	*/


_PROTOTYPE(int tst_compat, (p_type tp1, p_type tp2));
	/*	Test if two types are compatible.
	*/

_PROTOTYPE(int tst_ass_compat, (p_type tp1, p_type tp2));
	/*	Test if two types are assignment compatible.
	*/

_PROTOTYPE(char *incompat, (p_type tp1, p_type tp2));
	/*	Returns a string suitable for producing an error message
		when types tp1 and tp2 are not compatible. (This may also be
		because one or both are opaque types.
	*/

_PROTOTYPE(void chk_par_compat, (int parno,
				 int is_value_param,
				 p_type formaltp,
				 p_type actualtp,
				 p_def funcdef));
	/*	Check type compatibility of a parameter of a call to 'funcdef',
		parameter 'parno', formal type 'formaltp', actual type
		'actualtp'. 'is_value_param is set when the parameter is
		an IN parameter (in which case only assignment compatibility
		is required.
		An error message is produced in case of an incompatibility.
	*/

_PROTOTYPE(int pure_write, (p_def df));
	/*	Check that the operation indicated by df is a pure write.
	*/

_PROTOTYPE(p_type must_be_discrete_type, (p_node nd, char *s));
	/*	Checks that the type of node 'nd' is discrete, and produces
		an error message when it is not. The type is returned.
		's' is used to produce the error message as such:
			non-discrete type in %s
	*/

_PROTOTYPE(p_type must_be_gather_type, (p_type tp));
        /*      The result type in a GATHER must have the same shape as the
                partitioned object, so it must be an array with the
                same number of dimensions, with compatible index types.
        */


_PROTOTYPE(p_type must_be_aggregate_type, (p_node nd));
	/*	nd must indicate a type, or else an error message is produced.
		The type is returned.
	*/

_PROTOTYPE(p_type named_type_of, (p_type tp));
	/*	Follows the tp_equal links as long as tp->tp_next is the
		nil_type, and then returns tp_next (which should indicate
		the indicated graph type).
	*/

_PROTOTYPE(p_def get_union_variant, (p_type tp, p_node nd));
        /* Check that the expression indicated by "nd" is a legal initial
           value for the tag of the union indicated by "tp". If the
           expression is not constant, we can only check the type. If it
           is, we can also check that it corresponds to one of the
           alternatives. If it does, return it, otherwise return 0.
        */

_PROTOTYPE(p_def get_reduction_func, (p_idf funcid, p_type valtp));
        /*      Check that the function with name funcid may be used as a
                reduction function for values of type valtp.
                Criteria: the function must have two SHARED-parameters of a type
                equivalent with valtp.
                Return the def structure associated with the function.
        */

_PROTOTYPE(void start_nonscalar, (void));
	/*	Indicates that the parser starts reading a non-scalar type.
	 */

_PROTOTYPE(void end_nonscalar, (p_type tp));
	/*	Indicates that the parser is finished reading a non-scalar type.
	*/

#ifdef DEBUG
_PROTOTYPE(int dump_type, (p_type tp, int nested));
	/*	Produce a somewhat readable printout of type tp.
	*/
#endif

#define NULLTYPE ((p_type) 0)

#define result_type_of(tpx)	(tpx)->tp_next
#define record_type_of(tpx)	(tpx)->tp_next
#define param_list_of(tpx)	(tpx)->prc_params
#define element_type_of(tpx)	(tpx)->tp_next
#define generic_actual_of(tpx)	(tpx)->tp_next
#define	is_constructed_type(tpx) \
				((tpx)->tp_fund & T_CONSTRUCTED)
#define	is_opaque_type(tpx)	((tpx)->tp_fund == T_NODENAME && \
				 named_type_of(tpx) == nil_type)
#define get_element_type(tpx)	((tpx)->tp_next ? (tpx)->tp_next : error_type)

#define ufit(n, i)		(((n) & ~full_mask[(i)]) == 0)

#endif /* __TYPE_H__ */
