/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: main.c,v 1.34 1997/07/02 14:12:37 ceriel Exp $ */

/*   O R C A   C O M P I L E R   D R I V E R   */

#include "ansi.h"

#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <alloc.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "main.h"
#include "defaults.h"
#include "db.h"
#include "idf.h"
#include "getdb.h"
#include "chk_compile.h"
#include "arglist.h"
#include "strlist.h"

#ifndef __STDC__
extern char *strchr(), *strrchr(), *strncpy();
#endif

/* Prototypes of static functions. */

_PROTOTYPE(static void catch, (int signo));
_PROTOTYPE(static void cleanup, (void));
_PROTOTYPE(static void do_remove, (char *fn));
_PROTOTYPE(static char *basename, (char *str));
_PROTOTYPE(static int run_vec, (struct arglist *vec, char *name, int silent));
_PROTOTYPE(static int chk_status, (int status, char *proc));
_PROTOTYPE(static void Orca_compile, (void));
_PROTOTYPE(static void C_compile, (void));
_PROTOTYPE(static void do_checks, (struct arglist *ld));
_PROTOTYPE(static void hl_compile, (void));
_PROTOTYPE(static void all_compile, (void));
_PROTOTYPE(static void run_orca, (char *s));
_PROTOTYPE(static void add_includes_to_C_line, (void));
_PROTOTYPE(static int check_changed_line, (char *b, char *s, struct arglist *c));
_PROTOTYPE(static void move_changed_line, (char *b, char *s));
_PROTOTYPE(static void unlink_changed_line, (char *s));
_PROTOTYPE(static void do_chdir, (char *dir));
_PROTOTYPE(static int compare_and_move, (char *src, char *dst));
_PROTOTYPE(static int compare, (char *src, char *dst));
_PROTOTYPE(static void copy, (char *src, char *dst));
_PROTOTYPE(static void move, (char *src, char *dst));

static char	*drivername;		/* Name of this program. */
static int	do_C = 1;		/* Set if C compilation is required. */
static int	do_ld = 1;		/* Set if linking is required. */
static int	hl_driver;		/* set if "high" level driver. */
static int	all_driver;		/* set if all Orca files must be offered
					   to Orca compiler in one go.
					*/
static int	needs_clean = 1;	/* cleanup after panic, etc? */
static char	*o_file;		/* Where to store result of linking. */
int		v_flag;			/* Verbose flag. */
static int	no_exec;		/* Only print commands, do not exec. */
static int	exit_status;		/* Result of this run, != 0 if one or
					   more compilations failed.
					*/

extern int	errno;

static struct arglist
		sources,		/* Sources files. */
		sys_mach,		/* machine for which to compile. */
		libs,			/* Libraries. */
		C_sources,		/* List of C files. */
		Orca_line,		/* Compilation command for Orca file. */
		Orca_includes,		/* List of Orca include dirs. */
		Orca_srcdirs,		/* List of Orca source directories. */
		C_line,			/* Compilation command for C file. */
		ld_line,		/* Command for linking. */
		Orca_done;		/* Which Orca files have been compiled
					   earlier? Needed to suppress warnings
					   the second time around.
					*/

static int	Cpos,			/* Where to put C file in C_line. */
		Orcapos;		/* Where to put Orca file in Orca_line.
					*/
static int	kid;			/* Process id of compiler. */
char		*base;			/* Base name of current source file. */
int		changed_Cline;		/* Set if C compilation command
					   changed from previous time.
					*/
char		*wdir;
char		*oc_comp;

int kill(pid_t pid, int sig);
pid_t wait(int *stat_loc);

static void
catch(signo)
	int	signo;
{
	signal(signo, SIG_IGN);
	if (kid) kill(kid, signo);
	cleanup();
	unlink_changed_line(".occ");
	unlink_changed_line(".ccc");
	unlink_changed_line(".sys");
	exit(1);
}

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	register char *arg;
	int	ac;
	char	**av;
	char	*special = 0;
	char	*libnamold = 0;
	int	oc_specialset = 0;
	FILE	*info_f;

	drivername = basename(*argv++);

	if (mkdir(DRIVER_DIR, 0777) == -1
	    && errno != EEXIST) {
		panic("could not create directory %s", DRIVER_DIR);
	}

	if (! strcmp(drivername, "oc_2c")) {
		do_C = 0;
		do_ld = 0;
	}
	else if (! strcmp(drivername, "oc_check")) {
		do_C = 0;
		do_ld = 0;
		hl_driver = 1;
	}
	else if (! strcmp(drivername, "oc_2o")) {
		do_ld = 0;
	}
	else if (strcmp(drivername, "oc_ld")) {
		hl_driver = 1;
	}

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		signal(SIGHUP, catch);
	}
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		signal(SIGINT, catch);
	}
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN) {
		signal(SIGQUIT, catch);
	}

	init_defaults();

	/* Get sources and possible redefinitions of defaults. */
	ac = argc;
	av = argv;

	while (--ac > 0) {
		arg = *av++;
		if (*arg != '-') {
			register char *p = strchr(arg, '=');
			if (p) {
				*p = 0;
				if (! strcmp("OC_SPECIAL", arg)) {
					/* char *s = get_value("OC_SPECIAL"); */

					if (special) free(special);
					special = Salloc(p+1, strlen(p+1)+1);
					oc_specialset = 3;
				}
				if (! set_value(arg, p+1)) {
					panic("illegal variable: %s", arg);
				}
			}
			else append(arg, &sources);
		}
		else if (arg[1] == 'o' && arg[2] == '\0') {
			ac--;
			av++;
		}
		else if (arg[1] == 'p' && oc_specialset < 2) {
			char *s = get_value("OC_LIBNAMOLD");
			if (libnamold) free(libnamold);
			libnamold = Salloc(s, strlen(s)+3);
			strcat(libnamold, "_p");
			if (special) free(special);
			special = Salloc("profiling", 10);
			oc_specialset = 2;
		}
		else if (oc_specialset < 3 && ! strcmp(arg, "-trc")) {
			char *s = get_value("OC_LIBNAMOLD");
			if (libnamold) free(libnamold);
			libnamold = Salloc(s, strlen(s)+7);
			strcat(libnamold, "_trace");
			if (special) free(special);
			special = Salloc("tracing", 8);
			oc_specialset = 3;
		}
		else if (arg[1] == 'O' && ! oc_specialset) {
			char *s = get_value("OC_LIBNAMOLD");
			libnamold = Salloc(s, strlen(s)+3);
			strcat(libnamold, "_o");
			special = Salloc("optimized", 10);
			oc_specialset = 1;
		}
	}

	if (oc_specialset) {
		(void) set_value("OC_SPECIAL", special);
		(void) set_value("OC_LIBNAMOLD", libnamold);
	}

	split_and_append(get_value("OC_COMP"), &Orca_line);
	oc_comp = Orca_line.args[1];
	split_and_append(get_value("OC_MACH"), &sys_mach);
	split_and_append(get_value("OC_FLAGS"), &Orca_line);
	split_and_append(get_value("OC_CCOMP"), &C_line);
	split_and_append(get_value("OC_CFLAGS"), &C_line);
	split_and_append(get_value("OC_LD"), &ld_line);
	split_and_append(get_value("OC_LDFLAGS"), &ld_line);

	append(".", &Orca_srcdirs);

	while (--argc > 0) {
		arg = *argv++;
		if (*arg != '-') {
			continue;
		}
		switch(arg[1]) {
		case 'a':
			all_driver = 1;
			break;

		case 'c':
			do_ld = 0;
			break;

		case 'o':
			if (arg[2] == '\0') {
				if (--argc > 0) o_file = *argv++;
				else panic("-o without file name");
				break;
			}
			append(arg, &C_line);
			append(arg, &ld_line);
			break;

		case 'l':
			append(arg, &sources);
			break;

		case 'k':
		case 'K':
		case 'w':
		case 'N':
		case 'i':
		case 'u':
		case '-':
		case 'A':
			append(arg, &Orca_line);
			break;

		case 't':
			if (! strcmp(arg, "-trc")) {
				append("-DTRACING", &C_line);
				break;
			}
			append(arg, &C_line);
			append(arg, &ld_line);
			break;

		case 'I':
			append(arg, &Orca_line);
			append(arg, &Orca_includes);
			break;

		case 'D':
		case 'U':
			append(arg, &Orca_line);
			append(arg, &C_line);
			break;

		case 'v':
			v_flag++;
			if (! strcmp(arg, "-values")) {
				print_values();
				return 0;
			}
			if (arg[2] == 'n') no_exec = 1;
			break;

		case 'C':
			if (! strcmp(arg, "-CHK")) {
				append("-DNO_CHECKS", &C_line);
				append("--c", &Orca_line);
				break;
			}
			append(arg, &C_line);
			append(arg, &ld_line);
			break;

		case 'S':
			append(&arg[2], &Orca_srcdirs);
			break;

		case 'O':
			append(arg, &C_line);
			break;

		case 'L':
			if (! strcmp(arg, "-LIN")) {
				/* Recognized for backwards compatibility */
				break;
			}

			if (arg[2] == '\0') {
				append(arg, &Orca_line);
				break;
			}
			/* fall through */

		default:
			append(arg, &C_line);
			append(arg, &ld_line);
			break;
		}
	}
	split_and_append(get_value("OC_INCLUDES"), &Orca_line);
	split_and_append(get_value("OC_INCLUDES"), &Orca_includes);
	add_includes_to_C_line();
	split_and_append(get_value("OC_RTSINCLUDES"), &C_line);
	if (o_file) {
		append("-o", &ld_line);
		append(o_file, &ld_line);
	}
	split_and_append(get_value("OC_STARTOFF"), &ld_line);
	append("...", &Orca_line);
	Orcapos = Orca_line.argc;
	append("...", &C_line);
	Cpos = C_line.argc;
	info_f = fopen(".info.c", "w");
	if (info_f) {
		time_t	tm = time((time_t *) 0);
		char	*t = ctime(&tm);
		assert(t[24] == '\n');
		fprintf(info_f, "char compilation_date[] = \"%.24s\";\n", t);

		fputs("char Orca_compilation_command[] = \"", info_f);
		pr_vec(&Orca_line, info_f, '"');
		fputs(";\n", info_f);
		fputs("char C_compilation_command[] = \"", info_f);
		pr_vec(&C_line, info_f, '"');
		fputs(";\n", info_f);
	}
	else panic("could not create .info.c");
	if (all_driver) {
		all_compile();
	}
	else if (hl_driver) {
		hl_compile();
	}
	else {
		Orca_compile();
		if (! exit_status) {
			do_checks(&C_sources);
		}
	}
	needs_clean = 0;
	if (! exit_status && do_C) C_compile();
	if (C_sources.argc > 0 && ! exit_status && do_ld) {
		if (libs.argc) {
			int i = 1;
			while ((arg = libs.args[i])) {
				append(arg, &ld_line);
				i++;
			}
		}
		/* OC_LIBS after libs, because what is in libs might
		   refer to what is defined in OC_LIBS.
		*/
		split_and_append(get_value("OC_LIBS"), &ld_line);
		if (info_f) {
			append(".info.o", &ld_line);
			fputs("char link_command[] = \"", info_f);
			pr_vec(&ld_line, info_f, '"');
			fputs(";\n", info_f);
			fclose(info_f);
			info_f = 0;
			C_line.args[Cpos] = ".info.c";
			(void) run_vec(&C_line, ".info", 1);
		}
		(void) run_vec(&ld_line, "Linking", 0);
	}
	if (info_f) {
		fclose(info_f);
	}
	unlink_changed_line(".occ");
	unlink_changed_line(".ccc");
	unlink_changed_line(".sys");
	return exit_status;
}

static char *
basename(str)
	char	*str;
{
	char	*p = str;
	char	*endp;

	while (*p) p++;
	endp = p;
	while (p != str && *p != '/') p--;
	if (*p == '/') str = p+1;
	p = endp;
	while (p != str && *p != '.') p--;
	if (*p != '.') p = endp;
	else endp = p;
	p = Malloc(endp - str + 1);
	strncpy(p, str, endp - str);
	p[endp-str] = '\0';
	return p;
}

#if __STDC__
void
error(char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		fprintf(stderr, "%s: ", drivername);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
	exit_status = 1;
}
#else
/*VARARGS*/
void
error(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char *fmt = va_arg(ap, char *);

		fprintf(stderr, "%s: ", drivername);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
	exit_status = 1;
}
#endif

#if __STDC__
void
panic(char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		fprintf(stderr, "%s: ", drivername);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
	cleanup();
	unlink_changed_line(".occ");
	unlink_changed_line(".ccc");
	unlink_changed_line(".sys");
	exit(1);
}
#else
/*VARARGS*/
void
panic(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char *fmt = va_arg(ap, char *);

		fprintf(stderr, "%s: ", drivername);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
	cleanup();
	unlink_changed_line(".occ");
	unlink_changed_line(".ccc");
	unlink_changed_line(".sys");
	exit(1);
}
#endif

static void
Orca_compile()
{
	register int	i = 1;
	register char	*srcp;

	if (sources.argc == 0) return;

	while ((srcp = sources.args[i])) {
		register char *p = strrchr(srcp, '.');

		if (p &&
		    ( ! strcmp(p, ".spf") || ! strcmp(p, ".imp"))) {
			base = basename(srcp);
			Orca_line.args[Orcapos] = srcp;
			run_orca(srcp);
			(void) check_changed_line(base, ".occ", &Orca_line);
			move_changed_line(base, ".occ");
			free(base);
			base = 0;
		}
		else append(srcp, &C_sources);
		i++;
	}
}

static void
C_compile()
{
	register int	i = 1;
	register char	*srcp;
	int		changed_sys = 0;

	if (C_sources.argc == 0) return;
	if (hl_driver) {
		changed_sys = check_changed_line("", ".sys", &sys_mach);
		if (v_flag > 2 && changed_sys) {
			printf("target system changed; recompiling C files ...\n");
		}
	}
	while ((srcp = C_sources.args[i])) {
		register char *p = strrchr(srcp, '.');

		i++;
		if (srcp[0] != '-' && p &&
		    (! strcmp(p, ".c") ||
		     ! strcmp(p, ".C") ||
		     ! strcmp(p, ".cc") ||
		     ! strcmp(p, ".c++") ||
		     ! strcmp(p, ".cxx") ||
		     ! strcmp(p, ".cpp"))) {
			char *b = basename(srcp);
			struct idf *id = getinfo(srcp, 1);
			char *ofile = Malloc(strlen(b)+4+strlen(DRIVER_DIR));

			if (hl_driver && id->id_db) {
				sprintf(ofile, "%s/%s.o", DRIVER_DIR, b);
			}
			else sprintf(ofile, "%s.o", b);
			append(ofile, &ld_line);
			C_line.args[Cpos] = srcp;
			if (hl_driver && id->id_db) {
				changed_Cline = check_changed_line(b, ".ccc", &C_line);
				do_chdir(DRIVER_DIR);
				if (! changed_sys && chk_C_compile(srcp)) {
					do_chdir("..");
					free(b);
					continue;
				}
			}
			(void) run_vec(&C_line, srcp, 0);
			if (hl_driver && id->id_db) {
				do_chdir("..");
				if (changed_Cline) {
					move_changed_line(b, ".ccc");
				}
			}
			free(b);
		}
		else append(srcp, &libs);
	}
	if (hl_driver) {
		if (changed_sys) move_changed_line("", ".sys");
	}
}

#ifdef AMOEBA
extern char **environ;
#endif

static int
run_vec(vec, s, silent)
	struct arglist *vec;
	char *s;
	int silent;
{
	int status, pid;
	FILE	*ef;
	int	f;

	if (v_flag) {
		pr_vec(vec, stdout, '\n');
		if (no_exec) return 1;
	}
	if (s && ! silent && hl_driver) {
		fprintf(stderr, "%s:\n", s);
	}
	f = open(".oc_errors", O_WRONLY|O_CREAT|O_TRUNC, 0666);
#ifdef AMOEBA
	{ int fd[3];
	  fd[0] = 0;
	  fd[1] = 1;
	  fd[2] = f;

	  pid = newprocp(vec->args[1], &vec->args[1], environ, 3, fd, 0L);
	  if (pid < 0) {
		panic("could not execute %s", vec->args[1]);
	  }
	}
	close(f);
#else
	if ((pid = fork()) == 0) {
		if (f >= 0) {
			close(2);
			dup(f);
		}
		execvp(vec->args[1], &vec->args[1]);
		if (errno == ENOEXEC) {
			vec->args[0] = "/bin/sh";
			execv(vec->args[0], &vec->args[0]);
		}
		panic("could not execute %s", vec->args[1]);
	}
	else if (pid == -1) {
		panic("could not start process");
	}
	else {
		close(f);
	}
#endif
	kid = pid;
	wait(&status);
	if (! silent || status) {
	    ef = fopen(".oc_errors", "r");
	    if (ef != NULL) {
		int	c = getc(ef);

		if (c != EOF) {
			if (s &&
			    (! hl_driver || silent)) {
				fprintf(stderr, "%s:\n", s);
			}
			while (c != EOF) {
				putc(c, stderr);
				c = getc(ef);
			}
		}
		fclose(ef);
	    }
	}
	if (! silent) unlink(".oc_errors");
	kid = 0;
	if (chk_status(status, vec->args[1])) {
		return 1;
	}
	exit_status = 1;
	return 0;
}

static int
chk_status(status, prog)
	int	status;
	char	*prog;
{
	if (status) {
		switch(status & 0177) {
		case 0:
			break;
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			catch(status & 0177);
			break;
		default:
			error("%s died with signal %d",
				prog,
				status&0177);
		}
		return 0;
	}
	return 1;
}

static void
cleanup()
{
	char	buf[1000];

	if (needs_clean && base) {
		sprintf(buf, ".%s.db", base);
		unlink(buf);
		sprintf(buf, "%s.h", base);
		do_remove(buf);
		sprintf(buf, "%s.c", base);
		do_remove(buf);
		sprintf(buf, "%s.gc", base);
		do_remove(buf);
		sprintf(buf, "%s.gh", base);
		do_remove(buf);
	}
}


static void
do_remove(fn)
	char	*fn;
{
	FILE *f = fopen(fn, "r");
	char buf[128];
	static char *idstr = "/* Produced by the Orca-to-C compiler */";
	static int idstrlen;
	int n;

	if (! idstrlen) idstrlen = strlen(idstr);
	if (f == NULL) return;
	(void) fgets(buf, 127, f);
	n = strlen(buf);
	if (n < idstrlen && ! feof(f)) {
		fclose(f);
		return;
	}
	if (n > idstrlen) n = idstrlen;
	fclose(f);
	if (! strncmp(buf, idstr, n)) {
		unlink(fn);
	}
}

static void
do_checks(args)
	struct arglist	*args;
{
	register int	i = 1;
	register char	*srcp;

	while (args->args && (srcp = args->args[i])) {
		register char *p = strrchr(srcp, '.');

		if (p &&
		    (! strcmp(p, ".c") || ! strcmp(p, ".o"))) {
			t_idf *id = getinfo(srcp, 1);

			base = basename(srcp);
			if (id->id_db) {
			    if (v_flag > 1) {
				printf( "checking consistency of %s\n",
					base);
			    }
			    if (! chk_orca_compile(srcp)) {
				error("module \"%s\" should be recompiled", base);
			    }
			}
			free(base);
		}
		i++;
	}
}

char *
Orca_specfile(f, wd)
	char	*f;
	char	*wd;
{
	register char	**ip;
	int		len = strlen(f)+5;

	/* First, look in source directories. */
	ip = &Orca_srcdirs.args[1];
	if (wd) {
		Orca_srcdirs.args[1] = wd;
	}
	else	Orca_srcdirs.args[1] = ".";

	while (*ip) {
		int iscurrdir = ! strcmp(*ip, ".");
		int namelen = strlen(*ip)+len+2;
		register char	*p = Malloc(namelen);

		if (iscurrdir) sprintf(p, "%s.spf", f);
		else sprintf(p, "%s/%s.spf", *ip, f);
		if (access(p, R_OK) != 0) {
			if (iscurrdir) sprintf(p, "%.10s.spf", f);
			else sprintf(p, "%s/%.10s.spf", *ip, f);
			if (access(p, R_OK) != 0) {
				free(p);
				ip++;
				continue;
			}
		}
		if (hl_driver) {
			char *fn = Malloc(namelen);
			if (iscurrdir) sprintf(fn, "%s.imp", f);
			else sprintf(fn, "%s/%s.imp", *ip, f);
			if (access(fn, R_OK) != 0) {
				if (iscurrdir) sprintf(fn, "%.10s.imp", f);
				else sprintf(fn, "%s/%.10s.imp", *ip, f);
				if (access(fn, R_OK) != 0) {
					free(fn);
					(void) append_unique(p, &sources);
					p = Salloc(p, namelen);
				}
				else if (append_unique(fn, &sources) == 0) {
					free(fn);
				}
			}
			else if (append_unique(fn, &sources) == 0) {
				free(fn);
			}
		}
		return p;
	}

	/* Now look in include directories. */
	ip = &Orca_includes.args[1];

	while (*ip) {
		register char	*p = Malloc(strlen(*ip)+len);

		sprintf(p, "%s/%s.spf", &(*ip)[2], f);
		if (access(p, R_OK) != 0) {
			sprintf(p, "%s/%.10s.spf", &(*ip)[2], f);
			if (access(p, R_OK) != 0) {
				free(p);
				ip++;
				continue;
			
			}
		}
		return p;
	}
	return 0;
}

char *
include_file(f)
	char	*f;
{
	register char	**ip;
	int		len = strlen(f);

	ip = &C_line.args[1];

	if (access(f, R_OK) == 0) {
		return Salloc(f, len+1);
	}
	while (*ip) {
		if ((*ip)[0] == '-' && (*ip)[1] == 'I') {
			register char	*p = Malloc(strlen(*ip)+len);

			strcpy(p, &((*ip)[2]));
			strcat(p, "/");
			strcat(p, f);
			if (access(p, R_OK) == 0) {
				return p;
			}
			free(p);
		}
		ip++;
	}
	return 0;
}

static void
hl_compile()
{
	int		change = 1;
	register int	i;
	register char	*srcp;
	int		first = 1;

	if (sources.argc == 0) return;

	while (change && exit_status == 0) {
		i = 1;

		change = 0;
	    	while ((srcp = sources.args[i])) {
			register char *p = strrchr(srcp, '.');
			char *nm, *path;

			i++;
			if (p &&
			    ( ! strcmp(p, ".spf") || ! strcmp(p, ".imp"))) {
				struct idf *id = getinfo(srcp, 1);

		    		base = basename(srcp);
				Orca_line.args[Orcapos] = srcp;
				if (check_changed_line(base, ".occ", &Orca_line)
				    && ! id->id_ocdone) {
					if (v_flag > 2) {
						 printf("Orca compilation command changed ... recompiling %s\n", srcp);
					}
				}
		    		else if (chk_orca_compile(srcp)) {
					if (! id->id_generic) {
						char *fn = Malloc(strlen(base)+3);
						sprintf(fn, "%s.c", base);
						if (! append_unique(fn, &C_sources)) free(fn);
					}
					if (v_flag > 1 && ! id->id_ocdone) {
						printf("Translation of %s is up-to-date.\n",
							srcp);
						id->id_ocdone = 1;
					}
					free(base);
					continue;
		    		}
		    		if (! no_exec) {
					change = 1;
					if (v_flag > 1 && id->id_ocdone) {
						printf("Translation of %s is no longer  up-to-date.\n",
							srcp);
					}
				}
		    		run_orca(srcp);

				if (exit_status == 0) {
				    /* Now make sure that all imported files are
				       on the compilation list.
				    */
				    id = getinfo(srcp, 1);
				    if (id->id_db) {
				        void *l = set_strlist(db_getfield(id->id_db, "__init__", "imports"));
				        while (get_strlist(&nm, &path, l)) {
					    p = Orca_specfile(nm, id->id_wdir);
					    if (p) free(p);
					    free(path);
					    free(nm);
				        }
					end_strlist(l);
				    }
				}
				move_changed_line(base, ".occ");
				id->id_ocdone = 1;
		    		free(base);
		    		base = 0;
			}
			else if (first) {
				append(srcp, &C_sources);
			}
	    	}
		first = 0;
	}
}

static void
all_compile()
{
	/*	Compile all Orca files in one go, so that compiler sees
		all Orca files.
	*/
	int		change = 1;
	register int	i;
	register char	*srcp;
	int		first = 1;

	if (sources.argc == 0) return;

	while (change && exit_status == 0) {
		Orca_line.argc = Orcapos - 1;
		change = 0;

		i = 1;
		/* First, recreate Orca compilation command. */
	    	while ((srcp = sources.args[i])) {
			register char *p = strrchr(srcp, '.');

			if (p &&
			    ( ! strcmp(p, ".spf") || ! strcmp(p, ".imp"))) {
				append(srcp, &Orca_line);
			}
			else if (first) append_unique(srcp, &C_sources);
			i++;
		}

		/* Then, check if it has changed. */
		if (check_changed_line(".ALL.COMP", ".occ", &Orca_line)) {
			change = 1;
		}
		else {
			/* If not, check if any of the source files must
			   be recompiled.
			*/
			for (i = Orcapos; i <= Orca_line.argc; i++) {
				base = basename(Orca_line.args[i]);
				if (check_changed_line(base, ".occ", &Orca_line)
				    || ! chk_orca_compile(Orca_line.args[i])) {
					free(base);
					change = 1;
					break;
				}
				free(base);
			}
		}

		if (! change && ! first) {
			FILE *ef = fopen(".oc_errors", "r");
			if (ef != NULL) {
				int	c = getc(ef);

				if (c != EOF) {
					while (c != EOF) {
						putc(c, stderr);
						c = getc(ef);
					}
				}
				fclose(ef);
	  		}
		}

		if (! first) unlink(".oc_errors");

		first = 0;

		/* If we must, compile. */
		if (change && run_vec(&Orca_line, (char *) 0, 1)) {
			/* Update files containing compilation commands. */
			(void) check_changed_line(".ALL.COMP", ".occ", &Orca_line);
			move_changed_line(".ALL.COMP", ".occ");

			for (i = Orcapos; i <= Orca_line.argc; i++) {
				t_idf *id = getinfo(Orca_line.args[i], 0);
				char buf[1000];
				char *fn;

				base = basename(Orca_line.args[i]);
				fn = Malloc(strlen(base)+5);

				(void) check_changed_line(base, ".occ", &Orca_line);
				move_changed_line(base, ".occ");

				/* Compare and move compiler output. */
				strcpy(fn, base);
				strcat(fn, id->id_generic ? ".gh" : ".h");
				if (! no_exec) {
		  			sprintf(buf, "%s/%s", DRIVER_DIR, fn);
		  			(void) compare_and_move(fn, buf);
				}
				strcpy(fn, base);
				strcat(fn, id->id_generic ? ".gc" : ".c");
				if (! no_exec) {
		  			sprintf(buf, "%s/%s", DRIVER_DIR, fn);
		  			(void) compare_and_move(fn, buf);
				}
				if (! id->id_generic) {
					if (! append_unique(fn, &C_sources)) free(fn);
				}
				else free(fn);

				/* Add Orca source files that are not included
				   yet.
				*/
				if (id->id_db) {
				    char *nm, *path;
				    void *l = set_strlist(db_getfield(id->id_db, "__init__", "imports"));
				    while (get_strlist(&nm, &path, l)) {
				        char *p = Orca_specfile(nm, id->id_wdir);
					if (p) free(p);
					free(path);
					free(nm);
				    }
				    end_strlist(l);
				}
				id->id_ocdone = 1;
				free(base);
				base = 0;
			}
		}
		else if (change) {
			for (i = Orcapos; i <= Orca_line.argc; i++) {
				base = basename(Orca_line.args[i]);
				cleanup();
				free(base);
			}
		}
	}
}

static void
run_orca(s)
	char	*s;
{
	int silent = ! append_unique(s, &Orca_done);
	if (run_vec(&Orca_line, s, silent)) {
		t_idf *id = getinfo(s, 0);
		char buf[1000];
		char *fn = Malloc(strlen(base)+5);

		if (silent) unlink(".oc_errors");
		strcpy(fn, base);
		strcat(fn, id->id_generic ? ".gh" : ".h");
		if (! no_exec) {
		  	sprintf(buf, "%s/%s", DRIVER_DIR, fn);
			if (hl_driver) {
		  		(void) compare_and_move(fn, buf);
			}
			else	copy(fn, buf);
		}
		strcpy(fn, base);
		strcat(fn, id->id_generic ? ".gc" : ".c");
		if (! no_exec) {
		  	sprintf(buf, "%s/%s", DRIVER_DIR, fn);
			if (hl_driver) {
		  		(void) compare_and_move(fn, buf);
			}
			else	copy(fn, buf);
		}
		if (! id->id_generic) {
			if (! append_unique(fn, &C_sources)) free(fn);
		}
		else free(fn);
	}
	else {
		cleanup();
	}
}

static void
add_includes_to_C_line()
{
	/*	Add the includes to the C-compilation line.  For every include,
		an include is added for the .oc_driver directory.
	*/

	register int i = 1;
	char	*srcp;
	int 	len = strlen(DRIVER_DIR)+2;

	srcp = Malloc(len+1);
	sprintf(srcp, "-I%s", DRIVER_DIR);
	append(srcp, &C_line);
	while ((srcp = Orca_includes.args[i])) {
		char *buf;
		if (hl_driver && srcp[2] != '/') {
			char *b1 = Malloc(strlen(srcp)+4);
			append(srcp, &C_line);
			sprintf(b1, "-I../%s", &srcp[2]);
			srcp = b1;
		}
		buf = Malloc(strlen(srcp)+len);
		i++;
		sprintf(buf, "%s/%s", srcp, DRIVER_DIR);
		append(srcp, &C_line);
		append(buf, &C_line);
	}
}

static int
check_changed_line(b, suff, line)
	char	*b;
	char	*suff;
	struct arglist *line;
{
	FILE *f;
	char buf[512];
	char buf2[128];

	sprintf(buf2, "%s/.%d%s", DRIVER_DIR, (int) getpid(), suff);
	f = fopen(buf2, "w");
	if (f == NULL) {
		panic("could not open temporary file");
	}
	pr_vec(line, f, '\n');
	fclose(f);

	sprintf(buf, "%s/%s%s", DRIVER_DIR, b, suff);
	return compare(buf2, buf);
}

static void
move_changed_line(b, suff)
	char	*b;
	char	*suff;
{
	char buf[512];
	char buf2[128];

	sprintf(buf2, "%s/.%d%s", DRIVER_DIR, (int) getpid(), suff);
	sprintf(buf, "%s/%s%s", DRIVER_DIR, b, suff);
	(void) move(buf2, buf);
}

static void
unlink_changed_line(suff)
	char	*suff;
{
	char buf[128];

	sprintf(buf, "%s/.%d%s", DRIVER_DIR, (int) getpid(), suff);
	(void) unlink(buf);
	sprintf(buf, ".%d%s", (int) getpid(), suff);
	(void) unlink(buf);
}

static void
do_chdir(d)
	char	*d;
{
	if (chdir(d) != 0) {
		panic("could not chdir to working directory");
	}
	wdir = d;
}

static int
compare_and_move(src, dst)
	char	*src, *dst;
{
	if (compare(src, dst)) {
		move(src, dst);
		return 1;
	}
	(void) unlink(src);
	return 0;
}

static void
move(src, dst)
	char	*src, *dst;
{
	(void) unlink(dst);
	if (rename(src, dst) != 0) {
		panic("could not move %s to %s", src, dst);
	}
}

static int
compare(src, dst)
	char	*src, *dst;
{
	FILE	*fs, *fd;
	int	cs = 0, cd = 1;

	fs = fopen(src, "r");
	fd = fopen(dst, "r");

	if (fs == NULL) {
		panic("could not open %s", src);
	}
	if (fd != NULL) {
		cd = getc(fd);
		while ((cs = getc(fs)) != EOF) {
			if (cd != cs) break;
			cd = getc(fd);
		}
		fclose(fd);
	}
	fclose(fs);
	return cd != cs;
}

static void
copy(src, dst)
	char	*src, *dst;
{
	FILE	*fs, *fd;
	int	cs;

	fs = fopen(src, "r");
	fd = fopen(dst, "w");

	if (fs == NULL) {
		panic("could not open %s", src);
	}
	if (fd == NULL) {
		panic("could not open %s", dst);
	}
	while ((cs = getc(fs)) != EOF) {
		putc(cs, fd);
	}
	fclose(fs);
	fclose(fd);
}
