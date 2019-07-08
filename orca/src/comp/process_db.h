/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __PROCESS_DB_H__
#define __PROCESS_DB_H__

/*   D A T A B A S E   R E A D I N G   A N D   W R I T I N G   */

/* $Id: process_db.h,v 1.5 1997/05/15 12:02:50 ceriel Exp $ */

#include	"ansi.h"
#include	"def.h"

/* For each module/object, a database is maintained containing information
   about the module/object, and processes, operations, functions, et cetera
   declared in this module/object. This information consists, a.o. of
   - read/write behavior of operations
   - blocking behavior of operations
   - access patterns for functions and processes
   - compilation dependencies between modules/objects
*/
	
_PROTOTYPE(void get_db, (p_def df));
	/*	Gets the current data base for the module/object indicated
		by 'df'.
	*/

_PROTOTYPE(void put_db, (p_def df));
	/*	Writes the database for the module/object indicated by 'df'.
	*/

#endif /* __PROCESS_DB_H__ */
