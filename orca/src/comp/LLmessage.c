/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S Y N T A X	 E R R O R   R E P O R T I N G */

/* $Id: LLmessage.c,v 1.8 1997/05/15 12:01:31 ceriel Exp $ */

/*	Defines the LLmessage routine. LLgen-generated parsers require the
	existence of a routine of that name.
	The routine must do syntax-error reporting and must be able to
	insert tokens in the token stream.
*/

#include	<alloc.h>
#include	<flt_arith.h>

#include	"ansi.h"
#include	"LLlex.h"
#include	"error.h"
#include	"misc.h"

#if ! defined(__STDC__) || __STDC__ == 0
LLmessage(tk)
	int	tk;
#else
void
LLmessage(
	int tk
	)
#endif
{

	if (tk > 0)	{
		/* if (tk > 0), it represents the token to be inserted.
		*/
		t_token *dotp = &dot;

#ifndef LLNONCORR
		error("%s missing", symbol2str(tk));
#endif

		aside = *dotp;

		dotp->tk_symb = tk;

		switch (tk)	{
		/* The operands need some body */
		case IDENT:
			dotp->tk_idf = gen_anon_idf();
			break;
		case STRING:
			dotp->tk_string = (p_string)
				Malloc(sizeof (t_string));
			dotp->tk_string->s_length = 1;
			dotp->tk_string->s_str = Salloc("", 1);
			break;
		case INTEGER:
			dotp->tk_int = 1;
			break;
		case REAL:
			dotp->tk_real = (p_real) Malloc(sizeof(t_real));
			dotp->tk_real->r_real = Salloc("0.0", 4);
			flt_str2flt(dotp->tk_real->r_real, &dotp->tk_real->r_val);
			break;
		}
	}
	else if (tk  < 0) {
		error("end of file expected");
	}
	else {
#ifndef LLNONCORR
		error("%s deleted", symbol2str(dot.tk_symb));
#else
		error("%s not expected", symbol2str(dot.tk_symb));
#endif
	}
	tk_nmb_at_last_syn_err = token_nmb;
}
