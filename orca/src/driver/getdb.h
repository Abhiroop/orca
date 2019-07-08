/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: getdb.h,v 1.3 1995/07/31 08:56:57 ceriel Exp $ */

/*   G E T   D A T A B A S E   E N T R Y   */

/* "name" is a module or object name, possibly with a path prefix, and
   possibly with a .spf or .imp suffix. "getinfo" returns an idf structure
   with the database in id_db. The database is looked for in the directory
   indicated by the path prefix, and also in the current directory.
   If it is not found, the id_db field is set to 0.
*/
_PROTOTYPE(t_idf *getinfo, (char *name, int keepdb));
