/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __GEN_CODE_H__
#define __GEN_CODE_H__

/* $Id: gen_code.h,v 1.18 1997/05/15 12:02:08 ceriel Exp $ */

#include	<stdio.h>
#include	"ansi.h"
#include	"def.h"
#include	"type.h"

/* Code generation for function/operation/process bodies and statements. */

_PROTOTYPE(void gen_data, (p_def df));
	/*	Generates the data variable indicated by 'df'.
	*/

_PROTOTYPE(void gen_func, (p_def df));
	/*	Generates the body for a function/process/operation 'df'.
	*/

_PROTOTYPE(void gen_proto, (p_def df));
	/*	Generates prototypes for the function/operation/object 'df'.
	*/

_PROTOTYPE(void gen_operation_wrappers, (p_def df));
	/*	Generates the wrappers around an operation 'df'.
	*/

_PROTOTYPE(char *gen_process_wrapper, (p_def df));
	/*	Generates a wrapper around a process 'df'.
		Returns the name of the wrapper function.
	*/

_PROTOTYPE(void gen_prototype, (FILE *f, p_def funcdf, char *nm, p_type ftp));
	/*      Generate a prototype for the function/operation indicated by
                'funcdf', on file descriptor 'f'.
                This function can be used for declaration as well as
                definition, because no terminating ; is produced.
                The name to be used for the function is indicated by 'nm'.
                if 'ftp' is set, a function type is declared instead.
        */

_PROTOTYPE(int could_be_called_from_operation, (p_def df));
	/*	Check if 'df' could be called from within an operation.
	*/

_PROTOTYPE(void gen_deps_func, (p_def df));
	/*	Produce code for dependencies of a parallel operation, either
		user defined or PDG.
	*/

/*
_PROTOTYPE(void gen_expr_ld, (FILE *, p_node));
_PROTOTYPE(void gen_expr_st, (FILE *, p_node));
_PROTOTYPE(void gen_expr_stld, (FILE *, p_node));
_PROTOTYPE(void gen_addr_ld, (FILE *, p_node));
_PROTOTYPE(void gen_addr_st, (FILE *, p_node));
_PROTOTYPE(void gen_addr_stld, (FILE *, p_node));
*/

#define	gen_expr_ld	gen_expr
#define	gen_expr_st	gen_expr
#define	gen_expr_stld	gen_expr
#define gen_addr_ld	gen_addr
#define gen_addr_st	gen_addr
#define gen_addr_stld	gen_addr

extern int
	indentlevel;
extern int
	clone_local_obj;

#define indent(f)	fprintf(f, "%*s", indentlevel, "    ")

#endif /* __GEN_CODE_H__ */
