/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __INSTANTIATE_H__
#define __INSTANTIATE_H__

/* I N S T A N T I A T I O N   O F   G E N E R I C S */

/* $Id: instantiate.h,v 1.7 1997/05/15 12:02:22 ceriel Exp $ */

#include	"ansi.h"
#include	"idf.h"
#include	"node.h"
#include	"def.h"
#include	"type.h"

_PROTOTYPE(void start_generic_formals, (void));
	/*	Indicates the start of the generic formal parameter
		specifications.
	*/

_PROTOTYPE(void add_generic_formal, (p_def df, p_type tp));
	/*	Adds a definition 'df' with type 'tp' to the generic formal
		parameter specifications, and matches it with the
		corresponding actual parameter in case of an instantiation.
	*/

_PROTOTYPE(void end_generic_formals, (void));
	/*	Indicates the end of the generic formal parameter
		specifications.
	*/

_PROTOTYPE(void start_instantiate, (int kind, p_idf inst_id, p_idf gen_id));
        /*      Instantiation of the generic module/object indicated by
		"gen_id".  "inst_id" indicates the name of the instantiation,
                "kind" is either D_MODULE or D_OBJECT.
        */

_PROTOTYPE(void add_generic_actual, (p_node nd));
	/*	Adds 'nd' to the list of actual parameters for the
		instantiation.
	*/

_PROTOTYPE(void finish_instantiate, (void));
	/*	End of the instantiation. Obtains the generic specification
		(by recursively calling the parser).
	*/

#endif /* __INSTANTIATE_H__ */
