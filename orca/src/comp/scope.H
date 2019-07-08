/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __SCOPE_H__
#define __SCOPE_H__

/* S C O P E   M E C H A N I S M */

/* $Id: scope.H,v 1.9 1997/05/15 12:02:54 ceriel Exp $ */

#include	"ansi.h"

typedef struct scope t_scope, *p_scope;

#define OPENSCOPE	0	/* Indicating an open scope */
#define CLOSEDSCOPE	1	/* Indicating a closed scope (module) */

/* A "scope" structure exists for each scope in the compilation unit.
   The fields are:
   sc_name
   sc_genname	both used for code generation. They usually indicate the
		name of the scope. When names are to be produced dynamically
		(generics), sc_genname has a more complex form, involving
		the _concat macro.
   sc_scopeclosed
		flag, set when the scope is "closed". This affects the
		behaviour of lookup routines: in a closed scope, identifiers
		in enclosing scopes are not visible, in an open (non-closed)
		scope, they are.
   sc_def	header of the list of definitions in this scope, linked through
		the df_nextinscope field of the "def" structures.
   sc_end	indicates the end of the list of definitions in this scope. It
		is maintained so that we don't have to look for the end of the
		list all the time. Alternatively, the list could be circular.
   sc_definedby	"def" structure for the identifier defining this scope. This
		could be an object, module, function, operation, or process.
		It may also be NULL, for instance for scopes introduced
		by FOR-statements.
   sc_encl	pointer to the (statically) enclosing scope.
   sc_FOR	list of scopes in which FOR-loop variables live.
   sc_anoncount	counter used for the generation of type names for anonymous
		types. We want to do this per scope, because different
		compilations reading the same specification must use the
		same names for the same anonymous types of this specification.
*/

struct scope {
    char	*sc_name;
    char	*sc_genname;
    int		sc_scopeclosed;
    struct def	*sc_def,
    		*sc_end,
    		*sc_definedby;
    p_scope	sc_encl;
    p_scope	sc_FOR;
    int		sc_anoncount;
};

/* ALLOCDEF "scope" 10 */

extern p_scope
	CurrentScope,	/* Last opened and not yet closed scope. */
	GlobalScope,	/* Global scope (in which modules and objects are
			   found.
			*/
	PervasiveScope,	/* Scope of predefined identifiers, visible anywhere. */
	ProcScope;	/* Scope of the current
			   process/object/function/operation.
			*/

#define enclosing(x)	((x)->sc_encl)
#define scopeclosed(x)	((x)->sc_scopeclosed)

_PROTOTYPE(void open_scope, (int kind));
	/*	Opens a scope that is either open (automatic imports) or closed,
		depending on 'kind', which should either be OPENSCOPE or
		CLOSEDSCOPE.
	*/

_PROTOTYPE(p_scope open_and_close_scope, (int kind));
	/*	Opens a scope that is either open (automatic imports) or closed,
		depending on 'kind', which should either be OPENSCOPE or
		CLOSEDSCOPE. The scope is immediately closed again. A pointer
		to the resulting scope struct is returned.
	*/

_PROTOTYPE(void init_scope, (void));
	/*	Initializes the global variables.
	*/

_PROTOTYPE(void chk_procs, (void));
        /*      To be called at scope closing. Check all definitions, and if one
		is either D_FUNCTION, D_PROCESS, or D_OPERATION, check that it
		is defined.
	*/

_PROTOTYPE(void chk_forwards, (void));
        /*      Look for all forward definitions (resulting from NODENAME
                type declarations).  If it still is a D_TYPE with df_type 0
                it is actually either imported from the enclosing scope(s)
                or not declared.
        */

_PROTOTYPE(void close_scope, (void));
	/*	Closes the current scope. The current scope becomes the
		enclosing one.
	*/

#ifdef DEBUG
_PROTOTYPE(int dump_scope, (p_scope sc));
	/*	Produce a somewhat readable list of definitions in this scope.
	*/
#endif

#endif /* __SCOPE_H__ */
