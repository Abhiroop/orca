/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: getdb.c,v 1.7 1995/07/31 08:56:54 ceriel Exp $ */

/*   G E T   D A T A B A S E   E N T R Y   */

#include "ansi.h"

#include <stdio.h>
#include <alloc.h>

#include "db.h"
#include "idf.h"
#include "getdb.h"
#include "main.h"

#if ! defined(__STDC__) || __STDC__ == 0
extern char *strrchr();
#endif

t_idf *
getinfo(name, keepdb)
	char	*name;
	int	keepdb;
{
	char	*modnam = strrchr(name, '/');
	char	*dotpos;
	char	*dir = 0;
	char	*fn;
	t_idf	*idf;

	if (! modnam) {
		modnam = name;
	}
	else {
		*modnam = 0;
		dir = Salloc(name, strlen(name)+1);
		*modnam = '/';
		modnam++;
	}
	dotpos = strrchr(modnam, '.');
	fn = Salloc(modnam, dotpos ? dotpos-modnam+1 : strlen(modnam)+1);
	if (dotpos) {
		fn[dotpos-modnam] = '\0';
	}
	modnam = fn;

	idf = str2idf(modnam, 1);
	if (! keepdb && idf->id_db) {
		db_close(idf->id_db);
		idf->id_db = 0;
		idf->id_generic = 0;
	}
	if (! idf->id_db) {
		char *prefix = (wdir && ! strcmp(wdir, DRIVER_DIR)) ? "../" : "";

		fn = Malloc(strlen(modnam)+(dir ? strlen(dir)+9 : 8));
		sprintf(fn, "%s.%s.db", prefix, modnam);
		idf->id_db = db_open(fn, 0);

		if (! idf->id_db && dir) {
			sprintf(fn, "%s%s/.%s.db", dir[0] != '/' ? prefix : "",
				dir, modnam);
			idf->id_db = db_open(fn, 0);
		}
		free(fn);
		if (idf->id_db) {
			idf->id_generic = db_getfield(idf->id_db, "__init__", "generic") != 0;
		}
	}
	free(modnam);
	if (! idf->id_wdir) {
		idf->id_wdir = dir ? dir : ".";
	}
	else if (dir) free(dir);
	return idf;
}
