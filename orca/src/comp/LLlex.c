/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* L E X I C A L   A N A L Y S E R   F O R   O R C A */

/* $Id: LLlex.c,v 1.19 1998/06/26 10:20:55 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"
#include	"idfsize.h"
#include	"numsize.h"

#include	<stdio.h>
#include	<alloc.h>
#include	<assert.h>
#include	<flt_arith.h>

#include	"LLlex.h"
#include	"input.h"
#include	"f_info.h"
#include	"class.h"
#include	"errout.h"
#include	"const.h"
#include	"main.h"
#include	"error.h"
#include	"options.h"
#include	"def.h"
#include	"flexarr.h"
#include	"specfile.h"
#include	"conditional.h"

t_token		dot,
		aside;
int		token_nmb;
int		tk_nmb_at_last_syn_err;

static int	eofseen;

_PROTOTYPE(static p_string GetString, (int));

void
LLlexinit()
{
	token_nmb = 0;
	tk_nmb_at_last_syn_err = -ERR_SHADOW;
	ASIDE = 0;
}

static p_string
GetString(upto)
	int	upto;
{
	/*	Read an Orca string, delimited by the character "upto".
	*/
	int	ch;
	p_string
		str = (p_string)
			Malloc((unsigned) sizeof(t_string));
	char	*p;
	unsigned int
		len;
	p_flex	f = flex_init(sizeof(char), 32);

	while (LoadChar(ch), ch != upto)	{
		if (ch == '\\') LoadChar(ch);  /* escape character */
		if (class(ch) == STNL)	{
			lexerror("newline in string");
			LineNumber++;
			break;
		}
		if (ch == EOI)	{
			lexerror("end-of-file in string");
			break;
		}
		p = flex_next(f);
		*p = ch;
	}
	p = flex_next(f);
	*p = '\0';
	str->s_str = flex_finish(f, &len);
	str->s_length = len - 1;
	str->s_name = 0;
	str->s_exported = Specification;
	add_to_stringtab(str);
	return str;
}

void
UnloadChar(ch)
	int	ch;
{
	if (ch == EOI) eofseen = 1;
	else PushBack();
}

int
LLlex()
{
	/*	LLlex() is the Lexical Analyzer.
		The putting aside of tokens is taken into account.
	*/
	p_token	tk = &dot;
	char	buf[(IDFSIZE > NUMSIZE ? IDFSIZE : NUMSIZE) + 2];
	int	ch, nch;

	if (ASIDE)	{	/* a token is put aside		*/
		*tk = aside;
		ASIDE = 0;
		return tk->tk_symb;
	}

	token_nmb++;

	if (LineNumber == 0) {
		goto firstline;
	}
again:
	if (eofseen) {
		eofseen = 0;
		ch = EOI;
	}
	else {
		LoadChar(ch);
		if ((ch & 0200) && ch != EOI) {
			error("non-ascii '\\%03o' read", ch & 0377);
			goto again;
		}
	}

	dot.tk_pos.pos_lineno = LineNumber;
	dot.tk_pos.pos_filename = FileName;

	switch (class(ch))	{

	case STNL:
firstline:
		LineNumber++;
		while (LoadChar(ch), ch == '.') {
			doconditional();
		}
		UnloadChar(ch);
		goto again;

	case STSKIP:
		goto again;

	case STSIMP:
		if (ch == '#')	{
			for (;;) {  /* comment: skip until end of line */
				LoadChar(nch);
				if (nch == EOI) {
					lexerror("unterminated comment");
					break;
				}
				if (class(nch) == STNL) {
					PushBack();
					break;
				}
			}
			goto again;
		}

	}

	if (in_include) {
		lexerror("an .include file may only contain preprocessing commands");
	}

	switch(class(ch)) {

	case STGARB:
		if ((unsigned) ch - 040 < 0137)	{
			lexerror("garbage char %c", ch);
		}
		else	lexerror("garbage char \\%03o", ch);
		goto again;

	case STSIMP:
		/* Comment is skipped above. */
		return tk->tk_symb = ch;

	case STCOMP:
		LoadChar(nch);
		switch (ch)	{

		case '$':
			if (dp_flag && nch == '$') {
				return tk->tk_symb = DOLDOL;
			}
			break;

		case '.':
			if (nch == '.')	{
				return tk->tk_symb = UPTO;
			}
			break;

		case '=':
			if (nch == '>') {
				return tk->tk_symb = ARROW;
			}
			break;

		case ':':
			if (nch == '=')	{
				return tk->tk_symb = BECOMES;
			}
			break;

		case '<':
			if (nch == '=')	{
				return tk->tk_symb = LESSEQUAL;
			}
			if (nch == '<')	{
				return tk->tk_symb = LEFTSHIFT;
			}
			break;

		case '>':
			if (nch == '=')	{
				return tk->tk_symb = GREATEREQUAL;
			}
			if (nch == '>')	{
				return tk->tk_symb = RIGHTSHIFT;
			}
			break;
		case '|':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = B_ORBECOMES;
				} else {
					lexerror("= expected after |:");
				}
			}
			break;
		case '&':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = B_ANDBECOMES;
				} else {
					lexerror("= expected after &:");
				}
			}
			break;
		case '^':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = B_XORBECOMES;
				} else {
					lexerror("= expected after ^:");
				}
			}
			break;
		case '+':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = PLUSBECOMES;
				} else {
					lexerror("= expected after +:");
				}
			}
			break;
		case '-':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = MINBECOMES;
				} else {
					lexerror("= expected after -:");
				}
			}
			break;
		case '*':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = TIMESBECOMES;
				} else {
					lexerror("= expected after *:");
				}
			}
			break;
		case '/':
			if (nch == '=') return tk->tk_symb = NOTEQUAL;
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = DIVBECOMES;
				} else {
					lexerror("= expected after /:");
				}
			}
			break;
		case '%':
			if (nch == ':') {
				LoadChar(nch);
				if (nch == '=') {
					return tk->tk_symb = MODBECOMES;
				} else {
					lexerror("= expected after %:");
				}
			}
			break;

		default :
			crash("(LLlex, STCOMP)");
		}
		UnloadChar(nch);
		return tk->tk_symb = ch;

	case STIDF:
	{
		char *tag = &buf[0];
		p_idf id;

		if (ch == '_') {
			lexerror("an identifier may not start with an underscore");
		}
		do	{
			if (tag - buf < IDFSIZE) *tag++ = ch;
			LoadChar(ch);
			if (ch == '_' && *(tag - 1) == '_') {
				lexerror("an identifier may not contain two consecutive underscores");
			}
		} while(in_idf(ch));

		UnloadChar(ch);
		*tag = '\0';
		if (*(tag - 1) == '_') {
			lexerror("last character of an identifier may not be an underscore");
		}

		tk->tk_idf = id = str2idf(buf, 1);
		return tk->tk_symb = id->id_reserved ? id->id_reserved : IDENT;
	}

	case STCHAR:
		LoadChar(ch);
		tk->tk_int = (int) ch & 0377;

		if (ch == EOI || (LoadChar(ch), ch != '\'')) {
			lexerror("\' missing");
			UnloadChar(ch);
		}
		return tk->tk_symb = CHARACTER;

	case STSTR: {
		p_string
			str = GetString(ch);

		tk->tk_string = str;
		return tk->tk_symb = STRING;
		}

	case STNUM:
	{
		/*	The problem arising with the "parsing" of a number
			is that we don't know the base in advance so we
			have to read the number with the help of a rather
			complex finite automaton.
		*/
		enum statetp {Oct,Hex,Dec,OctEndOrHex,End,OptReal,Real};
		enum statetp state;
		int base = 8;
		char *np = &buf[0];

		*np++ = ch;
		state = is_oct(ch) ? Oct : Dec;
		LoadChar(ch);
		for (;;) {
			switch(state) {
			case Oct:
				while (is_oct(ch))	{
					if (np < &buf[NUMSIZE]) *np++ = ch;
					LoadChar(ch);
				}
				if (ch == 'B') {
					state = OctEndOrHex;
					break;
				}
				/* Fall Through */
			case Dec:
				base = 10;
				while (is_dig(ch))	{
					if (np < &buf[NUMSIZE]) {
						*np++ = ch;
					}
					LoadChar(ch);
				}
				if (is_hex(ch)) state = Hex;
				else if (ch == '.') state = OptReal;
				else {
					state = End;
					if (ch == 'H') base = 16;
					else PushBack();
				}
				break;

			case Hex:
				while (is_hex(ch))	{
					if (np < &buf[NUMSIZE]) *np++ = ch;
					LoadChar(ch);
				}
				base = 16;
				state = End;
				if (ch != 'H') {
					lexerror("H expected after hex number");
					PushBack();
				}
				break;

			case OctEndOrHex:
				if (np < &buf[NUMSIZE]) *np++ = ch;
				LoadChar(ch);
				if (ch == 'H') {
					base = 16;
					state = End;
					break;
				}
				if (is_hex(ch)) {
					state = Hex;
					break;
				}
				PushBack();
				ch = *--np;
				*np++ = '\0';
				/* Fall through */

			case End: {
				int ovfl = 0;

				*np = '\0';
				if (np >= &buf[NUMSIZE]) {
					tk->tk_int = 1;
					lexerror("constant too long");
				}
				else {
					arith ubound = max_int[get_wordsize()]/base;
					np = &buf[0];
					while (*np == '0') np++;
					tk->tk_int = 0;
					while (*np) {
						int c;

						if (is_dig(*np)) {
							c = *np++ - '0';
						}
						else {
							assert(is_hex(*np));
							c = *np++ - 'A' + 10;
						}
						if (tk->tk_int < 0 ||
						    tk->tk_int > ubound) {
							ovfl = 1;
						}
						tk->tk_int = tk->tk_int*base;
						tk->tk_int += c;
						if (tk->tk_int < 0) ovfl = 1;
					}
				}
				if (ovfl && base == 10) {
					lexwarning("overflow in constant");
				}
				return tk->tk_symb = INTEGER;
				}

			case OptReal:
				/*	The '.' could be the first of the '..'
					token. At this point, we need a
					look-ahead of two characters.
				*/
				LoadChar(ch);
				if (ch == '.') {
					/*	Indeed the '..' token
					*/
					PushBack();
					PushBack();
					state = End;
					break;
				}
				state = Real;
				break;
			case Real:
				break;
			}
			if (state == Real) break;
		}

		/* a real real constant */
		if (np < &buf[NUMSIZE]) *np++ = '.';

		while (is_dig(ch)) {
			/*	Fractional part
			*/
			if (np < &buf[NUMSIZE]) *np++ = ch;
			LoadChar(ch);
		}

		if (ch == 'E' || ch == 'D') {
			/*	Scale factor. For the time being, accept 'D'
				too, for backwards compatibility.
			*/

			if (ch == 'D') {
				LoadChar(ch);
				if (!(ch == '+' || ch == '-' || is_dig(ch))) {
					goto noscale;
				}
			}
			else LoadChar(ch);
			if (np < &buf[NUMSIZE]) *np++ = 'E';
			if (ch == '+' || ch == '-') {
				/*	Signed scalefactor
				*/
				if (np < &buf[NUMSIZE]) *np++ = ch;
				LoadChar(ch);
			}
			if (is_dig(ch)) {
				do {
					if (np < &buf[NUMSIZE]) *np++ = ch;
					LoadChar(ch);
				} while (is_dig(ch));
			}
			else {
				lexerror("bad scale factor");
			}
		}

noscale:
		*np++ = '\0';
		UnloadChar(ch);

		tk->tk_real = (p_real) Malloc(sizeof(t_real));
		if (np >= &buf[NUMSIZE]) {
			tk->tk_real->r_real = Salloc("0.0", 4);
			lexerror("real constant too long");
		}
		else	tk->tk_real->r_real = Salloc(buf, (unsigned) (np - buf));
		flt_str2flt(tk->tk_real->r_real, &tk->tk_real->r_val);
		return tk->tk_symb = REAL;

		/*NOTREACHED*/
	}

	case STEOI:
		return tk->tk_symb = -1;

	default:
		crash("(LLlex) Impossible character class");
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
	return 0;
}
