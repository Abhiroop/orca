/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __LLLEX_H__
#define __LLLEX_H__

/* T O K E N   D E S C R I P T O R   D E F I N I T I O N */

/* $Id: LLlex.h,v 1.11 1997/07/02 14:12:03 ceriel Exp $ */

#include	<flt_arith.h>

#include	"ansi.h"
#include	"idf.h"
#include	"extra_tokens.h"

/* Structure to store a string literal.
*/
typedef struct string {
	unsigned int
		s_length;	/* Length of the string. */
	char	*s_str;		/* The string itself. */
	struct string
		*s_next;	/* Strings are linked in a per module/object
				   list.
				*/
	char	*s_name;	/* Name of the C variable generated for this
				   string.
				*/
	int	s_exported;	/* Set when the string resides in a
				   specification.
				*/
} t_string, *p_string;

/* Structure for floating point constants. There are two representations in
   this structure:
   r_real	represented as a string (this one does not always exist, so
		test for it before using it;
   r_val	of type "flt_arith", which is an extened precision represen-
		tation suitable for the flt_arith module;
*/
typedef struct real {
	char	*r_real;
	flt_arith
		r_val;
} t_real, *p_real;

/* A position consists of a filename and a line number.
*/
typedef struct position	{
	int	pos_lineno;
	char	*pos_filename;
} t_pos, *p_pos;

#define NULLPOS	((p_pos) 0)

typedef struct token	{
	int	tk_symb;	/* The token number as produced by LLgen. */
	t_pos	tk_pos;		/* Its position. */
	union {			/* The token attributes. */
	    p_idf
		tkk_idf;	/* tk_symb == IDENT, the idf structure. */
	    p_string
		tkk_str;	/* tk_symb == STRING, the string structure. */
	    long
		tkk_int;	/* tk_symb == INTEGER, the value. */
	    struct real
		*tkk_real;	/* tk_symb == REAL, the real structure. */
	} tk_data;
} t_token, *p_token;

/* Some macros to simplify access to token attributes: */
#define tk_string tk_data.tkk_str
#define tk_real tk_data.tkk_real
#define tk_idf	tk_data.tkk_idf
#define tk_int	tk_data.tkk_int

extern t_token	dot;		/* The current token. */
extern t_token	aside;		/* A token put aside during error recovery. */

#define DOT	dot.tk_symb	/* The current token number. */
#define ASIDE	aside.tk_symb	/* The token number of the token put aside. */

extern int	token_nmb;	/* current token count */
extern int	tk_nmb_at_last_syn_err;
				/* token count at last syntax error */

_PROTOTYPE(int LLlex, (void));
	/*	The lexical analyzer. Reads the next token and stores it in
		'dot'. The token number is returned.
	*/

_PROTOTYPE(void LLlexinit, (void));
	/*	Initializes some variables for the lexical analyzer.
		Should be called before parsing a compilation unit.
	*/

_PROTOTYPE(char *symbol2str, (int));
	/*	Not really declared in LLlex.c, but produced by make.tokcase.
		This routine maps a token number onto a string representation
		of it, suitable for use in error messages.
	*/
_PROTOTYPE(void UnloadChar, (int ch));
	/*	Pushes the character ch back onto the input stream.
	*/
#endif /* __LLLEX_H__ */
