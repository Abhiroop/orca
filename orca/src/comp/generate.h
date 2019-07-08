/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __GENERATE_H__
#define __GENERATE_H__

/* $Id: generate.h,v 1.16 1997/05/15 12:02:17 ceriel Exp $ */

#include	<stdio.h>
#include	"ansi.h"
#include	"LLlex.h"
#include	"def.h"
#include	"node.h"
#include	"type.h"

_PROTOTYPE(void walkdefs, (p_def df, int kinds, void (*fnc)(p_def)));
	/*	Walk the definition list indicated by 'df', and apply
		'fnc' to each definition with df_kind in the bitset
		indicated by 'kinds'.
	*/

_PROTOTYPE(char *gen_name, (char *pref, char *id_str, int fix));
        /*      Generates a name using prefix "pref" and the string "id_str".
                If in a generic module or object, and "fix" is not set,
                leave this to the preprocessor, because in this case the
                name of the scope is a macro. Otherwise, we use the following
                scheme: <pref><scopename>__<id_str>. <pref> should probably
                end with an underscore.
        */

_PROTOTYPE(void gen_instantiation, (p_def instdef));
        /*      Instantiation is done by defining some macros and then
                including. If the instantiation is in a specification
                module, it ends up in both the generated include file and
                the generated C file.
        */

_PROTOTYPE(void assign_name, (p_def df));
        /*      Assign a name for code generation to fields, variables, etc.
        */

_PROTOTYPE(void gen_stringtab, (p_def df));
	/*	Produce array structures for all anonymous strings in the
		current compilation unit.
	*/

_PROTOTYPE(void gen_const, (p_def df));
	/*	If 'df' is a function, operation or process, generate C-code
		for the structured constants within 'df', if 'df' indicates
		a structured constant, generate code for it.
		A structured constant is either an aggregate or a string.
	*/

_PROTOTYPE(void gen_modinit, (p_def df));
	/*	Generate initialization routines for the module/object
		indicated by 'df'.
	*/

_PROTOTYPE(void gen_init_macro, (p_type tp, FILE *f));
        /*      Produce an initialization macro for type 'tp', on file 'f'.
                Unions and arrays have two initialization macros because
                they can have bounds and tag expressions.
        */

_PROTOTYPE(void gen_init_with_bat, (char *dfnam, p_type tp, FILE *f, p_node bat,
				    char *trcnam, char *nl));
	/*	Produce an initialization with a bounds-and-tag expression.
		'dfnam' is the name of the variable being initialized,
		'tp' is its type, the initialization is to be produced on
		file 'f', 'bat' is the bounds-and-tag expression, trcnam is
		a name used for tracing, and 'nl' indicates the newline
		character sequence to be used here. Note that the initialization
		may be produced within a macro, for instance within the
		initialization macro of a record with a field containing a
		bounds-and-tag expression.
	*/

_PROTOTYPE(int base_char_of, (p_type tp));
	/*	Find the base character of type 'tp'. This character is used
		to produce names. The mapping is:

			n	NODENAME
			a	ARRAY
			s	SET
			b	BAG
			g	GRAPH
			u	UNION
			r	RECORD
			p	PARTITIONED OBJECT
			o	OBJECT
	*/

_PROTOTYPE(char *type_name, (p_type tp));
	/*      Finds the name of the type indicated by 'tp'.
		If it does not have a name, an anonymous one is produced.
	*/

_PROTOTYPE(char *gen_trace_name, (p_def df));
	/*	Generate a name for 'df', usable for tracing purposes.
	*/

_PROTOTYPE(void init_gen, (void));
	/*	To be called before code generation for each module/object.
	*/

#endif /* __GENERATE_H__ */
