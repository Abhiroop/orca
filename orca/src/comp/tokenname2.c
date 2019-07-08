/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* T O K E N   D E F I N I T I O N S;
 *
 * S E C O N D	 P A R T:  L O W E R   C A S E	 K E Y W O R D S
 *
 */

/* $Id: tokenname2.c,v 1.9 1997/05/15 12:03:14 ceriel Exp $ */

/* This file contains a table with Orca keywords in lower case; it cannot
 * be merged with the tokenname.c file, since that file is preprocessed.
 */

#include	"ansi.h"
#include	"tokenname.h"
#include	"LLlex.h"

struct tokenname tkidf_lc[] =	{	/* names of the identifier tokens */
	{ACCESS, "access", 1},
	{AND, "and", 0},
	{ARRAY, "array", 0},
	{BAG, "bag", 0},
	{BEGIN, "begin", 0},
	{CASE, "case", 0},
	{CONST, "const", 0},
	{CHARACTER, "character", 0},
	{DATA, "data", 0},
	{DEPENDENCIES, "dependencies", 1},
	{DO, "do", 0},
	{ELSE, "else", 0},
	{ELSIF, "elsif", 0},
	{END, "end", 0},
	{ESAC, "esac", 0},
	{EXIT, "exit", 0},
	{FI, "fi", 0},
	{FOR, "for", 0},
	{FORK, "fork", 0},
	{FROM, "from", 0},
	{FUNCTION, "function", 0},
	{GATHER, "gather", 1},
	{GENERIC, "generic", 0},
	{GRAPH, "graph", 0},
	{GUARD, "guard", 0},
	{IF, "if", 0},
	{IMPLEMENTATION, "implementation", 0},
	{IMPORT, "import", 0},
	{IN, "in", 0},
	{MODULE, "module", 0},
	{NEW, "new", 0},
	{NODENAME, "nodename", 0},
	{NODES, "nodes", 0},
	{NOT, "not", 0},
	{NUMERIC, "numeric", 0},
	{OBJECT, "object", 0},
	{OD, "od", 0},
	{OF, "of", 0},
	{ON, "on", 0},
	{OPERATION, "operation", 0},
	{OR, "or", 0},
	{OUT, "out", 0},
	{PARALLEL, "parallel", 1},
	{PROCESS, "process", 0},
	{RECORD, "record", 0},
	{REDUCE, "reduce", 1},
	{REPEAT, "repeat", 0},
	{RETURN, "return", 0},
	{SCALAR, "scalar", 0},
	{SET, "set", 0},
	{SHARED, "shared", 0},
	{SPECIFICATION, "specification", 0},
	{THEN, "then", 0},
	{TYPE, "type", 0},
	{UNION, "union", 0},
	{UNTIL, "until", 0},
	{WHILE, "while", 0},
	{WITH, "with", 1},
	{0, ""}
};
