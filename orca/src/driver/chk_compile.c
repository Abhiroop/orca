/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: chk_compile.c,v 1.20 1997/09/03 14:31:42 ceriel Exp $ */

/*   C H E C K I N G   I F   C O M P I L A T I O N   I S   N E E D E D   */

#include "debugcst.h"
#include "ansi.h"

#include <stdio.h>
#include <assert.h>
#include <alloc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "db.h"
#include "idf.h"
#include "getdb.h"
#include "chk_compile.h"
#include "main.h"
#include "defaults.h"
#include "strlist.h"

_PROTOTYPE(static int chk_consistency, (DB db));
_PROTOTYPE(static int chk_name, (char *name, DB db));

#if ! defined(__STDC__) || __STDC__ == 0
extern char *strchr(), *strrchr();
#endif

int
chk_orca_compile(fn)
	char	*fn;
{
	/*	Check if the Orca file indicated by fn must be (re)compiled.
		This check requires several sub-checks:
		- check if there is a DB file. If not: recompile.
		- check if there is a time stamp. If not: recompile.
		- check that the filename in the time stamp corresponds with
		  fn. If not: recompile.
		- check if resulting .c and .h file (or .gc and .gh file)
		  are present. If not: recompile.
		- check time stamp with modification time of fn, corresponding
		  .spf file, and all imported .spf files. If any of those
		  is newer than the time stamp: recompile.
		- check read/write and blocking assumptions. If not correct:
		  recompile.
		Return 0 if recompilation is needed, 1 otherwise.
	*/

	t_idf	*id = getinfo(fn, 1);
	char	buf[1000];
	struct stat fn_stat, b_stat;
	long	last_compile;
	char	*nm, *path;
	static long	currtim;
	void	*l;
	
	if (! currtim) currtim = time((time_t *) 0);

	if (! id->id_db) {
		/* No database found. Recompilation is needed. */
		if (v_flag > 2) {
			printf("No database found ... recompiling %s.\n", fn);
		}
		return 0;
	}

	assert(base);

	sprintf(buf, "%s/%s.occ", DRIVER_DIR, base);
	if (stat(buf, &fn_stat) != 0) {
		/* Could not stat time stamp. Recompile. */
		if (v_flag > 2) {
			printf("Could not stat time stamp ... recompiling %s.\n", fn);
		}
		return 0;
	}
	last_compile = fn_stat.st_mtime;

	if (last_compile <= currtim &&
	    (stat(fn, &fn_stat) != 0
	     || fn_stat.st_mtime > last_compile)) {
		/* fn is modified after the timestamp.  Recompilation is needed.
		*/
		if (v_flag > 2) {
			printf("File modified after time stamp ... recompiling %s.\n", fn);
		}
		return 0;
	}

	if (stat(oc_comp, &b_stat) != 0
	    || b_stat.st_mtime > last_compile) {
		/* Orca compiler changed after time stamp. Recompile. */
		if (v_flag > 2) {
			printf("Compiler modified after time stamp ... recompiling %s.\n", fn);
		}
		return 0;
	}
	sprintf(buf, "%s/%s.%s", DRIVER_DIR, base, id->id_generic ? "gh" : "h");
	if (stat(buf, &b_stat) != 0) {
		/* Generated include file not present. Recompile. */
		if (v_flag > 2) {
			printf("Generated include file not present ... recompiling %s.\n", fn);
		}
		return 0;
	}
	sprintf(buf, "%s/%s.%s", DRIVER_DIR, base, id->id_generic ? "gc" : "c");
	if (stat(buf, &b_stat) != 0) {
		/* Generated C file not present. Recompile. */
		if (v_flag > 2) {
			printf("Generated C file not present ... recompiling %s.\n", fn);
		}
		return 0;
	}

	l = set_strlist(db_getfield(id->id_db, "__init__", "imports"));
	while (get_strlist(&nm, &path, l)) {
		char	*spf_file = Orca_specfile(nm, id->id_wdir);
		char	*ifile = Malloc(strlen(path)+5);
		int	retval = 1;

		strcpy(ifile, path);
		free(path);
		free(nm);
		strcat(ifile, ".spf");
		if (! spf_file
		    || stat(spf_file, &fn_stat) != 0
		    || stat(ifile, &b_stat) != 0
		    || fn_stat.st_ino != b_stat.st_ino
		    || (last_compile <= currtim
			&& fn_stat.st_mtime > last_compile)) {
			/* If no specification file could be found through the
			   current "include" path, or if it is different from
			   the one mentioned in the DB file, or if it
			   changed, recompilation is needed.
			*/
			if (v_flag > 2) {
				printf("Specfile %s modified ... recompiling %s.\n", spf_file, fn);
			}
			retval = 0;
		}
		if (spf_file) free(spf_file);
		free(ifile);
		if (! retval) {
			end_strlist(l);
			return 0;
		}
	}

	if (! chk_consistency(id->id_db)) {
		if (v_flag > 2) {
			printf("Database inconsistency ... recompiling %s.\n", fn);
		}
		end_strlist(l);
		return 0;
	}
	end_strlist(l);
	return 1;
}

/* Consistency checking consists of two "jobs":
   - checking that the read/write assumptions made during the compilation
     of an operation/function/process/initialisation still are correct;
   - checking that the blocking assumptions made during the compilation of
     an operation/function still are correct.
   The chk_consistency function performs these two checks on each
   operation/function/process/initialisation in the database indicated by "db".
   It returns 1 if these checks succeed, 0 if they fail for some reason
   (in which case a recompilation is required).
*/

static int
chk_consistency(db)
	DB	db;
{
	char *name;

	if (! db) return 1;

	db_initentry(db);
	while ((name = db_nextentry(db))) {
		if (! chk_name(name, db)) {
			return 0;
		}
	}
	return 1;
}

static int
chk_name(name, db)
	char *name;
	DB db;
{
	char *str, *dir;
	void *l;

	/* First, check the translation dependencies. */

	l = set_strlist(db_getfield(db, name, "transdep"));
	while (get_strlist(&str, &dir, l)) {
		char *dtpos = strchr(str, '.');
		t_idf *idf = getinfo(dir, 1);

		free(dir);
		assert(dtpos);
		*dtpos++ = 0;

		if (! idf->id_db) {
			free(str);
			continue;
		}
		dir = db_getfield(idf->id_db, str, "flags");
		if (dir && strcmp(dir, dtpos)) {
			/* Inconsistency found. The file containing the
			   definition for "name" must be recompiled.
			*/
			if (v_flag > 3) {
				printf("Translation of %s assumed %s.%s was a %s, but it is a %s\n",
					name, idf->id_text, str, dtpos, dir);
			}
			free(str);
			end_strlist(l);
			return 0;
		}
		free(str);
	}

	str = db_getfield(db, name, "kind");

	end_strlist(l);
	if (! str || ! strcmp(str, "process") || ! strcmp(str, "init")) {
		return 1;
	}

	/* Now check the blocking dependencies. */

	if (! db_getfield(db, name, "guards")) {
		/* If the operation has guards, it always blocks.
		   If it has not, it depends on the functions/operations it
		   calls.
		*/ 
		int blocking, collect_blocking;

		str = db_getfield(db, name, "blocking");
		blocking = str && str[0] != 'n';
		collect_blocking = 0;
		l = set_strlist(db_getfield(db, name, "blockdep"));
		while (get_strlist(&str, &dir, l)) {
			t_idf *idf = getinfo(dir, 1);

			free(dir);
			if (! idf->id_db) {
				free(str);
				continue;
			}
			dir = db_getfield(idf->id_db, str, "blocking");
			free(str);
			if (dir && dir[0] != 'n') {
				collect_blocking = 1;
			}
		}
		end_strlist(l);
		if (collect_blocking && ! blocking) {
			if (v_flag > 3) {
				printf("Blocking of %s is %d, collect = %d\n",
					name, blocking, collect_blocking);
			}
			return 0;
		}
	}
	return 1;
}

static time_t	mtime;
int
chk_C_compile(fn)
	char	*fn;
{
	/*	Check if the C file indicated by fn must be (re)compiled.
		This check requires several sub-checks:
		- check if C compilation command changed. If so: recompile.
		- check if resulting .o file is present. If not: recompile.
		- check time stamp of .o file with respect to fn and
		  included files. If any of those is newer than the time stamp:
		  recompile.
		Return 0 if recompilation is needed, 1 otherwise.
	*/

	t_idf	*id = getinfo(fn, 1);
	char	buf[1000];
	char	*s = strrchr(fn, '.');
	struct stat fn_stat, o_stat;
	char	*nm, *path;
	void	*l;

	if (! id->id_db) {
		/* No database found. This may happen because there is a
		   C file in the argument list. Recompile.
		*/
		if (v_flag > 2) {
			printf("No database ... recompiling %s.\n", fn);
		}
		return 0;
	}

	if (changed_Cline) {
		if (v_flag > 2) {
			printf("C compilation line changed ... recompiling %s.\n", fn);
		}
		return 0;
	}

	*s = 0;
	sprintf(buf, "%s.o", fn);
	*s = '.';

	if (! id->id_generic && stat(buf, &o_stat) != 0) {
		/* Generated .o file not present. Recompile. */
		if (v_flag > 2) {
			printf("The .o file is not present ... recompiling %s.\n", fn);
		}
		return 0;
	}

	if (! id->id_generic &&
	    ( stat(fn, &fn_stat) != 0 /* ??? */
	     || fn_stat.st_mtime > o_stat.st_mtime)) {
		/* C file newer than object file. Recompile */
		if (v_flag > 2) {
			printf("File newer than object file ... recompiling %s.\n", fn);
		}
		return 0;
	}

	if (id->id_generic) o_stat.st_mtime = mtime;
	l = set_strlist(db_getfield(id->id_db, "__init__", "imports"));
	while (get_strlist(&nm, &path, l)) {
		t_idf	*imp = getinfo(path, 1);
		char	*incl_file;

		if (id->id_generic && id == imp) continue;
		sprintf(buf, "%s.%s", nm, imp->id_generic ? "gh" : "h");
		incl_file = include_file(buf);
		if (! incl_file) {
			sprintf(buf, "%.10s.%s", nm, imp->id_generic ? "gh" : "h");
			incl_file = include_file(buf);
		}
		if (!incl_file
		    || stat(incl_file, &fn_stat) != 0
		    || fn_stat.st_mtime > o_stat.st_mtime) {
			if (v_flag > 2) {
				if (! incl_file) {
					printf("Could not find include file %s ... recompiling %s.\n", buf, fn);
				}
				else {
					printf("Include file %s changed ... recompiling %s.\n", incl_file, fn);
				}
			}
			if (incl_file) free(incl_file);
			end_strlist(l);
			return 0;
		}
		if (incl_file) free(incl_file);
		if (imp->id_generic) {
			sprintf(buf, "%s.gc", nm);
			incl_file = include_file(buf);
			if (! incl_file) {
				sprintf(buf, "%.10s.gc", nm);
				incl_file = include_file(buf);
			}
			if (! incl_file
			    || stat(incl_file, &fn_stat) != 0
			    || fn_stat.st_mtime > o_stat.st_mtime) {
				if (v_flag > 2) {
					if (! incl_file) {
						printf("Could not find include file %s ... recompiling %s.\n", buf, fn);
					}
					else {
						printf("Include file %s changed ... recompiling %s.\n", incl_file, fn);
					}
				}
				if (incl_file) free(incl_file);
				end_strlist(l);
				return 0;
			}
			mtime = o_stat.st_mtime;
			if (! chk_C_compile(incl_file)) {
				end_strlist(l);
				return 0;
			}
			if (incl_file) free(incl_file);
		}
	}

	end_strlist(l);
	return 1;
}
