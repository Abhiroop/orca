/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*   D A T A B A S E   R E A D I N G   A N D   W R I T I N G   */

/* $Id: process_db.c,v 1.31 1997/05/15 12:02:49 ceriel Exp $ */

#include "debug.h"
#include "ansi.h"

#include <stdio.h>
#include <alloc.h>
#include <assert.h>
#include <system.h>

#include "process_db.h"
#include "idf.h"
#include "scope.h"
#include "db.h"
#include "generate.h"

extern char	*Version;
static DB	db;
static int	db_from_src_dir;
static int	db_error;

_PROTOTYPE(static void get_dbentry, (t_def *));
_PROTOTYPE(static void put_dbentry, (t_def *));
_PROTOTYPE(static int use_db, (t_def *, char *));
_PROTOTYPE(static char *get_dbnam, (t_def *, int, int));
_PROTOTYPE(static char *dependency_list, (t_dflst, int));
_PROTOTYPE(static char *dbget, (t_def *, int));

#if ! defined(__STDC__) || __STDC__ == 0
extern char *strcpy(), *strcat();
#endif

static char *
dbget(df, from_srcdir)
	t_def	*df;
{
	char	*fn;
	char	*manager;

	fn = get_dbnam(df, 0, from_srcdir);
	if (! use_db(df, fn)) {
		free(fn);
		return 0;
	}
	db = db_open(fn, 0);

	if (! db) {
		free(fn);
		return 0;
	}

	manager = db_manager(db);
	/* Check for old database? Disabled for now.
	if (manager && strcmp(manager, Version)) {
	}
	*/
	return fn;
}

void
get_db(df)
	t_def	*df;
{
	char	*fn;

	fn = dbget(df, 0);
	if (! fn) return;
	db_error = 0;
	if (df->df_kind & (D_MODULE|D_OBJECT)) get_dbentry(df);
	if (db_error) {
		db_close(db);
		free(fn);
		fn = dbget(df, 1);
		if (! fn) return;
		db_error = 0;
		if (df->df_kind & (D_MODULE|D_OBJECT)) get_dbentry(df);
	}
	if (! db_error) {
		walkdefs(df->bod_scope->sc_def,
			 D_OPERATION|D_FUNCTION|D_PROCESS,
			 get_dbentry);
	}
	free(fn);
	db_close(db);
}

void
put_db(df)
	t_def	*df;
{
	char	*fn = get_dbnam(df, 1, 0);

	db = db_open(fn, 1 + (use_db(df, fn) ? 0 : 1));

	if (! db) {
		free(fn);
		return;
	}

	db_setmanager(db, Version);

	if (df->df_kind & (D_MODULE|D_OBJECT)) put_dbentry(df);

	walkdefs(df->bod_scope->sc_def, D_OPERATION|D_FUNCTION|D_PROCESS,
		 put_dbentry);

	db_close(db);
	free(fn);
}

static char *
get_dbnam(df, wr, from_srcdir)
	t_def	*df;
	int	wr;
	int	from_srcdir;
{
	char	*fn;
	char	*rfn;
	FILE	*f;

	db_from_src_dir = 0;
	if (df->df_flags & D_INSTANTIATION) {
		df = df->mod_gendf;
	}
	fn = Malloc((unsigned)(strlen(df->mod_file)+5));
	strcpy(fn, ".");
	strcat(fn, df->mod_file);
	strcat(fn, ".db");
	if (wr) return fn;
	if (! from_srcdir) {
		f = fopen(fn, "r");
		if (f != NULL) {
			fclose(f);
			return fn;
		}
	}
	db_from_src_dir = 1;
	rfn = Malloc((unsigned)(strlen(df->mod_dir)+strlen(fn)+2));
	*rfn = 0;
	if (*(df->mod_dir)) {
		strcpy(rfn, df->mod_dir);
		strcat(rfn, "/");
	}
	strcat(rfn, fn);
	free(fn);
	return rfn;
}

static void
get_dbentry(df)
	t_def	*df;
{
	char	*s;
	char	*nm = (df->df_kind & (D_MODULE|D_OBJECT))
			? "__init__"
			: df->df_idf->id_text;

	if (df->df_kind == D_FUNCTION && (df->df_flags & D_GENERICPAR)) return;

	if ((df->df_kind & (D_MODULE|D_OBJECT)) &&
	    ! db_from_src_dir &&
	    df->mod_dir[0] != '\0') {
		s = db_getfield(db, nm, "src_dir");
		if (! s || strcmp(s, df->mod_dir)) {
			db_error = 1;
			return;
		}
	}
	if (df->df_kind == D_FUNCTION) {
		s = db_getfield(db, nm, "flags");
		if (s && s[0] == 'e') df->df_flags |= D_EXTRAPARAM;
	}
	s = db_getfield(db, nm, "blocking");
	if (s) {
		if (s[0] == 'n') df->df_flags |= D_NONBLOCKING;
		else df->df_flags |= D_BLOCKING;
	}
	s = db_getfield(db, nm, "flags");
	if (s) {
		while (*s) {
			if (*s == 'r') df->df_flags |= D_HASREADS;
			else if (*s == 'w') df->df_flags |= D_HASWRITES;
			s++;
		}
	}
	if (df->df_kind & (D_PROCESS|D_FUNCTION)) {
		s = db_getfield(db, nm, "pattern");
		if (s) df->prc_patstr = Salloc(s, (unsigned)(strlen(s)+1));
		else df->prc_patstr = 0;
	}
#ifdef DEBUG
	s = db_getfield(db, nm, "kind");
	if (s) switch(df->df_kind) {
	case D_OPERATION:
		assert(s[0] == 'o');
		break;
	case D_FUNCTION:
		assert(s[0] == 'f');
		break;
	case D_PROCESS:
		assert(s[0] == 'p');
		break;
	case D_OBJECT:
		assert(s[0] == 'i');
		break;
	}
#endif
}

static char *
dependency_list(l, rw)
	t_dflst	l;
	int	rw;
{
	/*	Creates a comma-separated list of entries in the list
		indicated by 'l'. Each entry has the following form:
			name(dir/file)
		If rw is set, a suffix is added to name depending on the
		read/write behavior.
	*/

	char	*s = 0;
	unsigned int
		slen = 0;
	t_def	*df;

	def_walklist(l, l, df) {
		t_def *mdf = (df->df_kind & (D_MODULE|D_OBJECT))
					? df
					: df->df_scope->sc_definedby;
		unsigned int len = strlen(df->df_idf->id_text)
				   + strlen(mdf->mod_file) + 3;

		if (*(mdf->mod_dir)) len += strlen(mdf->mod_dir) + 1;
		s = Realloc(s, slen + len);
		if (! slen) *s = 0;
		else strcat(s, ",");
		strcat(s, df->df_idf->id_text);
		slen += len;
		if (rw) {
			char *srw = "";
			if (df->df_kind == D_OPERATION) {
			  int f = df->df_flags & (D_HASREADS|D_HASWRITES);
			  srw = (f == D_HASREADS)
					? ".r"
					: (f == D_HASWRITES) ? ".w" : ".rw";

			}
			else if (df->df_kind == D_FUNCTION) {
				 srw = (df->df_flags & D_EXTRAPARAM) ? ".e" : ".n";
			}
			if (*srw) {
				slen += strlen(srw);
				s = Realloc(s, slen);
				strcat(s, srw);
			}
		}
		strcat(s, "(");
		if (*(mdf->mod_dir)) {
			strcat(s, mdf->mod_dir);
			strcat(s, "/");
		}
		strcat(s, mdf->mod_file);
		strcat(s, ")");
	}
	return s ? s : "";
}

static void
put_dbentry(df)
	t_def	*df;
{
	int	m = df->df_flags & (D_HASREADS|D_HASWRITES);
	char	*nm = (df->df_kind & (D_MODULE|D_OBJECT))
			? "__init__"
			: df->df_idf->id_text;

	if ((df->df_kind & (D_MODULE|D_OBJECT)) &&
	    (df->df_flags & D_INSTANTIATION)) return;
	if ((df->df_kind & (D_MODULE|D_OBJECT)) &&
	    ! db_from_src_dir &&
	    df->mod_dir[0] != '\0') {
		db_putfield(db, nm, "src_dir", df->mod_dir);
	}
	switch(df->df_kind) {
	case D_OPERATION:
		if (df->df_type == nil_type) break;
		db_putfield(db, nm, "kind", "operation");
		db_putfield(db, nm, "blocking",
			df->df_flags & D_BLOCKING ? "yes" : "no");
		db_putfield(db, nm, "flags",
			m == (D_HASREADS|D_HASWRITES) ? "rw"
				: m == D_HASREADS ? "r" : "w");
		db_putfield(db, nm, "blockdep",
			dependency_list(df->prc_blockdep, 0));
		db_putfield(db, nm, "transdep",
			dependency_list(df->bod_transdep, 1));
		break;
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		db_putfield(db, nm, "kind", "function");
		db_putfield(db, nm, "blocking",
			df->df_flags & D_BLOCKING ? "yes" : "no");
		db_putfield(db, nm, "flags",
			df->df_flags & D_EXTRAPARAM ? "e" : "n");
		db_putfield(db, nm, "blockdep",
			dependency_list(df->prc_blockdep, 0));
		db_putfield(db, nm, "transdep",
			dependency_list(df->bod_transdep, 1));
		db_putfield(db, nm, "pattern", df->prc_patstr);
		break;
	case D_PROCESS:
		db_putfield(db, nm, "kind", "process");
		db_putfield(db, nm, "transdep",
			dependency_list(df->bod_transdep, 1));
		db_putfield(db, nm, "pattern", df->prc_patstr);
		break;
	case D_OBJECT:
	case D_MODULE:
		db_putfield(db, nm, "kind", "init");
		db_putfield(db, nm, "transdep",
			dependency_list(df->bod_transdep, 1));
		db_putfield(db, nm, "imports",
			dependency_list(df->mod_imports, 0));
		if (df->df_flags & D_GENERIC) {
			db_putfield(db, nm, "generic", "");
		}
		if (df->df_flags & D_MAIN) {
			db_putfield(db, nm, "main_program", "");
		}

		break;
	}
}

static int
use_db(df, dfn)
	t_def	*df;
	char	*dfn;
{
	/*	Determine if the database file dfn, if it exsists, is at
		least as new as the specification for "df". If it exists and
		is, return 1, otherwise return 0.
	*/

	long	dtm = sys_modtime(dfn);
	long	stm;
	char	*buf;

	if (dtm == -1L) return 0;
	buf = Malloc((unsigned)strlen(df->mod_dir)+strlen(df->mod_file)+6);
	if (*(df->mod_dir)) {
		strcpy(buf, df->mod_dir);
		strcat(buf, "/");
		strcat(buf, df->mod_file);
	}
	else	strcpy(buf, df->mod_file);
	strcat(buf, ".spf");
	stm = sys_modtime(buf);
	free(buf);
	return ((unsigned long)stm < (unsigned long)dtm) ? 1 : 0;
}
