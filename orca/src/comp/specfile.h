/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __SPECFILE_H__
#define __SPECFILE_H__

/* S P E C I F I C A T I O N S */

/* $Id: specfile.h,v 1.8 1997/05/15 12:03:00 ceriel Exp $ */

#include	"ansi.h"
#include	"idf.h"
#include	"def.h"

_PROTOTYPE(p_def get_specification, (p_idf id,
				     int is_import,
				     int kind,
				     int generic,
				     p_idf inst_id));
        /*      Returns a pointer to the "def" structure of the
                module or object specification indicated by "id".
                We may have to read the module specification itself.
		"is_import" is set when the call is a result from an IMPORT.
                "kind" indicates whether a module or an object is expected.
		"generic" is set when the specification must be a GENERIC.
                "inst_id" is only used when this is an instantiation, for
                name generation.
        */

extern int	Specification;	/* Set when reading an object/module
				   specification.
				*/

#endif /* __SPECFILE_H__ */
