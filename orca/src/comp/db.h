/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __DB_H__
#define __DB_H__

/* $Id: db.h,v 1.9 1997/05/15 12:01:50 ceriel Exp $ */

#include	"ansi.h"

/* Simple character oriented ASCII incore database routines.
   Neither fieldnames nor keys nor field values may contain a newline,
   a colon, a null-byte, or a semicolon.
*/

typedef struct database
	*DB;

_PROTOTYPE(DB db_open, (char *f, int mode));
	/*	Opens the compiler info file 'f'. Returns 0 when it fails
		for some reason. If 'mode' is 0, only reading is allowed,
		otherwise both reading and writing is allowed.
		If mode is 2, the old contents of the database is
		destroyed.
	*/

_PROTOTYPE(char *db_manager, (DB db));
	/*	If the db_manager field of the DB header is not set, return 0.
		Otherwise, return the manager.
	*/

_PROTOTYPE(void db_setmanager, (DB db, char *manager));
	/*	Set the db_manager field of 'db' to 'manager'.
	*/

_PROTOTYPE(char *db_getfield, (DB db, char *key, char *fldnam));
	/*	From the DB record indicated by 'key', return the field
		indicated by 'fldnam'.
	*/

_PROTOTYPE(void db_putfield, (DB db, char *key, char *fldnam, char *val));
	/*	From the DB record indicated by 'key', set the field
		indicated by 'fldnam' to 'val'.
	*/

_PROTOTYPE(int db_close, (DB db));
	/*	Make all changes of 'db_putfield' effective by writing the
		new database to file. Return 0 if this somehow fails,
		1 if it succeeds.
	*/

_PROTOTYPE(void db_initentry, (DB db));
_PROTOTYPE(char *db_nextentry, (DB db));
	/*	These routines enable the user to walk through the
		database entries one by one. First, call db_initentry,
		and then call db_nextentry until it returns 0.
	*/

#endif /* __DB_H__ */
