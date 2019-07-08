/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */
 
/* C O N D I T I O N A L   C O M P I L A T I O N */
 
/* $Id: conditional.c,v 1.3 1998/06/26 10:20:55 ceriel Exp $ */
 
#include "ansi.h"

#include "idfsize.h"
#include "class.h"
#include "LLlex.h"
#include "error.h"
#include "conditional.h"
#include "input.h"
#include "flexarr.h"
#include "misc.h"
#include "main.h"

#define IFDEPTH	256
static char
	ifstack[IFDEPTH];

int	nestlevel = -1;
int	in_include;

_PROTOTYPE(static t_idf *getid, (void));
_PROTOTYPE(static char *getstr, (void));
_PROTOTYPE(static void push_if, (void));
_PROTOTYPE(static void do_define, (void));
_PROTOTYPE(static void do_undef, (void));
_PROTOTYPE(static void do_ifdef, (int how));
_PROTOTYPE(static void do_else, (void));
_PROTOTYPE(static void do_endif, (void));
_PROTOTYPE(static void do_include, (void));
_PROTOTYPE(static int SkipToNewLine, (void));
_PROTOTYPE(static void skip_block, (int to_endif));

static t_idf *
getid()
{
	int	ch;
	char	buf[IDFSIZE+2];
	char	*tag = &buf[0];

	while (LoadChar(ch), class(ch) == STSKIP) {};
	if (class(ch) == STIDF) {
		do {
			if (tag-buf < IDFSIZE) *tag++ = ch;
			LoadChar(ch);
		} while (in_idf(ch));
		UnloadChar(ch);
		*tag = 0;
		return str2idf(buf, 1);
	}
	return 0;
}

static char *
getstr()
{
	int	ch;

	while (LoadChar(ch), class(ch) == STSKIP) {};
	if (class(ch) == STSTR) {
	    int		upto = ch;
	    unsigned	len;
	    p_flex	f = flex_init(sizeof(char), 32);
	    char	*p;

	    while (LoadChar(ch), ch != upto) {
		if (ch == '\\') LoadChar(ch);  /* escape character */
                if (class(ch) == STNL)  {
                        lexerror("newline in string");
                        UnloadChar(ch);
			free(flex_finish(f, &len));
                        return 0;
                }
                if (ch == EOI)  {
                        lexerror("end-of-file in string");
			free(flex_finish(f, &len));
                        return 0;
                }
                p = flex_next(f);
                *p = ch;
	    }
            p = flex_next(f);
	    *p = 0;
	    return flex_finish(f, &len);
	}
	lexerror("string expected");
	return 0;
}

static void
push_if()
{
	if (nestlevel >= IFDEPTH) {
		fatal("too many nested .ifdef/.ifndef");
	}
	else	ifstack[++nestlevel] = 0;
}

#define pop_if() \
	nestlevel--

void
doconditional()
{
	t_idf 	*p = getid();

	if (p != 0) {
		switch(p->id_resmac) {
		case K_DEFINE:
			do_define();
			break;
		case K_UNDEF:
			do_undef();
			break;
		case K_IFDEF:
			do_ifdef(1);
			break;
		case K_IFNDEF:
			do_ifdef(0);
			break;
		case K_ELSE:
			do_else();
			break;
		case K_ENDIF:
			do_endif();
			break;
		case K_INCLUDE:
			do_include();
			break;
		default:
			lexerror("illegal conditional compilation line");
			SkipToNewLine();
			break;
		}
	}
	else {
		lexerror("illegal conditional compilation line");
		SkipToNewLine();
	}
}

static void
do_include()
{	
	char *fn = getstr();

	if (! fn) {
		SkipToNewLine();
		return;
	}
	if (SkipToNewLine()) {
		lexerror("garbage following .include line");
	}

	DEFPATH[0] = WorkingDir;
	if (! InsertFile(fn, DEFPATH, &FileName)) {
		error("Could not open %s\n", fn);
		return;
	}
	in_include++;
	WorkingDir = get_dirname(FileName);
	LineNumber = 0;
}

static void
do_define()
{
	t_idf	*p = getid();
	int	err = 0;

	if (p != 0) {
		p->id_mac = 1;
	}
	else {
		lexerror("illegal .define line");
		err = 1;
	}
	if (SkipToNewLine()) {
		if (! err) lexerror("garbage following .define line");
	}
}

static void
do_undef()
{
	t_idf	*p = getid();
	int	err = 0;

	if (p != 0) {
		p->id_mac = 0;
	}
	else {
		lexerror("illegal .undef line");
		err = 1;
	}
	if (SkipToNewLine()) {
		if (! err) lexerror("garbage following .undef line");
	}
}

static void
do_ifdef(how)
	int	how;		/* how == 1 : ifdef; how == 0 : ifndef */
{
	t_idf	*p = getid();
	int	err = 0;
	int	ismac = how;

	push_if();
	if (p != 0) {
		ismac = p->id_mac;
	}
	else {
		lexerror("illegal .ifdef line");
		err = 1;
	}
	if (SkipToNewLine()) {
		if (! err) lexerror("garbage following .ifdef line");
	}
	if (how ^ (ismac != 0)) {
		skip_block(0);
	}
}

static void
do_else()
{
	if (SkipToNewLine()) {
		lexerror("garbage following .else");
	}
	if (nestlevel <= nestlow) {
		lexerror(".else without corresponding .ifdef");
	}
	else {
		if (ifstack[nestlevel]) {
			lexerror(".else after .else");
		}
		++ifstack[nestlevel];
		skip_block(1);
	}
}

static void
do_endif()
{
	if (SkipToNewLine()) {
		lexerror("garbage following .endif");
	}
	if (nestlevel <= nestlow) {
		lexerror(".endif without corresponding .ifdef");
	}
	else {
		pop_if();
	}
}

static int
SkipToNewLine()
{
	int	ch;
	int	comment = 0;
	int	garbage = 0;

	while (LoadChar(ch), class(ch) != STNL) {
		if (ch == '#') comment = 1;
		else if (class(ch) != STSKIP) {
			if (! comment) garbage = 1;
		}
	}
	++LineNumber;
	return garbage;
}

static void
skip_block(to_endif)
	int	to_endif;
{
	int	ch;
	int	skiplevel = nestlevel;
	t_idf	*p;

	for (;;) {
		LoadChar(ch);
		if (ch == EOI) {
			return;
		}
		if (class(ch) == STNL) {
			LineNumber++;
			continue;
		}
		if (ch != '.') {
			SkipToNewLine();
			continue;
		}
		p = getid();
		if (p == 0) {
			lexerror("illegal conditional compilation line");
			SkipToNewLine();
			continue;
		}
		switch(p->id_resmac) {
		case K_DEFINE:
		case K_UNDEF:
		case K_INCLUDE:
			SkipToNewLine();
			break;
		case K_IFDEF:
		case K_IFNDEF:
			push_if();
			SkipToNewLine();
			break;
		case K_ELSE:
			if (ifstack[nestlevel]) {
				lexerror(".else after .else");
			}
			++(ifstack[nestlevel]);
			if (!to_endif && nestlevel == skiplevel) {
				if (SkipToNewLine()) {
					lexerror("garbage following .else");
				}
				return;
			}
			SkipToNewLine();
			break;
		case K_ENDIF:
			if (SkipToNewLine()) {
				lexerror("garbage following .endif");
			}
			if (nestlevel == skiplevel) {
				pop_if();
				return;
			}
			pop_if();
			break;
		}
	}
}
