/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: LLmessage.c,v 1.1 1999/10/11 14:22:59 ceriel Exp $ */
/*		PARSER ERROR ADMINISTRATION		*/

#include	"arith.h"
#include	"LLlex.h"
#include	"Lpars.h"

extern char *symbol2str();

LLmessage(tk)	{
	if (tk < 0)
		error("garbage at end of line");
	else if (tk)	{
		error("%s missing", symbol2str(tk));
		if (DOT != EOF) SkipToNewLine();
		DOT = EOF;
	}
	else
		error("%s deleted", symbol2str(DOT));
}
