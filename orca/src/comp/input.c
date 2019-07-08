/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* I N S T A N T I A T I O N   O F   I N P U T	 P A C K A G E */

/* $Id: input.c,v 1.6 1998/06/26 10:20:55 ceriel Exp $ */

#include	"ansi.h"
#include	"f_info.h"
struct f_info	file_info;
#include	"input.h"
#include	"conditional.h"
#include	"error.h"
#include	<inp_pkg.body>


int
AtEoIF()
{
	/*	Make the unstacking of input streams noticable to the
		lexical analyzer
	*/
	if (in_include) {
		in_include--;
		return 0;
	}
	if (nestlevel != nestlow) {
		lexerror("missing .endif");
		nestlevel = nestlow;
	}
	return 1;
}

int
AtEoIT()
{
	/*	Make the end of the text noticable
	*/
	return 1;
}
