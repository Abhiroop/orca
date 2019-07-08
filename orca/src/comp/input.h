/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __INPUT_H__
#define __INPUT_H__

/* I N S T A N T I A T I O N   O F   I N P U T	 M O D U L E */

/* $Id: input.h,v 1.5 1997/05/15 12:02:19 ceriel Exp $ */

#include	"ansi.h"
#include	"inputtype.h"
#include	"f_info.h"

#define INP_NPUSHBACK	3
#define INP_TYPE	struct f_info
#define INP_VAR		file_info

_PROTOTYPE(int AtEoIF, (void));
	/*	Returns 1, to make unstacking of an input stream visible.
	*/

_PROTOTYPE(int AtEoIT, (void));
	/*	Returns 1, to make unstacking of input text visible.
	*/

#include <inp_pkg.spec>
#endif /* __INPUT_H__ */
