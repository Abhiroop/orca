/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* T O K E N   D E F I N I T I O N S */

/* $Id: tokenname.c,v 1.21 1998/06/26 10:20:56 ceriel Exp $ */

#include	"ansi.h"
#include	"tokenname.h"
#include	"idf.h"
#include	"LLlex.h"
#include	"error.h"
#include	"options.h"
#include	"conditional.h"

/*	To centralize the declaration of %tokens, their presence in this
	file is taken as their declaration. The Makefile will produce
	a grammar file (tokenfile.g) from this file. This scheme ensures
	that all tokens have a printable name.
	Also, the "token2str.c" file is produced from this file.
*/

#ifdef ___NOTDEF___
struct tokenname tkspec[] =	{	/* the names of the special tokens */
	{IDENT, "identifier", 0},
	{STRING, "string", 0},
	{INTEGER, "number", 0},
	{CHARACTER, "CHARACTER", 0},
	{REAL, "real", 0},
	/* The next few tokens have a space after the { so that make.tokfile
	   does not see them (but make.tokcase does).
	*/
	{ U_CHECK, "union check", 0},
	{ A_CHECK, "array check", 0},
	{ G_CHECK, "graph check", 0},
	{ CHECK, "runtime check", 0},
	{ ORBECOMES, "OR:=", 0 },
	{ ANDBECOMES, "AND:=", 0 },
	{ LSHBECOMES, "<<:=", 0 },
	{ RSHBECOMES, ">>:=", 0 },
	{ INIT, "INIT", 0 },
	{ ALIAS_CHK, "alias check", 0 },
	{ TMPBECOMES, ":=", 0 },
	{ ARR_INDEX, "array index", 0 },
	{ ARR_SIZE, "array size", 0 },
	{ FOR_UPDATE, "FOR_UPDATE", 0 },
	{ UPDATE, "UPDATE", 0 },
	{ COND_EXIT, "COND_EXIT", 0 },
	{ ALIASBECOMES, "ALIASBECOMES", 0 },
	{ FROM_CHECK, "FROM_CHECK", 0 },
	{ DIV_CHECK, "DIV_CHECK", 0 },
	{ MOD_CHECK, "MOD_CHECK", 0 },
	{ CPU_CHECK, "CPU_CHECK", 0 },
	{0, "", 0}
};

struct tokenname tkcomp[] =	{	/* names of the composite tokens */
	{LESSEQUAL, "<=", 0},
	{GREATEREQUAL, ">=", 0},
	{UPTO, "..", 0},
	{ARROW, "=>", 0},
	{BECOMES, ":=", 0},
	{NOTEQUAL, "/=", 0},
	{PLUSBECOMES, "+:=", 0},
	{MINBECOMES, "-:=", 0},
	{TIMESBECOMES, "*:=", 0},
	{DIVBECOMES, "/:=", 0},
	{MODBECOMES, "%:=", 0},
	{LEFTSHIFT, "<<", 0},
	{RIGHTSHIFT, ">>", 0},
	{B_ORBECOMES, "|:=", 0 },
	{B_ANDBECOMES, "&:=", 0 },
	{B_XORBECOMES, "^:=", 0 },
	{DOLDOL, "$$", 1 },
	{0, "", 0}
};
#endif

struct tokenname tkidf[] =	{	/* names of the identifier tokens */
	{ACCESS, "ACCESS", 1},
	{AND, "AND", 0},
	{ARRAY, "ARRAY", 0},
	{BAG, "BAG", 0},
	{BEGIN, "BEGIN", 0},
	{CASE, "CASE", 0},
	{CONST, "CONST", 0},
	{DATA, "DATA", 0},
	{DEPENDENCIES, "DEPENDENCIES", 1},
	{DO, "DO", 0},
	{ELSE, "ELSE", 0},
	{ELSIF, "ELSIF", 0},
	{END, "END", 0},
	{ESAC, "ESAC", 0},
	{EXIT, "EXIT", 0},
	{FI, "FI", 0},
	{FOR, "FOR", 0},
	{FORK, "FORK", 0},
	{FROM, "FROM", 0},
	{FUNCTION, "FUNCTION", 0},
	{GATHER, "GATHER", 1},
	{GENERIC, "GENERIC", 0},
	{GRAPH, "GRAPH", 0},
	{GUARD, "GUARD", 0},
	{IF, "IF", 0},
	{IMPLEMENTATION, "IMPLEMENTATION", 0},
	{IMPORT, "IMPORT", 0},
	{IN, "IN", 0},
	{MODULE, "MODULE", 0},
	{NEW, "NEW", 0},
	{NODENAME, "NODENAME", 0},
	{NODES, "NODES", 0},
	{NOT, "NOT", 0},
	{NUMERIC, "NUMERIC", 0},
	{OBJECT, "OBJECT", 0},
	{OD, "OD", 0},
	{OF, "OF", 0},
	{ON, "ON", 0},
	{OPERATION, "OPERATION", 0},
	{OR, "OR", 0},
	{OUT, "OUT", 0},
	{PARALLEL, "PARALLEL", 1},
	{PROCESS, "PROCESS", 0},
	{RECORD, "RECORD", 0},
	{REDUCE, "REDUCE", 1},
	{REPEAT, "REPEAT", 0},
	{RETURN, "RETURN", 0},
	{SCALAR, "SCALAR", 0},
	{SET, "SET", 0},
	{SHARED, "SHARED", 0},
	{SPECIFICATION, "SPECIFICATION", 0},
	{THEN, "THEN", 0},
	{TYPE, "TYPE", 0},
	{UNION, "UNION", 0},
	{UNTIL, "UNTIL", 0},
	{WHILE, "WHILE", 0},
	{WITH, "WITH", 1},
	{0, "", 0}
};

#ifdef ___NOTDEF___
struct tokenname tkinternal[] = {	/* internal keywords	*/
	{0, "", 0}
};

struct tokenname tkstandard[] =	{	/* standard identifiers */
	{0, "", 0}
};
#endif

struct tokenname tkconditional[] = {
	{	K_DEFINE, "define", 0},
	{	K_UNDEF, "undef", 0},
	{	K_IFDEF, "ifdef", 0},
	{	K_IFNDEF, "ifndef", 0},
	{	K_ELSE, "else", 0},
	{	K_ENDIF, "endif", 0},
	{	K_INCLUDE, "include", 0},
	{ 0, "", 0},
};

/* Some routines to handle tokennames */

void
reserve(resv)
	struct tokenname
		*resv;
{
	/*	The names of the tokens described in resv are entered
		as reserved words.
	*/
	t_idf	*p;

	while (resv->tn_symbol)	{
		if (dp_flag || ! resv->tn_data_parallel) {
			p = str2idf(resv->tn_name, 0);
			if (!p) fatal("out of memory");
			p->id_reserved = resv->tn_symbol;
		}
		resv++;
	}
}

void
add_conditionals()
{
	struct tokenname *c = tkconditional;
	t_idf	*p;

	while (c->tn_symbol) {
		p = str2idf(c->tn_name, 0);
		if (!p) fatal("out of memory");
		p->id_resmac = c->tn_symbol;
		c++;
	}
}
