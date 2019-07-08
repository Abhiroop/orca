/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* U S E R   O P T I O N - H A N D L I N G */

/* $Id: options.c,v 1.28 1997/07/10 12:34:15 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

#include	<alloc.h>
#include	<assert.h>
#include	<stdio.h>

#include	"options.h"
#include	"main.h"
#include	"class.h"
#include	"error.h"
#include	"const.h"

char		options[128];
int		dp_flag = 0;
char		*dep_filename;
static int	nDEF = 2,
		mDEF = 0,
		ndirs = 1;

_PROTOTYPE(static int txt2int, (char **));

void
DoOption(text)
	char	*text;
{
	assert(*text == '-');
	text++;
	switch(*text++)	{

	case '-':	/* debug options. */
			/* We have:
			 * -C: dump bodies.
			 * -S: dump symbol table.
			 * -P: dump closures.
			 * -d: (debug) messages.
			 * -o: dump global optimizer info.
			 * -x: no array trick (for statically sized arrays)
			 * -l: check lexical analyzer
			 */
		options[(int)(*text)]++;
		break;

	case 'k':	/* only keywords in lower case */
	case 'K':	/* keywords in lower & upper case */
	case 'w':	/* disable warnings */
	case 'm':	/* generate dependencies in makefile format */
	case 'N':	/* no code generation (actually on /dev/null) */
	case 'n':	/* unroll loops */
	case 'i':	/* generate prototype for ini_ function instead
			   of empty macro (usually when compiling specification)
			*/
	case 'L':	/* generate line directives */
	case 'u':	/* don't complain about undefined functions */
	case 'O':	/* optimize */
	case 'V':	/* optimize (LV) */
	case 'c':	/* no checks */
	case 'f':	/* cloning */
	case 'a':	/* no age fields ... */
	case 'p':	/* no polling (no m_rts() calls) */
		options[(int)(text[-1])]++;
		break;

	case 'D':
		str2idf(text, 0)->id_mac = 1;
		break;
	case 'U': 
		str2idf(text, 0)->id_mac = 0;
		break;
	case 'W': {	/* set word-size */
		char	*t = text;	/* because &text is illegal */
		int	sz;

		sz = txt2int(&t);
		if (*t || sz <= 0)
			fatal("malformed -W option");
		set_wordsize(sz);
		}
		break;

	case 'A':	/* generate dependencies */
		options['A'] = 1;
		if (*text) {
			dep_filename = text;
		}
		break;

	case 'I':
		{
			int i;
			char *new = text;

			if (! *new) new = 0;

			if (++nDEF > mDEF) {
				DEFPATH = (char **)
				   Realloc((char *)DEFPATH,
					   (unsigned)(mDEF+=10)*sizeof(char *));
			}

			for (i = ndirs++; i < nDEF; i++) {
				char *tmp = DEFPATH[i];

				DEFPATH[i] = new;
				new = tmp;
			}
		}
		break;
	case 'd':	/* data-parallel flag */
		dp_flag = 1;
		break;
	}
}

static int
txt2int(tp)
	char	**tp;
{
	/*	the integer pointed to by *tp is read, while increasing
		*tp; the resulting value is yielded.
	*/
	int	val = 0;
	int	ch;

	while (ch = **tp, ch >= '0' && ch <= '9')	{
		val = val * 10 + ch - '0';
		(*tp)++;
	}
	return val;
}
