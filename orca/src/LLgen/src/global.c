/* Copyright (c) 1991 by the Vrije Universiteit, Amsterdam, the Netherlands.
 * For full copyright and restrictions on use see the file COPYING in the top
 * level of the LLgen tree.
 */

/*
 *  L L G E N
 *
 *  An Extended LL(1) Parser Generator
 *
 *  Author : Ceriel J.H. Jacobs
 */

/*
 * global.c
 * Contains declarations visible in several other source files
 */

# include "types.h"
# include "extern.h"
# include "io.h"

# ifndef NORCSID
static string rcsid4 = "$Id: global.c,v 2.16 1997/02/21 11:27:51 ceriel Exp $";
# endif

char	ltext[LTEXTSZ];
p_nont	nonterms;
p_nont	maxnt;
int	nnonterms;
p_token	tokens;
p_token	maxt;
int	ntokens;
int	nterms, nalts;
int	norder, torder;
#ifdef NON_CORRECTING
int	nsubstarts;
p_set	start_firsts;
#endif
p_start	start;
int	linecount;
int	assval;
FILE	*fout;
FILE	*fpars;
FILE	*finput;
FILE	*fact;
char	f_pars[] = PARSERFILE;
char	f_temp[] = ACTFILE;
#ifdef NON_CORRECTING
char	f_nc[20];
#endif
char	f_out[20];
string	f_input;
char	f_include[20];
char	f_rec[20];
string	e_noopen = "Cannot open %s";
int	verbose;
int	wflag;
string	lexical;
string	prefix;
string	onerror;
int	ntneeded;
int	ntprint;
# ifndef NDEBUG
int	debug;
#endif /* not NDEBUG */
p_file	files;
p_file	maxfiles;
p_file	pfile;
string	LLgenid = "/* LLgen generated code from source %s */\n";
t_token lextoken;
int	nerrors;
string	rec_file, incl_file;
#ifdef NON_CORRECTING
string	nc_rec_file, nc_incl_file;
#endif
int	low_percentage = 10, high_percentage = 30;
int	min_cases_for_jmptable = 8;
int	jmptable_option;
int	ansi_c = 0;
#ifdef NON_CORRECTING
int	non_corr = 0;
int	subpars_sim = 0;
p_gram	illegal_gram;
#endif
int	strip_grammar = 0;
int	in_production;		/* set when the parser is reading a production
				   rule.
				*/
