/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __TOKENNAME_H__
#define __TOKENNAME_H__

/* T O K E N N A M E   S T R U C T U R E */

/* $Id: tokenname.h,v 1.8 1997/07/02 14:12:14 ceriel Exp $ */

#include	"ansi.h"

/* For each lexical token that is not a single ascii character, a tokenname
   structure exists. It has the following fields:
   tn_symbol	the number of the token, as allocated by the parser generator.
   tn_name	the token as a string. This is used for error reporting as well
		as the initialization of the symbol table for keywords.
   tn_data_parallel
		flag, set to non-zero if the reserved word indicated by this
		tokenname struct is only reserved when using the data-parallel
		Orca extension.
*/

struct tokenname {
    int		tn_symbol;
    char	*tn_name;
    int		tn_data_parallel;
};

_PROTOTYPE(void reserve, (struct tokenname *resv));
	/*	The names of the tokens in the tokenname list 'resv' are
		entered as reserved words.
	*/

_PROTOTYPE(void add_conditionals, (void));
	/*	The names of the conditional compilation commands are
		added to the symbol table.
	*/

extern struct tokenname tkidf[], tkidf_lc[];
#endif /* __TOKENNAME_H__ */
