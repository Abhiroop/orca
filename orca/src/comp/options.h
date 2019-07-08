/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

/* U S E R   O P T I O N - H A N D L I N G */

/* $Id: options.h,v 1.7 1997/05/15 12:02:45 ceriel Exp $ */

#include	"ansi.h"

_PROTOTYPE(void DoOption, (char *option));
	/*	Process option 'option'.
	*/

extern char options[];		/* For the one-letter options. */
extern int  dp_flag;		/* data-parallel orca. */
extern char *dep_filename;	/* Filename on which to produce
				   make-dependencies.
				*/
#endif /* __OPTIONS_H__ */
