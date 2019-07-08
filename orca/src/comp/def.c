/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S Y M B O L	 D E F I N I T I O N   M E C H A N I S M */

/* $Id: def.c,v 1.42 1997/11/03 13:33:43 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<stdio.h>
#include	<alloc.h>
#include	<assert.h>

#include	"def.h"
#include	"scope.h"
#include	"type.h"
#include	"misc.h"
#include	"main.h"
#include	"specfile.h"
#include	"error.h"
#include	"options.h"
#include	"node.h"
#include	"f_info.h"

_PROTOTYPE(static t_def *do_import, (t_def *, t_scope *, int));
_PROTOTYPE(static char *df_kind_of, (t_def *));
_PROTOTYPE(static void df_warning, (t_pos *, t_def *, char *));
_PROTOTYPE(static int is_defined, (t_type *, p_node));
_PROTOTYPE(static t_def *mk_def, (t_idf *, t_scope *, int));
_PROTOTYPE(static void import_effects, (t_def *, struct scope *, int));
_PROTOTYPE(static void add_opn, (t_def *, char *, int));
_PROTOTYPE(static void enter_from_import_list, (t_idlst, t_def *));
_PROTOTYPE(static void check_filename, (t_def *, char *));
_PROTOTYPE(static void number_operations, (t_def *));
_PROTOTYPE(static void add_self, (p_def));
_PROTOTYPE(static void add_read_write, (p_def));

t_def *
start_impl(id, kind, generic)
    t_idf   *id;	/* name of implementation module/object */
    int	    kind;	/* D_DATA, D_MODULE, or D_OBJECT */
    int	    generic;	/* is it a generic module/object? */
{
    int	    base_kind = kind == D_DATA ? D_MODULE : kind;
    t_def   *df = get_specification(id, 0, base_kind, generic, (p_idf) 0);

    check_filename(df, kind == D_DATA ? "data module" :
		       kind == D_MODULE ? "module" : "object");

    if (! CurrDef) CurrDef = df;
    CurrentScope = df->bod_scope;

    if (! is_anon_idf(df->df_idf) && df->df_kind == D_MODULE) {
	if ((kind & D_DATA) != (df->df_flags & D_DATA)) {
	    if (kind == D_DATA) {
		error("\"%s\": no DATA MODULE specification", id->id_text);
	    }
	    else {
		error("\"%s\": no DATA MODULE implementation", id->id_text);
	    }
	}
    }
    df->df_flags |= D_IMPL_SEEN;

    if (kind == D_OBJECT) {
	add_self(df);
    }
    return df;
}

void
end_impl(df)
    t_def   *df;
{
    if (df->df_kind == D_OBJECT && df->df_type != error_type) {
	/* Create record type for object again, to get the tp_flags right
	   (for initialization, etc). This depends on the fields of the
	   record, but when the type was created, the record did not have any
	   fields yet.
	*/
	if (df->df_type != error_type) {
	    number_operations(df);
	    free_type(record_type_of(df->df_type));
	    record_type_of(df->df_type) = record_type(CurrentScope);
	}
    }
    chk_procs();
    chk_usage();
}

t_def *
start_spec(id, kind, generic)
    t_idf   *id;
    int	    kind;
    int	    generic;
{
    int	    base_kind = kind == D_DATA ? D_MODULE : kind;
    t_def   *df = define(id, GlobalScope, base_kind);
    t_def   *tf;

    check_filename(df, kind == D_DATA ? "data module" :
		       kind == D_MODULE ? "module" : "object");

    if (! CurrentScope->sc_name) {
	CurrentScope->sc_name = dot.tk_idf->id_text;
	CurrentScope->sc_genname = dot.tk_idf->id_text;
    }

    if (! CurrDef) CurrDef = df;
    def_initlist(&(df->mod_imports));
    df->mod_dir = WorkingDir;
    CurrentScope->sc_definedby = df;
    df->bod_scope = CurrentScope;
    df->df_flags |= D_BUSY;
    df->df_type = record_type(CurrentScope);
    if (generic) df->df_flags |= D_GENERIC;
    switch(kind) {
    case D_DATA:
	df->df_flags |= D_DATA;
	break;
    case D_OBJECT:
	df->df_flags |= D_EXPORTED;
	df->df_type = object_type(df->df_type);
	df->df_type->tp_def = df;
	/* Create a new compiler-internal type with the same name as the
	   object; this allows the object-name to be used inside the object
	   specification and implementation parts. We cannot just use an
	   import, because then "FROM obj IMPORT obj" does not work.
	*/
	tf = define(dot.tk_idf, CurrentScope, D_OBJECT);
	tf->df_type = df->df_type;
	tf->bod_scope = CurrentScope;
	tf->df_flags |= (df->df_flags&~D_BUSY)|D_USED;
	add_read_write(df);
	break;
    }
    return df;
}

void
end_spec(df)
    t_def   *df;
{
    df->df_flags &= ~D_BUSY;
    end_definition_list(&(df->bod_scope->sc_def));
    if (df->df_kind == D_OBJECT) {
	number_operations(df);
    }
}

static void
number_operations(df)
    t_def   *df;
{
    /*	Give operations in an object a number. As the internal
	READ_ and WRITE_ are added first, they automatically
	get numbers 0 and 1.
    */
    int	    count = 0;
    t_def   *d;

    for (d = record_type_of(df->df_type)->rec_scope->sc_def;
	 d;
	 d = d->df_nextinscope) {
	if (d->df_kind == D_OPERATION) {
	    d->prc_funcno = count++;
	}
    }
}

static void
check_filename(df, type)
    t_def   *df;
    char    *type;
{
    char    *p = get_basename(FileName);

    if (! is_anon_idf(df->df_idf) &&
	strncmp(df->df_idf->id_text, p, strlen(p))) {
	error("filename \"%s\" does not match %s name \"%s\"",
	      FileName,
	      type,
	      df->df_idf->id_text);
    }
    if (df->mod_file) {
	if (strcmp(df->mod_file, p)) {
	    error("filename \"%s\" does not match filename \"%s.spf\"", FileName, df->mod_file);
	}
	free(p);
    }
    else df->mod_file = p;
}

void
declare_fieldlist(idlist, type, bounds_and_tag, scope)
	t_idlst	idlist;
	t_type	*type;
	t_scope	*scope;
	p_node	bounds_and_tag;
{
	/*	Put a list of fields in the symbol table.
		They all have type "type", and are put in scope "scope".
	*/
	t_def	*df;
	t_idlst	l;
	t_idf	*id;

	idf_walklist(idlist, l, id) {
		df = define(id, scope, D_FIELD);
		if (df->df_kind != D_ERROR) {
			df->df_type = type;
			df->fld_bat = bounds_and_tag;
		}
	}
	idf_killlist(&idlist);
}

void
declare_unionel(id, type, bat, scope, istag, tagval, tagtype)
	t_idf	*id;
	t_type	*type;
	t_scope	*scope;
	int	istag;
	p_node	tagval;
	p_node	bat;
	t_type	*tagtype;
{
	/*	Put a field of a union in the symbol table.
		If istag=1, the field is the tag-field of the union, otherwise
		it is a normal field of the form "tagvalue => id : type".
		Put it in scope "scope".
	*/
	t_def	*df;

	df = define(id, scope, D_UFIELD);
	df->df_type = type;
	if (istag) {
		df->df_flags |= D_TAG;
		if (!(type->tp_fund & T_DISCRETE)) {
			error("non-discrete type in union-tag field");
		}
	}
	else	{
		assert(tagval != 0);
		chk_type_equiv(tagtype, tagval, "union-tag value");
		df->fld_tagvalue = tagval;
	}
	df->fld_bat = bat;
}

static int
is_defined(tp, bat)
	t_type	*tp;
	p_node	bat;
{
	p_node	nd;

	if (node_emptylist(bat)) {
		if (tp->tp_flags & T_NEEDS_INIT) return 0;
		return D_DEFINED;
	}

	nd = node_getlistel(bat);
	if (tp->tp_fund == T_ARRAY || tp->tp_fund == T_OBJECT) {
		p_node	n = bat;
		int	i;

		for (i = 0; i < tp->arr_ndim; i++) {
			n = node_nextlistel(n);
		}
		if (is_defined(element_type_of(tp), n)) return D_DEFINED;
		if (nd->nd_left->nd_class == Value
		    && nd->nd_right->nd_class == Value
		    && nd->nd_left->nd_int > nd->nd_right->nd_int) return D_DEFINED;
		return 0;
	}
	if (tp->tp_fund == T_UNION && nd->nd_class == Value) {
		t_def *df = tp->rec_scope->sc_def;

		while ((df = df->df_nextinscope)) {
			if (df->fld_tagvalue->nd_int == nd->nd_int) {
				return is_defined(df->df_type, node_nextlistel(bat));
			}
		}
	}
	return 0;
}

void
declare_varlist(idlist, type, bat, kind)
	t_idlst	idlist;
	t_type	*type;
	p_node	bat;
	int	kind;
{
/*	Enter a list of identifiers representing variables or object fields
	into the name list. "type" represents the type of the variables.
	"bat" is the parse-tree for the bounds and tag of the variables.
	"kind" is either D_OFIELD or D_VARIABLE.
*/
	int	defined_flag = is_defined(type, bat);
	t_idlst	l;
	t_idf	*id;

	idf_walklist(idlist, l, id) {
		t_def *df;

		df = define(id, CurrentScope, kind);
		if (df->df_kind != D_ERROR) {
			df->df_type = type;
			df->var_bat = bat;
		}
		if (CurrentScope->sc_definedby->df_kind == D_MODULE) {
			df->df_flags |= D_DATA;
		}
		if (kind == D_VARIABLE) df->df_flags |= defined_flag;
	}
	idf_killlist(&idlist);

	/* Propagate the T_NOEQ and T_DYNAMIC info of the type into
	   the record type of the object. This must be done because it
	   is not done when the record type is created (because the
	   object fields were not known at that time).
	*/
	if (kind == D_OFIELD) {
	    record_type_of(CurrentScope->sc_definedby->df_type)->tp_flags |=
		type->tp_flags & (T_NOEQ|T_DYNAMIC);
	}
}

void
declare_paramlist(EnclType, ppr, idlist, type, mode, bat, reduction_f)
	int	EnclType;  /* D_PROCESS, D_FUNCTION, etc. */
	t_dflst	*ppr;
	t_idlst	idlist;
	t_type	*type;
	int	mode;
	p_node	bat;
	t_def	*reduction_f;
{
	/*	Create (part of) a parameterlist of a procedure.
		"ids" indicates the list of identifiers, "tp" their type, and
		"mode" indicates D_INPAR, D_OUTPAR, or D_SHAREDPAR.
	*/
	int	defined_flag = is_defined(type, bat);
	t_idlst	l;
	t_idf	*id;

	switch(EnclType) {
	  case D_OPERATION|D_PARALLEL:
	  case D_OPERATION:
		if (mode == D_SHAREDPAR) {
			error("no shared parameters allowed in operation");
		}
		break;
	  case D_PROCESS:
		if (mode == D_OUTPAR) {
			error("no out parameters allowed in process");
		}
		if (mode == D_SHAREDPAR && type->tp_fund != T_OBJECT
		    && type != error_type) {
			if (type->tp_fund == T_ARRAY &&
			    element_type_of(type)->tp_fund == T_OBJECT) {
				/* HACK: allow arrays of objects. */
				break;
			}
			error("only objects may be shared");
		}
		break;
	  case D_FUNCTION:
		break;
	  default:
		crash("bad EnclType argument to declare_paramlist");
	}

	idf_walklist(idlist, l, id) {
		t_def *df;

		if (id != 0) {
			df = define(id, CurrentScope, D_VARIABLE);
		}
		else {
			/* Can only happen when a procedure type is defined */
			assert(EnclType == D_FUNCTION);
			df = new_def();
		}
		if (df->df_kind != D_ERROR) {
			df->var_bat = bat;
			df->df_type = type;
			df->var_reducef = reduction_f;
			if (reduction_f) {
				mode |= D_REDUCED;
			}
			else if (mode == D_OUTPAR && EnclType == (D_PARALLEL|D_OPERATION)) {
				mode |= D_GATHERED;
				df->var_gathertp = type;
				df->df_type = element_type_of(type);
			}
		}
		df->df_flags |= mode;
		def_enlist(ppr, df);
		/* OUT and SHARED parameters need not be used (read) */
		if (mode != D_INPAR) df->df_flags |= D_USED;
		/* for OUT parameters, it depends on the type,
		   as determined at the start of this procedure
		*/
		if (mode & D_OUTPAR) df->df_flags |= defined_flag;
	}
	idf_killlist(&idlist);
}

static void
import_effects(idef, scope, flag)
	t_def	*idef;
	t_scope	*scope;
	int	flag;
{
	/*	Handle side effects of an import:
		- importing an enumeration type also imports literals
	*/
	t_def	*df = idef;
	t_type	*tp;

	while (df->df_kind & D_IMPORTED) {
		df = df->imp_def;
	}

	tp = df->df_type;
	if (df->df_kind == D_TYPE
	    && tp->tp_fund == T_ENUM) {
		/* Also import all enumeration literals
		*/
		for (df = tp->enm_enums; df; df = df->enm_next) {
			if (! do_import(df, scope, flag|D_AUTOIMPORT)) {
				assert(0);
			}
		}
	}
	else if (df->df_kind == D_MODULE || df->df_kind == D_OBJECT) {
		if (df->bod_scope == CurrentScope) {
			error("cannot import \"%s\" in \"%s\"",
				df->df_idf->id_text, df->df_idf->id_text);
		}
	}
}

static t_def *
do_import(df, scope, flag)
	t_def	*df;
	t_scope	*scope;
	int	flag;
{
	/*	Definition "df" is imported to scope "scope".
	*/
	t_def	*idef = define(df->df_idf, scope, D_IMPORT);

	if (idef->df_kind != D_ERROR) {
		idef->imp_def = df;
	}
	idef->df_flags |= flag;
	import_effects(idef, scope, flag);
	return idef;
}

static void
enter_from_import_list(idlist, FromDef)
	t_idlst	idlist;
	t_def	*FromDef;
{
	/*	Import the list idlist from the module indicated by Fromdef.
	*/
	t_scope	*sc;
	t_def	*df;
	char	*module_name = FromDef->df_idf->id_text;
	t_idlst	l;
	t_idf	*id;

	switch(FromDef->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		sc = FromDef->bod_scope;
		if (sc == CurrentScope) {
		error("cannot import from current module \"%s\"", module_name);
			return;
		}
		break;
	default:
		error("identifier \"%s\" is not a module", module_name);
		/* fall through */
	case D_ERROR:
		/* We also end up here if some specification could not
		   be found.
		*/
		sc = CurrentScope;
		break;
	}

	idf_walklist(idlist, l, id) {
		if (! (df = lookup(id, sc, 0))) {
			if (FromDef->df_kind != D_ERROR && ! is_anon_idf(id)) {
				error("identifier \"%s\" not declared in module \"%s\"",
					id->id_text,
					module_name);
			}
			df = define(id,sc,D_ERROR);
		}
		else if (! (df->df_flags & D_EXPORTED)) {
			if (! is_anon_idf(df->df_idf)) {
				error("identifier \"%s\" not declared in \"%s\"",
					id->id_text,
					module_name);
			}
		}
		else if (df->df_kind & D_OPERATION) {
			error("operation \"%s\" may not be imported",
				id->id_text);
		}
		else if (! do_import(df, CurrentScope, 0)) assert(0);
	}
	idf_killlist(&idlist);
}

void
handle_imports(FromId, idlist)
	t_idf	*FromId;
	t_idlst	idlist;
{
	t_def	*df;
	t_idf	*id;
	t_idlst	l;

	if (FromId) {
		df = get_specification(FromId, 1, D_MODULE|D_OBJECT, 0, (p_idf) 0);
		enter_from_import_list(idlist, df);
	}
	else {
		idf_walklist(idlist, l, id) {
			df = get_specification(id, 1, D_MODULE|D_OBJECT, 0, (p_idf) 0);
			if (! do_import(df, CurrentScope, 0)) assert(0);
		}
		idf_killlist(&idlist);
	}
}

static t_def *
mk_def(id, scope, kind)
	t_idf	*id;
	t_scope	*scope;
	int	kind;
{
	/*	Create a new definition structure in scope "scope", with
		id "id" and kind "kind".
	*/
	t_def	*df;

	df = new_def();
	df->df_idf = id;
	df->df_scope = scope;
	df->df_kind = kind;
	df->df_next = id->id_def;
	df->df_position = dot.tk_pos;
	if (Specification) {
		df->df_flags = D_EXPORTED;
	}
	assert(! id->id_def || id->id_def->df_idf == id);
	id->id_def = df;
	if (kind == D_ERROR) df->df_type = error_type;
	if (kind & (D_TYPE|D_CONST|D_OPAQUE)) {
		df->df_flags |= D_DEFINED;
	}

	/* enter the definition in the list of definitions in this scope
	*/
	if (scope->sc_end) scope->sc_end->df_nextinscope = df;
	else scope->sc_def = df;
	scope->sc_end = df;
	if (kind & (D_ERROR|D_PROCESS|D_OPERATION|D_FUNCTION|D_MODULE|D_OBJECT)) {
		df->df_body = new_body();
		def_initlist(&df->bod_transdep);
		if (kind & (D_PROCESS|D_OPERATION|D_FUNCTION)) {
			df->df_proc = new_dfproc();
			def_initlist(&df->prc_blockdep);
		}
		if (kind & (D_ERROR|D_OBJECT|D_MODULE)) {
			df->df_unit = new_unit();
			def_initlist(&df->mod_hincludes);
			def_initlist(&df->mod_cincludes);
		}
	}
	return df;
}

t_def *
define(id, scope, kind)
	t_idf	*id;
	t_scope	*scope;
	int	kind;
{
	/*	Declare an identifier in a scope, but first check if it
		already has been defined.
		If so, then check for the cases in which this is legal,
		and otherwise give an error message.
	*/
	t_def	*df;

	if (kind == D_ERROR) {
		return mk_def(id, scope, kind);
	}
	df = lookup(id, scope, D_IMPORT);
	if (	/* Already in this scope */
		df
	   ) {
		switch(df->df_kind) {
		case D_INUSE:
			if (kind != D_INUSE) {
				error("identifier \"%s\" already used; may not be redefined in this scope", df->df_idf->id_text);
				return mk_def(id, scope, D_ERROR);
			}
			return df;

		case D_OPAQUE:
			/* An opaque type. We may now have found the
			   definition of this type.
			*/
			if (kind == D_TYPE && !Specification
			    && scope == df->df_scope) {
				df->df_kind = D_TYPE;
				return df;
			}
			break;

		case D_TYPE:
			if (kind == D_TYPE &&
			    (! df->df_type || df->df_type == error_type)) {
				return df;
			}
			break;

		case D_ERROR:
			/* A definition generated by the compiler, because
			   it found an error. Maybe, the user gives a
			   definition after all.
			*/
			if (! (kind & (D_ERROR|D_PROCESS|D_OPERATION|D_FUNCTION|D_MODULE|D_OBJECT))) {
				free_dfproc(df->df_proc);
				df->df_proc = 0;
				free_body(df->df_body);
				df->df_body = 0;
			}
			if (kind & (D_TYPE|D_CONST|D_OPAQUE)) {
				df->df_flags |= D_DEFINED;
			}
			df->df_kind = kind;
			return df;
		}

		if (kind != D_ERROR) {
			/* Avoid spurious error messages
			*/
			error("identifier \"%s\" already declared",
			      id->id_text);
			df = mk_def(id, scope, D_ERROR);
		}

		return df;
	}
	return mk_def(id, scope, kind);
}

void
end_definition_list(pdf)
	t_def	**pdf;
{
	/*	Remove all imports from a definition module. This is
		neccesary because the implementation module might import
		them again.
	*/
	t_def	*df,
		*df1 = 0;

	while ((df = *pdf)) {
		if ((df->df_kind & D_IMPORTED)
		    && ! (df->df_flags & D_INSTANTIATION)) {
			if (! (df->df_flags & D_AUTOIMPORT) &&
			    ! (df->imp_def->df_flags & D_USED)) {
				pos_warning(&df->df_position, "identifier \"%s\" imported but not used", df->df_idf->id_text);
			}
			remove_from_id_list(df);
			*pdf = df->df_nextinscope;
			free_def(df);
		}
		else {
			df1 = df;
			switch(df->df_kind) {
			case D_TYPE:
			case D_CONST:
			case D_OBJECT:
			case D_FUNCTION:
				if (! (df->df_flags & D_GENERICPAR)) {
					df->df_flags |= D_USED;
				}
				if (df->df_kind == D_FUNCTION) {
					df->df_flags |= D_SPECSEEN;
				}
				break;
			case D_PROCESS:
			case D_OPERATION:
				df->df_flags |= D_USED|D_SPECSEEN;
				break;
			default:
				df->df_flags |= D_USED;
				break;
			}
			pdf = &(df->df_nextinscope);
		}
	}
	if (df1) df1->df_scope->sc_end = df1;
}

void
remove_from_id_list(df)
	t_def	*df;
{
	/*	Remove definition "df" from the definition list
	*/
	t_idf	*id = df->df_idf;
	t_def	*df1;

	if ((df1 = id->id_def) == df) id->id_def = df->df_next;
	else {
		while (df1->df_next != df) {
			assert(df1->df_next != 0);
			df1 = df1->df_next;
		}
		df1->df_next = df->df_next;
	}
}

static void
add_self(moddef)
	t_def	*moddef;
{
	/*	From within operations, we can refer to the object on which
		the operation is performed through the SELF identifier.
		The "add_self" function adds it to the symbol table.
	*/
	t_idf	*id;
	t_def	*df;

	if (! options['k']) {
		id = str2idf("SELF", 0);
		df = define(id, CurrentScope, D_VARIABLE);
		df->df_type = moddef->df_type;
		df->df_flags |= D_SELF|D_SHAREDPAR;
		df->df_name = "v__obj";
	}
	if (options['k'] || options['K']) {
		id = str2idf("self", 0);
		df = define(id, CurrentScope, D_VARIABLE);
		df->df_type = moddef->df_type;
		df->df_flags |= D_SELF|D_SHAREDPAR;
		df->df_name = "v__obj";
	}
}

t_def *
start_proc(kind, id, in_impl)
	int	kind;
	t_idf	*id;
	int	in_impl;
{
	/*	A function or process is declared, either in a specification
		or implementation. Create a def structure for it
		(if neccessary).
	*/
	t_def	*df;
	t_scope	*sc = CurrentScope;

	assert(kind & (D_FUNCTION | D_PROCESS | D_OPERATION ));

	if (! in_impl) {
		/* In a specification part
		*/
		df = define(id, CurrentScope, kind);
		df->df_flags |= D_USED;
	}
	else {
		df = lookup(id, CurrentScope, D_IMPORTED);
		if (!df || df->df_kind != kind || (df->df_flags & D_DEFINED)) {
			df = define(id, CurrentScope, kind);
			/*
			if (df->df_kind == D_OPERATION) {
				error("operation not declared in object specification");
			}
			*/
		}
	}

	open_scope(OPENSCOPE);
	if (in_impl) {
		CurrentScope->sc_name = mk_str(sc->sc_name, "_", id->id_text, (char *) 0);
		CurrentScope->sc_genname =
		    (CurrDef->df_flags & D_GENERIC) ?
			mk_str("_concat(", sc->sc_name, ", _", id->id_text, ")", (char *) 0) :
			CurrentScope->sc_name;
		ProcScope = CurrentScope;
	}
	CurrentScope->sc_definedby = df;
	if (df->df_type != std_type) {
		if (df->bod_scope) {
			t_def *df1, *df2;
			if (df->bod_scope->sc_name) {
				free(df->bod_scope->sc_name);
				if (df->bod_scope->sc_genname &&
				    df->bod_scope->sc_genname != df->bod_scope->sc_name) {
					free(df->bod_scope->sc_genname);
				}
			}
			df1 = df->bod_scope->sc_def;
			while (df1) {
				df2 = df1->df_nextinscope;
				if (df1->df_kind != D_VARIABLE) {
					remove_from_id_list(df1);
					free_def(df1);
				}
				df1 = df2;
			}
			free_scope(df->bod_scope);
		}
		df->bod_scope = CurrentScope;
	}

	/* Check for OrcaMain. */
	if (kind == D_PROCESS && ! strcmp(id->id_text, "OrcaMain")) {
		t_def	*defdf = enclosing(CurrentScope)->sc_definedby;

		if (defdf->df_flags & D_GENERIC) {
			error("OrcaMain may not be defined in a generic unit");
		}
		df->df_flags |= D_MAIN|D_USED;
		defdf->df_flags |= D_MAIN;
	}
	return df;
}

void
end_proc(df, body_seen)
	t_def	*df;
	int	body_seen;
{
	/*	The end of a function, process, or operation declaration.
		Close the scope and check that a value returning
		function or operation has at least one RETURN statement.
	*/

	if (! body_seen) {
		if (df->df_flags & D_SPECSEEN) {
			warning("\"%s\" specified more than once",
				df->df_idf->id_text);
		}
		df->df_flags |= D_SPECSEEN;
		close_scope();
		return;
	}
	chk_usage();
	close_scope();
	df->df_flags |= D_DEFINED;
	if (! (df->df_flags & D_RETURNEXPR) &&
	    result_type_of(df->df_type) != 0) {
		error("\"%s\" should return a value, but does not",
		      df->df_idf->id_text);
	}
	if (body_seen && df->df_kind == D_OPERATION &&
	    (df->df_flags & D_PARALLEL) && (df->df_flags & D_HAS_DEPENDENCIES) &&
	    !(df->df_flags & D_HAS_COMPLEX_OFLDSEL)) {
		warning("parallel operation \"%s\" has user-defined dependencies; compiler-generated dependencies ignored", df->df_idf->id_text);
	}
}

void
check_with_earlier_defs(df, params, resulttp)
	t_def	*df;
	t_dflst	params;
	t_type	*resulttp;
{
	/*	Check the header of a function, process, or operation
		declaration against a possible earlier specification
		in the module specification.
	*/
	t_dflst	pr;
	int	seen = 0;
	t_type	*prctp = proc_type(resulttp, params);

	if (df->df_type && df->df_type != error_type) {
		/* We already saw a definition of this type
		   in the definition module.
		*/
		if (!tst_proc_equiv(prctp, df->df_type)) {
			error("inconsistent declaration for \"%s\"",
			      df->df_idf->id_text);
		}
		pr = df->df_type->prc_params;
		df->df_type->prc_params = params;
		prctp->prc_params = pr;
		remove_type(prctp);
		seen = 1;
	}
	else	df->df_type = prctp;
	if (df->df_kind == D_OPERATION) {
		df->df_type->prc_objtype = df->df_scope->sc_definedby->df_type;
	}
	df->df_type->tp_def = df;

	if (df->df_flags & D_MAIN) {
		if (! def_emptylist(df->df_type->prc_params)) {
			error("OrcaMain should not have parameters");
		}
	}

	if (! seen && df->df_kind == D_FUNCTION) {
		t_def	*d;

		def_walklist(df->df_type->prc_params, pr, d) {
			if (is_shared_param(d) &&
			    d->df_type->tp_fund == T_OBJECT) {
				if (df->df_flags & D_HAS_SHARED_OBJ_PARAM) {
					df->df_flags &= ~D_HAS_SHARED_OBJ_PARAM;
					break;
				}
				df->df_flags |= D_HAS_SHARED_OBJ_PARAM;
			}
		}
	}
}

void
check_parallel_operation_spec(df, indexids, params, resulttp, resultreduce)
	t_def	*df;
	t_idlst	indexids;
	t_dflst	params;
	t_type	*resulttp;
	t_def	*resultreduce;
{
	int	cnt = 0;
	t_idlst	l;
	t_idf	*id;
	t_dflst	dl;
	t_def	*d;

	if (! (CurrDef->df_flags & D_PARTITIONED)) {
		error("parallel operations are only allowed in partitioned objects");
	}
	df->df_flags |= D_PARALLEL;
	df->opr_reducef = resultreduce;
	if (resulttp != 0) {
		df->df_flags |= (resultreduce != 0) ? D_REDUCED: D_GATHERED;
	}
	check_with_earlier_defs(df, params, resulttp);
	idf_walklist(indexids, l, id) {
		d = define(id, CurrentScope, D_VARIABLE);
		d->df_type = cnt >= CurrDef->df_type->arr_ndim ?
			error_type : CurrDef->df_type->arr_index(cnt);
		d->df_flags |= D_PART_INDEX|D_DEFINED|D_USED;
		d->var_level = cnt;
		cnt++;
	}
	if (CurrDef->df_type->arr_ndim &&
	    cnt != CurrDef->df_type->arr_ndim) {
		error("too %s partition indeces specified",
			cnt < CurrDef->df_type->arr_ndim ? "few" : "many");
	}
	def_walklist(params, dl, d) {
		if (is_out_param(d)) {
			df->df_flags |= d->df_flags & (D_GATHERED|D_REDUCED);
		}
	}
}

t_def *
lookup(id, scope, import)
	t_idf	*id;
	t_scope	*scope;
	int	import;
{
	/*	Look up a definition of an identifier in scope "scope".
		Make the "def" list self-organizing.
		Return a pointer to its "def" structure if it exists,
		otherwise return 0.
	*/
	t_def	*df,
		*df1;

	/* Look in the chain of definitions of this "id" for one with scope
	   "scope".
	*/
	for (df = id->id_def, df1 = 0;
	     df && df->df_scope != scope;
	     df1 = df, df = df->df_next) { assert(df->df_idf == id); }

	if (! df && import && scopeclosed(scope)) {
		for (df = id->id_def, df1 = 0;
		     df && df->df_scope != PervasiveScope;
		     df1 = df, df = df->df_next) { assert(df->df_idf == id); }
	}

	if (df) {
		/* Found it
		*/
		if (df1) {
			/* Put the definition in front
			*/
			assert(df1->df_idf == id);
			assert(df->df_idf == id);
			df1->df_next = df->df_next;
			df->df_next = id->id_def;
			id->id_def = df;
		}
		if (df->df_kind & import) {
		    int auto_import = df->df_flags & D_AUTOIMPORT;

		    while (df->df_kind & import) {
			assert(df->imp_def != 0);
			df->df_flags |= D_USED;
			df = df->imp_def;
			if (df->df_kind == D_ENUM && auto_import) {
				/* When an enumeration literal is used through
				   an imported definition, it could be that
				   it was imported only because its type
				   was imported. In this case, mark the type
				   as "used".
				*/
				df1 = df->df_type->tp_def;
				if (df1 && df1->df_idf) {
					df1 = lookup(df1->df_idf, scope, 0);
					if (df1 && df1->df_kind == D_IMPORT) {
						df1->df_flags |= D_USED;
					}
				}
			}
		    }
		}
	}
	return df;
}

t_def *
lookfor(id, vis, message)
	t_idf	*id;
	t_scope	*vis;
	int	message;
{
	/*	Look for an identifier in the visibility range started by "vis".
		If it is not defined create a dummy definition and,
		if message is set, give an error message.
	*/
	t_scope	*sc = vis;
	t_def	*df;

	while (sc) {
		df = lookup(id, sc, D_IMPORTED);
		if (df) {
			if (message
			    && sc != vis
			    && vis->sc_scopeclosed == 0) {
				t_def *d = define( id, vis, D_INUSE);
				if (d->df_kind != D_ERROR) {
					d->imp_def = df;
				}
			}
			return df;
		}
		sc = enclosing(sc);
	}

	if (message) id_not_declared(id);

	df = mk_def(id, vis, D_ERROR);
	df->df_flags |= D_USED;
	return df;
}

p_node
id2defnode(id)
	t_idf	*id;
{
	p_node	nd = mk_leaf(Def, IDENT);

	nd->nd_def = lookfor(id, CurrentScope, 1);
	nd->nd_type = nd->nd_def->df_type;
	return nd;
}


static char *
df_kind_of(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_VARIABLE:
		if (df->df_flags & D_INPAR) return "IN parameter";
		if (df->df_flags & D_OUTPAR) return "OUT parameter";
		if (df->df_flags & D_SHAREDPAR) return "SHARED parameter";
		return "variable";
	case D_CONST:
		if (df->df_flags & D_GENERICPAR) return "generic constant parameter";
		return "constant";
	case D_TYPE:
		if (df->df_flags & D_GENERICPAR) return "generic type parameter";
		return "type";
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) return "generic function parameter";
		return "function";
	case D_MODULE:
		return "module";
	case D_OBJECT:
		return "object";
	case D_PROCESS:
		return "process";
	}
	return "identifier";
}

static void
df_warning(pos, df, warn)
	t_pos	*pos;
	t_def	*df;
	char	*warn;
{
	if (is_anon_idf(df->df_idf)) {
		return;
	}
	if (! (df->df_kind & (D_MODULE|D_OBJECT|D_TYPE|D_CONST|D_VARIABLE|D_FUNCTION|D_PROCESS))) {
		return;
	}
	pos_warning(pos, "%s \"%s\" %s",
		df_kind_of(df),
		df->df_idf->id_text,
		warn);
}

void
chk_usage()
{
	t_def	*df = CurrentScope->sc_def;

	while (df) {
		if (df->df_kind == D_IMPORT) {
			t_def *df1 = df->imp_def;
			if (! (df->df_flags & D_USED)) {
				df_warning(&df->df_position, df1, "imported but never used");
			}
		}
		else if (df->df_kind
			 & (D_TYPE|D_CONST|D_VARIABLE|D_FUNCTION|D_PROCESS)) {
			switch(df->df_flags & (D_USED|D_DEFINED|D_INPAR|D_SHAREDPAR)) {
			case D_USED:
				if (df->df_kind == D_VARIABLE) {
					df_warning(&df->df_position, df, "never assigned");
				}
				break;
			case D_DEFINED:
			case D_INPAR|D_DEFINED:
			case D_INPAR:
				df_warning(&df->df_position, df, "never used");
				break;
			case 0:
				df_warning(&df->df_position, df, "never used/assigned");
				break;
			}
		}
		df = df->df_nextinscope;
	}
#ifdef DEBUG
	df = CurrentScope->sc_def;

	while (df) {
		if (df->df_kind == D_VARIABLE && (df->df_flags & D_SHAREDOBJ)) {
			debug("%s contains shared object", df->df_idf->id_text);
		}
		df = df->df_nextinscope;
	}
#endif
}

static void
add_read_write(objdef)
	t_def	*objdef;
{
	/*	Add dummy operations READ_ and WRITE_ to handle
		getting the value of an object, or writing it, for instance
		when the programmer does a := b, with a and b objects.
	*/
	add_opn(objdef, "READ_", D_OUTPAR);
	add_opn(objdef, "WRITE_", D_INPAR);
}

static void
add_opn(objdef, name, partype)
	t_def	*objdef;
	char	*name;
	int	partype;
{
	t_idf	*id = str2idf(name, 0);
	t_def	*d = define(id, CurrentScope, D_OPERATION);
	t_def	*param;
	t_dflst	paramlist;

	d->df_flags |= D_DEFINED|D_BLOCKINGDONE|D_NONBLOCKING|D_RWDONE;
	if (partype == D_INPAR) d->df_flags |= D_HASWRITES;
	else d->df_flags |= D_HASREADS;
	def_initlist(&paramlist);
	param = new_def();
	param->df_flags |= partype;
	param->df_type = objdef->df_type;
	param->df_name = "v";
	def_enlist(&paramlist, param);
	def_endlist(&paramlist);
	d->df_type = proc_type((t_type *) 0, paramlist);
	d->df_type->prc_objtype = objdef->df_type;
	d->bod_scope = new_scope();	/* to prevent further problems. */
	d->bod_scope->sc_definedby = d;
}

void
add_to_stringtab(s)
	t_string
		*s;
{
	/* Add string to string table, but only if it must
	   be generated for this module/object.
	*/

	t_scope	*sc = CurrentScope;

	while (sc && ! sc->sc_definedby) sc = enclosing(sc);
	if (CurrDef &&
	    sc &&
	    ((sc->sc_definedby->df_kind & (D_OPERATION|D_FUNCTION|D_PROCESS)) ||
	     sc->sc_definedby == CurrDef)) {
		s->s_next = CurrDef->mod_stringlist;
		CurrDef->mod_stringlist = s;
	}
}

t_def *
declare_type(id, tp, allow_opaque)
	t_idf	*id;
	t_type	*tp;
	int	allow_opaque;
{
	/*	A type with type-description "tp" is declared and must
		be bound to definition "df".
		This routine also handles the case that the type-field of
		"df" is already bound. In that case, it is either an opaque
		type, or an error message was given when "df" was created.
	*/
	t_def	*df = define(id, CurrentScope, tp == NULL ? D_OPAQUE : D_TYPE);
	t_type	*df_tp = df->df_type;

	if (! tp) {
		if (! allow_opaque) {
			error("opaque type only allowed in module/object specification");
		}
		tp = nodename_type(nil_type);
	}
	if (!tp->tp_def) tp->tp_def = df;
	if (! allow_opaque && df_tp && is_opaque_type(df_tp)) {
		if (tp->tp_fund != T_NODENAME) {
			error("opaque type \"%s\" is not a nodename type",
				df->df_idf->id_text);
		}
		df_tp->tp_equal = tp;
		if (! is_opaque_type(tp)) {
			return df;
		}
		while (tp && tp != df_tp && is_opaque_type(tp)) {
			tp = tp->tp_equal;
		}
		if (tp == df_tp) {
			/* Circular definition! */
			error("opaque type \"%s\" has a circular definition",
				 df->df_idf->id_text);
		}
		else if (tp) df_tp->tp_equal = tp;
	}
	else	df->df_type = tp;
	return df;
}

t_def *
declare_const(id, cst)
	t_idf	*id;
	p_node	cst;
{
	t_def	*df = define(id, CurrentScope, D_CONST);

	df->con_const = cst;
	df->df_type = cst->nd_type;
	return df;
}

#ifdef DEBUG
int
dump_def(df)
	t_def	*df;
{
	char	*s;

	switch(df->df_kind) {
	case D_MODULE:
		s = "module";
		break;
	case D_FUNCTION:
		s = "function";
		break;
	case D_VARIABLE:
		s = "variable";
		break;
	case D_FIELD:
		s = "record-field";
		break;
	case D_TYPE:
		s = "type";
		break;
	case D_ENUM:
		s = "enumeration-literal";
		break;
	case D_CONST:
		s = "constant";
		break;
	case D_IMPORT:
		s = "import";
		break;
	case D_OPAQUE:
		s = "opaque-type";
		break;
	case D_PROCESS:
		s = "process";
		break;
	case D_OBJECT:
		s = "object-type";
		break;
	case D_OPERATION:
		s = "operation";
		break;
	case D_UFIELD:
		s = "union-field";
		break;
	case D_OFIELD:
		s = "object-field";
		break;
	case D_INUSE:
		s = "use";
		break;
	case D_ERROR:
		s = "error";
		break;
	default:
		s = "unknown";
		break;
	}
	printf("(0x%lx) %s: %s\n", (long) df, df->df_idf->id_text, s);
	return 0;
}
#endif /* DEBUG */
