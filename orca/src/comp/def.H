/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __DEF_H__
#define __DEF_H__

/* I D E N T I F I E R	 D E S C R I P T O R   S T R U C T U R E */

/* $Id: def.H,v 1.46 1998/02/27 12:03:22 ceriel Exp $ */

#include	"ansi.h"
#include	"LLlex.h"
#include	"idf.h"
#include	"node.h"
#include	"list.h"

typedef struct def
		t_def,
		*p_def;

/* All definitions of an identifier are chained, so that for each
   identifier we get a list of definitions. The identifier structure (idf)
   has a field id_def which indicates the head of this list. The df_next
   field of the definition structure (def) is used for the chaining.
   Other fields in the def structure are:
   df_nextinscope
		(p_def)	for chaining all definitions in the same scope;
			this list is indicated by the sc_def field of the
			corresponding scope structure;
   df_idf	(p_idf)	this is a link back to the idf structure; often
			needed to get back at the name;
   df_scope	(p_scope)
			reference to the scope structure of the scope in
			which this definition resides;
   df_name	(char *)
			name used in code generation, usually the name of the
			corresponding C variable/tyoedef;
   df_type	(p_type)
			reference to the type structure describing the type
			of this definition;
   df_kind	(int)	indicates what kind of definition this is, see below
			for a list of possible values;
   df_position	(p_pos)	indicates the place where this definition is declared.
   df_num	(int)	for numbering definitions (used by the optimizer).
   df_flags	(long)	see below for the possible flags;
   df_value		a union containing several fields, depending on df_kind.
			The fields are accessed through macros as if they
			belong to the def structure itself.
*/

/* Possible values for df_kind: */

#define D_MODULE	0x00001 /* a module */
#define D_FUNCTION	0x00002 /* a function */
#define D_VARIABLE	0x00004 /* a variable (or parameter) */
#define D_FIELD		0x00008 /* a record field */
#define D_TYPE		0x00010 /* a type */
#define D_ENUM		0x00020 /* an enumeration literal */
#define D_CONST		0x00040 /* a constant */
#define D_IMPORT	0x00080 /* an imported definition */
#define D_OPAQUE	0x00100 /* an opaque type (changes to D_TYPE when the
				   type definition is found)
				*/
#define D_PROCESS	0x00200 /* a process */
#define D_OBJECT	0x00400 /* a object */
#define D_OPERATION	0x00800 /* an operation */
#define D_UFIELD	0x01000 /* a union field (either normal or tag) */
#define D_OFIELD	0x02000 /* an object field */
#define D_INUSE		0x04000	/* compiler internal: an implicitly imported
				   definition from an enclosing scope
				*/
#define D_ERROR		0x08000 /* compiler internal: a definition for an
				   undeclared identifier
				*/

/* Possible flags in df_flags (there are far too many ...): */

#define D_EXPORTED	0x01	/* set if def resides in a specification */
#define D_USED		0x02	/* set if used */
#define D_DEFINED	0x04	/* set if it is assigned a value */
#define D_SPECSEEN	0x08	/* for functions, operations, processes:
				   set if a specification has been processed.
				*/
#define D_FORLOOP	0x08	/* for variables:
				   set if it is a for-loop control variable.
				*/
#define D_AUTOIMPORT	0x08	/* for imports:
				   set if imported automatically because the
				   enumeration type was imported.
				*/
#define D_GENERICPAR	0x10	/* for types, object types, constants,
				   functions: set if it is a formal parameter
				   of a generic module/object.
				*/
#define D_INPAR		0x10	/* for variables:
				   set if it is an input parameter */
#define D_HASREADS	0x10	/* for operations:
				   set if it has read alternatives.
				*/
#define D_BLOCKING	0x20	/* used for operations and functions:
				   set if it possibly blocks.
				*/
#define D_OUTPAR	0x20	/* for variables:
				   set if it is an output parameter
				*/
#define D_GENERIC	0x20	/* for modules or object types:
				   set if it is a generic
				*/
#define D_BUSY		0x40	/* for modules and object types:
				   set if currently reading its specification.
				   Also used internally for functions.
				*/
#define D_SHAREDPAR	0x40	/* for variables:
				   set if it is a shared parameter
				*/
#define D_TAG		0x40	/* for union fields:
				   set if it is the tag-field
				*/
#define D_HASWRITES	0x40	/* for operations:
				   set if it has write alternatives.
				*/
#define D_SHAREDOBJ	0x80	/* for variables:
				   set if it contains a shared object.
				*/
#define D_NONBLOCKING	0x80	/* for operations and functions:
				   set if it never blocks.
				*/
#define D_INSTANTIATION	0x80	/* for modules and object types:
				   set if it is an instantiation.
				*/
#define D_INSTDONE	0x100	/* for modules and object types:
				   set if the instantiation has been
				   generated
				*/
#define D_BLOCKINGDONE	0x100	/* for operations and functions:
				   set if blocking analysis has been done.
				*/
#define D_COPY		0x100	/* for parameter variables:
				   set if a copy must be made.
				*/
#define D_RWDONE	0x200	/* for operations:
				   set if read/write analysis has been done.
				*/
#define D_MAIN		0x200	/* for modules, objects and processes:
				   set if it contains OrcaMain.
				*/
#define D_EXTRAPARAM	0x200	/* for functions: set if it must have
				   an extra parameter (which is used for passing
				   information to and from DoOperation)
				*/
#define D_SELF		0x200	/* for variables: set if it is SELF. */
#define D_GUARDS	0x400	/* for operations: set if it has guards */
#define D_HASFORKS	0x400	/* for processes: set if it contains forks */
#define D_HAS_SHARED_OBJ_PARAM \
			0x400	/* for functions: set if it has exactly one
				   shared parameter of an object type.
				   These functions are suitable for cloning.
				*/
#define D_INOUT_DONE	0x400	/* for objects and modules: set if it explicitly
				   includes InOut
				*/
#define D_LOWER_BOUND	0x400	/* for object fields: set if it is a
				   partition lower-bound.
				*/
#define D_UPPER_BOUND	0x800	/* for object fields: set if it is a
				   partition upper-bound.
				*/
#define D_INOUT_NEEDED	0x800	/* for objects and modules: set if it needs
				   InOut
				*/
#define D_RETURNEXPR	0x800	/* for objects, operations, functions,
				   processes: set if it contains a return
				   with an expression.
				*/
#define D_OBJ_COPY	0x1000	/* for operations: set if it must copy
				   the object for the write alternatives.
				*/
#define D_PARTITIONED	0x1000	/* for objects: set if it is partitioned.
				*/
#define D_PART_INDEX	0x1000	/* for variables: set if it is a partition
				   index.
				*/
#define D_PARALLEL	0x2000	/* for operations: set if it is a parallel
				   operation.
				*/
#define D_DATA		0x2000	/* for modules and variables: set if it is
				   (write-once) global data
				*/
#define D_IMPL_SEEN	0x4000	/* for modules and objects: set if the
				   implementation is seen.
				*/
#define D_HAS_OFLDSEL	0x4000	/* for operations: set if it has Ofldsel's
				   of object parts other than its own.
				*/
#define D_HAS_COMPLEX_OFLDSEL \
			0x8000	/* for operations: set if it has Ofldsel's
				   of object parts other than its own that are
				   so complicated that we cannot produce a PDG
				   function.
				*/
#define D_GATHERED	0x10000L/* for OUT parameters and operations: set
				   if it is (or has) a gathered OUT parameter.
				*/
#define D_REDUCED	0x20000L/* Only for OUT parameters and operations: set
				   if it is (or has) a reduced OUT parameter.
				*/
#define D_CALLS_OP	0x40000L/* for functions, operations, and processes:
				    set if it may cause a call to an operation
				*/
#define D_HAS_DEPENDENCIES \
			0x80000L/* for operations: set if it has a dependency
				   section.
				*/
#define D_SIMPLE_DEPENDENCIES \
			0x100000L/* for operations: set if it has a "simple"
				    dependency section (one which can be
				    transferred into a PDG function).
				*/

/* For df_kind = D_VARIABLE, the following fields exist:
   var_bat	(p_node)
			the list of bounds-and-tag expressions in the
			declaration of this variable;
   var_tmpvar	(struct tmp *)
			indicates the temporary variable for the FOR-loop when
			the def structure indicates a FOR-loop control variable;
   var_level	(int)	indicates the nesting level of a FOR-loop control
			variable;
   var_gathertp	(p_type)
			indicates the gathered type when it is a gathered OUT
			parameter;
   var_reducef	(p_def)	indicates the reduction function when it is a reduced
			OUT parameter.
*/

struct variable {	/* a variable */
    union {
	p_node	va_bat;		/* bounds and tag (if any) */
	int	va_level;	/* nesting level of FOR-loop variable */
    } va_xxx;
    union {
	struct tmp
		*va_tmpvar;	/* if it is a temporary */
	struct type
		*va_gathertp;	/* When it is a gathered OUT parameter */
	p_def	va_reducef;	/* When it is a reduced OUT parameter */
    } va_yyy;
#define var_bat		df_value.df_variable.va_xxx.va_bat
#define var_level	df_value.df_variable.va_xxx.va_level
#define var_tmpvar	df_value.df_variable.va_yyy.va_tmpvar
#define var_gathertp	df_value.df_variable.va_yyy.va_gathertp
#define var_reducef	df_value.df_variable.va_yyy.va_reducef
};

/* For df_kind = D_CONSTANT, the following field exists:
   con_const	(p_node)
			indicates the expression node containing the value
			resulting from the evaluation of the constant
			expression.
*/

struct constant {	/* a constant */
    p_node	co_const;	/* result of a constant expression */
#define con_const	df_value.df_constant.co_const
};

/* For df_kind = D_ENUM, the following fields exist:
   enm_val	(int)	the value of this enumeration literal;
   enm_next	(p_def)	the next enumeration literal of this type. We need the
			literals together when an enumeration type is imported.
			In that case, the enumeration literals must be imported
			as well.
*/

struct enumval {	/* an enumeration literal */
    int		en_val;		/* value of this enumeration literal */
    p_def	en_next;	/* next enumeration literal */
#define enm_val		df_value.df_enum.en_val
#define enm_next	df_value.df_enum.en_next
};

/* For df_kind = D_FIELD or df_kind = D_OFIELD or df_kind = D_UFIELD, the
   following fields exist:
   fld_bat	(p_node)
			the list of bounds-and-tag expressions in the
			declaration of this field;
   fld_dimno	(int)	for the upper and lower bounds of a partitioned object
			(which are stored as a D_OFIELD), the corresponding
			dimension;
   fld_tagvalue (p_node)
			the expression node containing the value selecting
			this union field (only for df_kind = D_UFIELD).
*/

struct field {		/* a record/union/object field */
    union {
	p_node	fd_bat;		/* bounds and tag (if any) */
	int	fd_dimno;	/* n-th dimension for partition bounds */
    } fd_xxx;
    p_node	fd_tagvalue;	/* const-expr to left of "=>" */
#define fld_bat		df_value.df_field.fd_xxx.fd_bat
#define fld_dimno	df_value.df_field.fd_xxx.fd_dimno
#define fld_tagvalue	df_value.df_field.fd_tagvalue
};

/* Mechanism for creating lists of definitions. */

typedef p_list	t_dflst;
#define def_initlist(p)		initlist(p)
#define def_delist(p, e)	delist(p, (void **) (e))
#define def_enlist(p, e)	do { \
					if (! t_dflstmember(*p, e)) { \
						enlist(p, (void *) (e)); \
					} \
				} while (0)
#define def_endlist(p)		endlist(p)
#define def_emptylist(p)	emptylist(p)
#define def_walklist(a,b,c)	walklist(a,b,c,p_def)
#define def_getlistel(a)	getlistel(a, p_def)
#define def_setlistel(a,b)	setlistel(a, b)
#define def_nextlistel(a)	nextlistel(a)
#define t_dflstmember(p, e)	listmember(p, (void *) e)

/* For df_kind = D_OBJECT, D_OPERATION, D_FUNCTION, or D_PROCESS, the following
   fields exist:
   bod_scope	(p_scope)
			the scope defined by this
			function/process/operation/object;
   bod_statlist1
		(p_node)
			list of statements in body or object initialization.
			for an operation, after read/write analysis only
			the read alternatives are stored here;
   bod_statlist2
		(p_node)
			The write alternatives are stored here after
			read/write analysis;
   bod_init	(p_node)
			the code resulting from bounds-and-tag expressions;
   bod_temps	(struct tmp *)
			list of temporaries required;
   bod_transdep (def_list)
			list of definitions on which the translation
			depends;
   bod_argtabsz (int)	when the generated code has to call DoOperation,
			it has to build an array of arguments. These are
			stored in an array of size bod_argtabsz;
   bod_marshallnames
		((char *)[7])
			names used for marshalling routines.
*/

typedef struct body {
    struct scope
		*scope;
    p_node	statlist1;
    p_node	statlist2;
    p_node	init;
    struct tmp	*temps;
    t_dflst	transdep;
    int		argtabsz;
    char	*marshallnames[7];
} t_body;

/* For df_kind = D_MODULE or df_kind = D_OBJECT. the following fields exist:
   mod_imports	(t_dflst)
			the list of modules/objects imported by this
			module/object, which is split out in
   mod_hincludes
		(t_dflst)
			the list of modules/objects imported in the
			specification, and
   mod_cincludes
		(t_dflst)
			the list of modules/objects imported in the
			implementation;
   mod_stringlist
		(p_string)
			the list of literal strings in this module/object;
   mod_file	(char *)
			the basename of the file in which the module/object
			resides;
   mod_name	(char *)
			the name of the initialisation routine of this
			module/object (used for code generation);
   mod_dir	(char *)
			the directory in which the module/object resides, with
			respect to the current directory;
   mod_funcaddrname
		(char *)
			name of the generated table of functions of which the
			address is taken in this module/object.
   mod_funcaddrcnt
		(int)	number of functions of which the address is taken in
			this module/object;
   mod_reductionfuncs
		(t_dflst)
			list of functions which are used in this module/object
			as reduction function;
   mod_initname	(char *)
			for partitioned objects, the name of the initialization
			operation;
   mod_funcaddrs
		(t_dflst)
			the list of functions of which the address is taken
			in this module/object;
   mod_gendf	(p_def)
			for an instantiation, the def structure of the
			generic module/object from which is instantiated;
   mod_fn	(char *)
			contains the name of the C variable containing the
			source filename (for producing runtime error messages).
*/

typedef struct unit {
    p_def	gendf;
    int		funcaddrcnt;
    char	*funcaddrname;
    t_dflst	funcaddrs;
    t_dflst	reductionfuncs;
    char	*dir;
    t_dflst	imports;
    t_dflst	hincludes;
    t_dflst	cincludes;
    char	*name;
    t_string	*stringlist;
    char	*file;
    char	*initname;
    char	*fn;
} t_unit;

/* For df_kind = D_OPERATION, D_PROCESS, or D_FUNCTION, the following fields
   exist:
   prc_name	(char *)
			for processes, the (runtime) name of the process
			descriptor;
   prc_addrname	(char *)
			name of function of which the address is taken
			(or the name of a wrapper function). The problem here
			is that functions sometimes get an extra parameter,
			so functions that are called through a function
			variable always get the extra parameter. If a
			function of which the address is taken does not get
			such an extra parameter, a wrapper function is
			generated which has one;
   prc_funcno	(int)	for functions, the number of this function (only used
			when its address is taken). Each of these functions
			gets an entry in an array where the runtime system
			will store the identification of the function.
			For operations, the number of this operation;
   prc_blockdep (t_dflst)
			list of functions/operations on which the blocking
			;ehaviour of this function/operation depends;
   prc_patstr	(char *)
			access pattern as a string;
   prc_pattern	(struct pattern *)
			access pattern in an internal format;
   prc_exp_pattern
		(struct pattern *)
			expanded access pattern in an internal format;
   opr_reducef	(p_def)	for a parallel operation, the reduction function in case
			the result is reduced;
   opr_dependencies
		(p_node)
			for a parallel operation, the dependencies code;
   opr_namer	(char *)
			for operations, the name of the function implementing
			the read alternatives;
   opr_namew	(char *)
			for operations, the name of the function implementing
			the write alternatives;
   df_stdname	(int)	for standard (built-in) functions, the value
			identifying which function. The values are defined
			in oc_stds.h;
*/

struct dfproc {
    t_dflst	blockdep;
    char	*name;
    char	*addrname;
    int		funcno;
    char	*patstr;
    struct pattern
		*pattern,
		*exp_pattern;
    p_def	reducef;
    p_node	dependencies;
};

struct dfprocX {	/* function, process, object, module, or operation */
    t_body	*pr_body;
    union {
	struct dfproc
		*pr_proc;
	t_unit	*pr_unit;
    }	pr_xxx;
#define df_proc df_value.df_prc.pr_xxx.pr_proc
#define df_unit df_value.df_prc.pr_xxx.pr_unit
#define df_body	df_value.df_prc.pr_body
};

#define mod_hincludes	df_unit->hincludes
#define mod_cincludes	df_unit->cincludes
#define mod_imports	df_unit->imports
#define mod_dir		df_unit->dir
#define mod_gendf	df_unit->gendf
#define mod_name	df_unit->name
#define mod_funcaddrs	df_unit->funcaddrs
#define mod_funcaddrname \
			df_unit->funcaddrname
#define mod_funcaddrcnt	df_unit->funcaddrcnt
#define mod_reductionfuncs \
			df_unit->reductionfuncs
#define mod_file	df_unit->file
#define mod_stringlist	df_unit->stringlist
#define mod_initname	df_unit->initname
#define mod_fn		df_unit->fn

#define prc_blockdep	df_proc->blockdep
#define prc_name	df_proc->name
#define prc_addrname	df_proc->addrname
#define prc_funcno	df_proc->funcno
#define prc_patstr	df_proc->patstr
#define prc_pattern	df_proc->pattern
#define prc_exp_pattern	df_proc->exp_pattern

#define bod_scope	df_body->scope
#define bod_statlist1	df_body->statlist1
#define bod_statlist2	df_body->statlist2
#define bod_init	df_body->init
#define bod_temps	df_body->temps
#define bod_transdep	df_body->transdep
#define bod_argtabsz	df_body->argtabsz
#define bod_marshallnames \
			df_body->marshallnames

#define opr_namer	df_name
#define opr_namew	prc_name
#define opr_depname	prc_patstr
#define opr_reducef	df_proc->reducef
#define opr_dependencies \
			df_proc->dependencies
#define df_stdname	prc_funcno

/* For df_kind = D_IMPORT or df_kind = D_INUSE, the following field exists:
   imp_def	(p_def)
			indicates the imported definition.
*/

struct import {		/* an imported definition */
    p_def	im_def;	/* imported definition */
#define imp_def		df_value.df_import.im_def
};

/* For df_kind = D_TYPE, the following field exists:
   tdf_forw_list (struct forw *)
			this is used when a type name is mentioned which
			is not declared yet (in a nodename declaration).
			In that case, a list of references to its type
			structure is built, and at the end of the scope
			the declaration must actually be present, or be
			taken from an enclosing scope. If present, the
			tdf_forw_list is used to fill in the graph type in
			the nodename type structures. After that, the list
			is killed.
*/

struct forw {		/* forwards graph-type definition */
    struct forw	*f_next;	/* next nodename type referring to this type */
    t_pos	f_pos;		/* position of current reference */
    struct type	*f_nodename;	/* the nodename type referring to this type */
};
#define tdf_forw_list	df_value.df_forw_list

struct def	{		/* list of definitions for a name */
    p_def	df_next,	/* next definition in definitions chain */
		df_nextinscope;	/* link all definitions in a scope */
    p_idf	df_idf;		/* link back to the name */
    struct scope
		*df_scope;	/* scope in which this definition resides */
    char	*df_name;	/* name used for code generation */
    int		df_kind;	/* the kind of this definition */
    long	df_flags;
    struct type
		*df_type;
    t_pos	df_position;	/* where declared? */
    int		df_num;		/* number (for optimizations) */
    union {
	struct variable
		df_variable;
	struct constant
		df_constant;
	struct enumval
		df_enum;
	struct field
		df_field;
	struct import
		df_import;
	struct dfprocX
		df_prc;
	struct forw
		*df_forw_list;
    }	df_value;
};

#define D_ISTYPE	(D_OPAQUE|D_TYPE|D_OBJECT)
#define D_IMPORTED	(D_IMPORT|D_INUSE)
#define is_type(dfx)	((dfx)->df_kind & D_ISTYPE)

#define is_parameter(df)	((df)->df_flags&(D_INPAR|D_OUTPAR|D_SHAREDPAR))
#define	is_shared_param(df)	((df)->df_flags&D_SHAREDPAR)
#define	is_in_param(df)		((df)->df_flags&D_INPAR)
#define	is_out_param(df)	((df)->df_flags&D_OUTPAR)

/* ALLOCDEF "body" 32 */

/* ALLOCDEF "dims" 32 */

/* ALLOCDEF "unit" 32 */

/* ALLOCDEF "dfproc" 32 */

/* ALLOCDEF "forw" 5 */

/* ALLOCDEF "def" 50 */

_PROTOTYPE(p_def define, (p_idf id, struct scope *sc, int kind));
	/*	Declares an identifier 'id' of kind 'kind'  in scope sc, but
		first checks if it already has been defined.
		If so, then checks for the cases in which this is legal,
		and otherwise gives an error message.
		Returns a pointer to the resulting def structure.
	*/

_PROTOTYPE(p_def lookup, (p_idf id, struct scope *sc, int import));
	/*	Looks up a definition of an identifier 'id' in scope 'sc'.
		If 'import' is set and the scope is a closed scope, also
		looks for it in the pervasive scope.
		Returns a pointer to its def structure if it exists,
		otherwise returns 0.
	*/

_PROTOTYPE(p_def lookfor, (p_idf id, struct scope *sc, int give_message));
	/*	Looks for identifier 'id' in the visibility range started by
		'sc'.  If it is not defined creates a dummy definition and,
		if give_message is set, gives an error message.
	*/

_PROTOTYPE(void end_definition_list, (p_def *pdf));
	/*	Removes all imports from the list started with *pdf. This is
		neccesary after reading a specification module/object  because
		the implementation module/object might import them again.
	*/

_PROTOTYPE(void remove_from_id_list, (p_def df));
	/*	Removes definition "df" from its definition list.
	*/

_PROTOTYPE(p_def start_impl, (p_idf id, int kind, int generic));
	/*	A new implementation unit begins.  Its name is indicated by
		'id', its kind is indicated by 'kind' (D_MODULE, D_DATA, or
		D_OBJECT), and whether or not it is generic is indicated by
		'generic'.
	*/

_PROTOTYPE(void end_impl, (p_def df));

_PROTOTYPE(p_def start_spec, (p_idf id, int kind, int generic));
	/*	A new specification unit begins. Its name is indicated by 'id',
		its kind is indicated by 'kind' (D_MODULE, D_DATA, or D_OBJECT),		and whether or not it is generic is indicated by 'generic'.
	*/

_PROTOTYPE(void end_spec, (p_def df));

_PROTOTYPE(p_def start_proc, (int kind, p_idf id, int in_impl));
	/*	A function or process is declared, either in a specification
		or implementation. Creates a def structure for it
		(if neccessary).
	*/

_PROTOTYPE(void end_proc, (p_def, int));

_PROTOTYPE(void check_with_earlier_defs, (p_def df,
					  t_dflst params,
					  struct type *result));
	/*	Check the header of a function, process, or operation
		declaration with parameters 'params and result type 'result'
		against a possible earlier specification in the module/object
		specification.
	*/

_PROTOTYPE(void check_parallel_operation_spec,
		(p_def df, t_idlst indexids, t_dflst params,
		 struct type *resulttp, p_def resultreduce));
	/*	Check the header of a parallel operation with indices
		'indexids', parameters 'params' and resulttype 'resulttp' which
		may be reduced with 'resultreduce' with a possible earlier
		specification in the object specification.
	*/

_PROTOTYPE(void chk_usage, (void));
	/*	All variables in the current scope are checked and warnings are
		produced if they are not used/assigned.
	*/

#ifdef DEBUG
_PROTOTYPE(int dump_def, (p_def df));
	/*	Produce a somewhat readable dump of 'df'.
	*/
#endif

_PROTOTYPE(p_def declare_type, (p_idf id, struct type *tp, int allow_opaque));
	/*	A type with type-description "tp" is declared and must
		be bound to definition "df".
		This routine also handles the case that the type-field of
		"df" is already bound. In that case, it is either an opaque
		type, or an error message was given when "df" was created.
	*/

_PROTOTYPE(p_def declare_const, (p_idf id, p_node expr));
	/*	Declares a CONST identifier 'id' and associates constant
		expression 'expr' with it.
	*/

_PROTOTYPE(void declare_fieldlist, (t_idlst idlist,
				    struct type *type,
				    p_node bat,
				    struct scope *scope));
	/*	Put 'idlist', a list of fields, in the symbol table.
		They all have type "type", bounds-and-tag expression 'bat',
		and are put in scope "scope".
	*/

_PROTOTYPE(void declare_unionel, (p_idf id,
				  struct type *type,
				  p_node bat,
				  struct scope *scope,
				  int istag,
				  p_node tagval,
				  struct type *tagtype));
	/*	Put a field 'id' of a union in the symbol table.
		If istag=1, the field is the tag-field of the union, otherwise
		it is a normal field of the form "tagvalue => id : type".
		Put it in scope "scope".
	*/

_PROTOTYPE(void declare_varlist, (t_idlst idlist,
				  struct type *type,
				  p_node bat,
				  int kind));
	/*	Enter a list of identifiers representing variables or object
		fields into the name list. "type" represents the type of the
		variables.  "bat" is the parse-tree for the bounds and tag of
		the variables.	"kind" is either D_OFIELD or D_VARIABLE.
	*/

_PROTOTYPE(void declare_paramlist, (int prockind,
				    t_dflst *ppr,
				    t_idlst idlist,
				    struct type *type,
				    int mode,
				    p_node bat,
				    p_def reduction_func));
	/*	Create (part of) a parameterlist of a procedure.
		"ids" indicates the list of identifiers, "type" their type, and
		"mode" indicates D_INPAR, D_OUTPAR, or D_SHAREDPAR.
		"bat" is the bounds-or-tag expression, and "reduction_func"
		a possible reduction function in case of an OUT parameter
		from a parallel operation.
		"prockind" indicates the type of the body. Possible values:
		D_OPERATION, D_OPERATION|D_PARALLEL, D_FUNCTION, D_PROCESS.
	*/

_PROTOTYPE(void handle_imports, (p_idf from_id, t_idlst idlist));
	/*	If from_id is 0, idlist is a list of modules/objects to be
		imported. If not, idlist is a list of identifiers to be
		imported from module/object from_id.
	*/

_PROTOTYPE(p_node id2defnode, (p_idf id));
	/*	Creates and returns a Def node for identifier 'id'.
	*/

_PROTOTYPE(void add_to_stringtab, (t_string *));
	/*	Add string to string table, but only if it must
		be generated for this module/object.
	*/

#define NULLDEF ((p_def) 0)

#endif /* __DEF_H__ */
