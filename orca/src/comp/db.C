/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: db.C,v 1.17 1997/05/15 12:01:49 ceriel Exp $ */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <alloc.h>
#include "ansi.h"
#include "db.h"
#include "locking.h"

#define DB_HSIZ	29		/* size of hash table, must be odd. */

typedef struct db_field {
	char	*db_fldnam;
	char	*db_fldval;
	struct db_field
		*db_nextfld;
} db_field;

typedef struct db_hash {
	char	*db_key;
	db_field
		*db_line;
	struct db_hash
		*db_next,
		*db_link;
} db_hash;

struct database {
	char	*db_filename;
	char	*db_manager;
	char	*db_stamp;
	int	db_written;	/* set when db_putfield is called. */
	FILE	*db_fd;
	db_hash	*db_htab[DB_HSIZ];
	db_hash	*db_list;
	db_hash *db_pos;	/* for db_nextentry. */
};

_PROTOTYPE(static DB db_init, (char *));
_PROTOTYPE(static char *db_getline, (FILE *));
_PROTOTYPE(static void db_processline, (DB, char *));
_PROTOTYPE(static int db_hashval, (char *));
_PROTOTYPE(static FILE *db_read, (char *f));
_PROTOTYPE(static FILE *db_write, (char *f));
_PROTOTYPE(static void db_closefd, (FILE *fd));
_PROTOTYPE(static void db_wrhtab, (FILE *, db_hash *));
_PROTOTYPE(static void db_wrftab, (FILE *, db_field *));
_PROTOTYPE(static void db_freehtab, (db_hash *));
_PROTOTYPE(static void db_freeftab, (db_field *));
#ifdef LOCKING
_PROTOTYPE(static int get_readlock, (int));
_PROTOTYPE(static int get_writelock, (int));
_PROTOTYPE(static void release_lock, (int));
#endif

#define DB_MAGIC	"!<db>!\n"
#define DB_STAMP	"Stamp: %d\n"
#define DB_MANAGER	"Manager: %s"

/* STATICALLOCDEF "db_field" 100 */

/* STATICALLOCDEF "db_hash" 25 */

DB
db_open(f, mode)
	char	*f;
	int	mode;
{
	/*	When the database is opened for reading and writing, we
		keep a write-lock until db_close is called. Therefore,
		we cannot close the file descriptor after reading the
		contents (but we can if the database is only opened for
		reading).
	*/
	FILE	*fd;
	DB	db = db_init(f);
	char	buf[20];
	char	*s;

	/* Open and lock database file. */
	if (! (fd = mode == 0 ? db_read(f) : db_write(f))) {
		db_close(db);
		return 0;
	}

	/* Remember file descriptor when reading and writing. */
	if (mode != 0) db->db_fd = fd;

	/* Get magic word. */
	if (fgets(buf, 20, fd) == 0) {
		if (mode == 0) {
			db_closefd(fd);
			db_close(db);
			return 0;
		}
		return db;
	}
	if (strcmp(buf, DB_MAGIC) != 0) {
		db_closefd(fd);
		db_close(db);
		return 0;
	}

	/* Get manager. */
	s = db_getline(fd);
	if (s) {
		db->db_manager = s;
	}
	else {
		if (mode == 0) {
			db_closefd(fd);
		}
		return db;
	}

	/* Get stamp. */
	s = db_getline(fd);
	if (s) {
		if (db->db_stamp) free(db->db_stamp);
		db->db_stamp = s;
	}
	else {
		if (mode == 0) db_closefd(fd);
		return db;
	}

	/* Get contents of database. */
	if (mode != 2) {
		while ((s = db_getline(fd))) {
			db_processline(db, s);
		}
	}
	if (mode == 0) db_closefd(fd);
	return db;
}

char *
db_manager(db)
	DB	db;
{
	return db->db_manager;
}

void
db_setmanager(db, manager)
	DB	db;
	char	*manager;
{
	char	buf[1024];

	(void) sprintf(buf, DB_MANAGER, manager);
	if (db->db_manager) {
		if (strcmp(db->db_manager, buf) == 0) return;
		free(db->db_manager);
	}
	db->db_manager = Salloc(buf, (unsigned)(strlen(buf)+1));
	db->db_written = 1;
}

char *
db_getfield(db, key, fldnam)
	DB	db;
	char	*key;
	char	*fldnam;
{
	db_hash	*h = db->db_htab[db_hashval(key)];
	db_field
		*f;

	while (h && strcmp(h->db_key, key) != 0) h = h->db_next;
	if (! h) return 0;
	f = h->db_line;
	while (f && strcmp(f->db_fldnam, fldnam) != 0) f = f->db_nextfld;
	if (! f) return 0;
	return f->db_fldval;
}

void
db_putfield(db, key, fldnam, fldval)
	DB	db;
	char	*key;
	char	*fldnam;
	char	*fldval;
{
	int	hval = db_hashval(key);
	db_hash	*h = db->db_htab[hval];
	db_field
		*f;

	if (! fldval) fldval = "";
	while (h && strcmp(h->db_key, key) != 0) h = h->db_next;
	if (! h) {
		h = new_db_hash();
		if (! db->db_list) {
			db->db_list = h;
		}
		else {
			db->db_pos->db_link = h;
		}
		db->db_pos = h;
		h->db_key = Salloc(key, (unsigned)(strlen(key)+1));
		h->db_next = db->db_htab[hval];
		db->db_htab[hval] = h;
	}
	f = h->db_line;
	while (f && strcmp(f->db_fldnam, fldnam) != 0) f = f->db_nextfld;
	if (! f) {
		f = new_db_field();
		f->db_nextfld = h->db_line;
		h->db_line = f;
		f->db_fldnam = fldnam;
	}
	if (! f->db_fldval || strcmp(f->db_fldval, fldval) != 0) {
		/* fprintf(stderr, "%s is %s, was %s\n", f->db_fldnam, fldval,
			!f->db_fldval ? "(null)" : f->db_fldval); */
		db->db_written = 1;
	}
	f->db_fldval = fldval;
}

int
db_close(db)
	DB	db;
{
	/*	When reading and writing, we still have an exclusive lock
		at this point. However, although we could write the new
		contents of the db on the same file descriptor, there is no
		way to truncate in case the new version is shorter.
		Therefore, the db is re-opened for writing (this is
		possible as the lock is only advisory).
	*/
	FILE	*fd;
	int	i;

	if (db->db_fd) {
		char	*p;
		int	stampno;
		/* Need to write. Check that there is a manager. */
#ifndef LOCKING
		db_closefd(db->db_fd);
#endif
		if (! db->db_manager) return 0;

		/* Open file for writing. */
		fd = fopen(db->db_filename, "w");
		if (! fd) return 0;

		fputs(DB_MAGIC, fd);
		fprintf(fd, "%s\n", db->db_manager);

		/* Give it a new stamp, but only if the database actually
		   changed. Otherwise, it just gets the old one.
		*/
		p = db->db_stamp;
		while (*p && (*p < '0' || *p > '9')) p++;
		stampno = atoi(p);
		if (db->db_written) {
			fprintf(fd, DB_STAMP, stampno+1);
		}
		else	fprintf(fd, DB_STAMP, stampno);

		for (i = DB_HSIZ - 1; i >= 0; i--) {
			db_wrhtab(fd, db->db_htab[i]);
		}
		fflush(fd);
#ifdef LOCKING
		db_closefd(db->db_fd);
#endif
		fclose(fd);
	}

	/* Dispose of the database contents. */
	for (i = DB_HSIZ - 1; i >= 0; i--) {
		db_freehtab(db->db_htab[i]);
	}
	if (db->db_stamp) free(db->db_stamp);
	if (db->db_manager) free(db->db_manager);
	free((char *) db);
	return 1;
}

static DB
db_init(f)
	char	*f;
{
	int	i;
	char	buf[20];
	DB	db = (DB) Malloc(sizeof(struct database));

	db->db_written = 0;
	db->db_manager = 0;
	db->db_filename = f;
	db->db_fd = 0;
	db->db_list = 0;
	(void) sprintf(buf, DB_STAMP, 0);
	db->db_stamp = Salloc(buf, (unsigned)(strlen(buf)+1));
	for (i = DB_HSIZ-1; i>= 0; i--) {
		db->db_htab[i] = 0;
	}
	return db;
}

static char *
db_getline(fd)
	FILE	*fd;
{
	unsigned int
		len = 512;
	char	*s = Malloc(len);
	char	*c = s;
	int	ch;

	while (ch = getc(fd), ch != EOF && ch != '\n') {
		if (c == &s[len-2]) {
			s = Realloc(s, len+len);
			c = &s[len-2];
			len += len;
		}
		*c++ = ch;
	}
	if (c == s && ch == EOF) {
		free(s);
		return 0;
	}
	*c++ = 0;
	s = Realloc(s, (unsigned)(c - s));
	return s;
}

#define starthash(h)	((h) = 0)
#define enhash(h, ch)	((h) = ((h) << 2) + ch)
#define stophash(h)	((h) = (h) % DB_HSIZ)

static void
db_processline(db, s)
	DB	db;
	char	*s;
{
	char	*c = s;
	db_hash	*h;
	unsigned int
		hval;

	starthash(hval);
	while (*c && *c != ';') {
		enhash(hval, *c);
		c++;
	}
	stophash(hval);
	if (! *c) return;
	*c++ = 0;
	h = new_db_hash();
	if (! db->db_list) {
		db->db_list = h;
	}
	else {
		db->db_pos->db_link = h;
	}
	db->db_pos = h;
	h->db_key = s;
	h->db_next = db->db_htab[hval];
	db->db_htab[hval] = h;
	while (*c) {
		db_field *f = new_db_field();

		f->db_nextfld = h->db_line;
		h->db_line = f;
		f->db_fldnam = c;
		while (*c && *c != ':' && *c != ';') c++;
		f->db_fldval = c;
		if (! *c) return;
		if (*c == ';') {
			*c++ = '\0';
			continue;
		}
		*c++ = 0;
		f->db_fldval = c;
		while (*c && *c != ';') c++;
		if (! *c) return;
		*c++ = 0;
	}
}

static int
db_hashval(s)
	char	*s;
{
	unsigned int
		h;

	starthash(h);
	while (*s) {
		enhash(h, *s);
		s++;
	}
	stophash(h);
	return h;
}

/*  F I L E   O P E N I N G   A N D   C L O S I N G   W I T H	L O C K S  */

static FILE *
db_read(f)
	char	*f;
{
	/*	Open the file 'f' for reading, using a shared lock.
		Return NULL if either the open or the locking fails.
		Note: this function may block, but is interruptable.
	*/
	FILE	*fd = fopen(f, "r");

	if (fd == NULL) {
		return NULL;
	}
#ifdef LOCKING
	if (! get_readlock(fileno(fd))) {
		fclose(fd);
		return NULL;
	}
#endif
	return fd;
}

static FILE *
db_write(f)
	char	*f;
{
	/*	Open the file 'f' for reading/writing, using an exclusive lock.
		Return NULL if either the open or the locking fails.
		Note: this function may block, but is interruptable.
		If mode is 2, rewrite the database.
	*/
	FILE	*fd = fopen(f, "r+");

	if (fd == NULL) {
		fd = fopen(f, "w+");
		if (fd == NULL) return NULL;
	}
#ifdef LOCKING
	if (! get_writelock(fileno(fd))) {
		fclose(fd);
		return NULL;
	}
#endif
	return fd;
}

static void
db_closefd(fd)
	FILE	*fd;
{
	/*	Close file descriptor fd, after releasing the lock.
	*/
#ifdef LOCKING
	fflush(fd);
	release_lock(fileno(fd));
#endif
	fclose(fd);
}

static void
db_wrhtab(fd, h)
	FILE	*fd;
	db_hash	*h;
{
	if (! h) return;
	if (h->db_next) {
		db_wrhtab(fd, h->db_next);
	}
	fprintf(fd,"%s;", h->db_key);
	db_wrftab(fd, h->db_line);
	putc('\n', fd);
}

static void
db_wrftab(fd, f)
	FILE	*fd;
	db_field
		*f;
{
	if (! f) return;
	if (f->db_nextfld) db_wrftab(fd, f->db_nextfld);
	if (f->db_fldval == 0) {
		fprintf(fd, "%s;", f->db_fldnam);
	}
	else fprintf(fd, "%s:%s;", f->db_fldnam, f->db_fldval);
}

static void
db_freehtab(h)
	db_hash	*h;
{
	if (! h) return;
	if (h->db_next) {
		db_freehtab(h->db_next);
	}
	free(h->db_key);
	db_freeftab(h->db_line);
	free_db_hash(h);
}

static void
db_freeftab(f)
	db_field
		*f;
{
	if (! f) return;
	if (f->db_nextfld) db_freeftab(f->db_nextfld);
	free_db_field(f);
}

void
db_initentry(db)
	DB	db;
{
	db->db_pos = db->db_list;
}

char *
db_nextentry(db)
	DB	db;
{
	db_hash	*pos = db->db_pos;

	if (! pos) return 0;
	db->db_pos = pos->db_link;
	return pos->db_key;
}

/*  F I L E   L O C K I N G  */

#ifdef LOCKING
static int
get_readlock(fd)
	int	fd;
{
	/*	Try to get a readlock on the file with file descriptor
		'fd'. Return 1 if it succeeds, 0 if it fails for some
		reason.
		Note: this function may block, but is interruptable.
	*/
	struct flock
		farg;

	farg.l_type = F_RDLCK;
	farg.l_whence = SEEK_SET;
	farg.l_start = 0;
	farg.l_len = 0;
	if (fcntl(fd, F_SETLKW, &farg) == -1) return 0;
	return 1;
}

static int
get_writelock(fd)
	int	fd;
{
	/*	Try to get a writelock on the file with file descriptor
		'fd'. Return 1 if it succeeds, 0 if it fails for some
		reason.
		Note: this function may block, but is interruptable.
	*/
	struct flock
		farg;

	farg.l_type = F_WRLCK;
	farg.l_whence = SEEK_SET;
	farg.l_start = 0;
	farg.l_len = 0;
	if (fcntl(fd, F_SETLKW, &farg) == -1) return 0;
	return 1;
}

static void
release_lock(fd)
	int	fd;
{
	/*	Release the lock held on file descriptor 'fd'.
	*/
	struct flock
		farg;

	farg.l_type = F_UNLCK;
	farg.l_whence = SEEK_SET;
	farg.l_start = 0;
	farg.l_len = 0;
	(void) fcntl(fd, F_SETLK, &farg);
}
#endif
