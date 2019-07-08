/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* M A I N   P R O G R A M */

/* $Id: main.c,v 1.44 1998/06/24 10:49:55 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<stdio.h>
#include	<alloc.h>
#include	<assert.h>

#include	"input.h"
#include	"f_info.h"
#include	"idf.h"
#include	"LLlex.h"
#include	"node.h"
#include	"scope.h"
#include	"def.h"
#include	"type.h"
#include	"oc_stds.h"
#include	"tokenname.h"
#include	"options.h"
#include	"specfile.h"
#include	"error.h"
#include	"const.h"
#include	"generate.h"
#include	"gen_descrs.h"
#include	"gen_code.h"
#include	"operation.h"
#include	"process_db.h"
#include	"simplify.h"
#include	"prepare.h"
#include	"misc.h"
#include	"main.h"
#include	"strategy.h"
#include	"closure.h"
#include	"chk.h"
#include	"sets.h"
#include	"opt_SR.h"
#include	"opt_LV.h"

t_def		*CurrDef;
char		**DEFPATH;

FILE		*fc, *fh;

static char	*ProgName;
static char	*OrcaString = "/* Produced by the Orca-to-C compiler */\n";

struct stdproc {
	char *st_nam;
	int  st_con;
};

typedef struct {
	char	*a_filename;
	t_def	*a_def;
	int	a_specification;
} t_arg;

_PROTOTYPE(int main, (int, char **));
_PROTOTYPE(static void end_lists, (t_def *));
_PROTOTYPE(static void ParseAndCheck, (char *, int));
_PROTOTYPE(static void CollectInfo, (t_def *));
_PROTOTYPE(static void ProcessInfo, (t_def *));
_PROTOTYPE(static void Translate, (t_def *, int));
_PROTOTYPE(static void add_procs, (struct stdproc *));
_PROTOTYPE(static void add_standards, (void));
_PROTOTYPE(static void gen_module_names, (t_def *));
_PROTOTYPE(static void test_orca_generated, (char *));
_PROTOTYPE(static void gen_outfiles, (t_def *));
_PROTOTYPE(static void gen_dependencies, (t_def *));
_PROTOTYPE(static void initialize, (void));
#ifdef DEBUG
_PROTOTYPE(static void LexScan, (void));
#endif
#if ! defined(__STDC__) || __STDC__ == 0
extern char *strrchr();
extern char *strcat();
extern int CompUnit();
extern int UnitSpecification();
#else
_PROTOTYPE(void CompUnit, (void));
_PROTOTYPE(void UnitSpecification, (void));
#endif

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	int	Nargc = 1;
	char	**Nargv = &argv[0];
	t_arg	*ap;

	ProgName = *argv++;

	DEFPATH = (char **) Malloc(2*(unsigned) sizeof(char *));
	DEFPATH[1] = 0;
	while (--argc > 0) {
		if (**argv == '-')
			DoOption((*argv++));
		else
			Nargv[Nargc++] = *argv++;
	}
	Nargv[Nargc] = 0;	/* terminate the arg vector	*/
	if (Nargc < 2) {
		fprintf(stderr, "%s: Use a file argument\n", ProgName);
		exit(1);
	}
	initialize();
	ap = (t_arg *) Malloc(Nargc * sizeof(t_arg));
	for (argc = 1; argc < Nargc; argc++) {
		char *s = strrchr(Nargv[argc], '.');
		if (! s
		    || (strcmp(s, ".spf") && strcmp(s, ".imp"))) {
			err_occurred = 1;
			fprintf(stderr, "%s: illegal filename %s\n",
				ProgName, Nargv[argc]);
			continue;
		}
		ap[argc].a_specification = !strcmp(s, ".spf");
		ParseAndCheck(Nargv[argc], ap[argc].a_specification);
		ap[argc].a_filename = Nargv[argc];
		ap[argc].a_def = CurrDef;
		if (CurrDef && (CurrDef->df_flags & D_GENERIC)) {
			/* When compiling more than one file, make
			   sure that the specification is read again
			   when instantiating.
			*/
			assert(CurrDef->df_scope == GlobalScope);
			CurrDef->df_scope =
				open_and_close_scope(CLOSEDSCOPE);
		}
	}
	if (err_occurred) {
		exit(1);
	}
#ifdef DEBUG
	if (options['l']) exit(0);
	CurrentScope = PervasiveScope;
	close_scope();
#endif
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, get_db);
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, end_lists);

	for (argc = 1; argc < Nargc; argc++) {
		if (! ap[argc].a_specification) {
			CollectInfo(ap[argc].a_def);
		}
	}

	for (argc = 1; argc < Nargc; argc++) {
		if (! ap[argc].a_specification) {
			ProcessInfo(ap[argc].a_def);
		}
	}

	closure_main();
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, assign_name);

	for (argc = 1; argc < Nargc; argc++) {
		Translate(ap[argc].a_def, ap[argc].a_specification);
	}
	exit(0);
	return 0;
}

static void
initialize()
{
	init_idf();
	init_cst();

	/* Add keywords to identifier table; the Orca compiler has the
	 * following options:
	 *   default: keywords are in upper case
	 *   -k:      keywords are in lower case
	 *   -K:      both upper case and lower case keywords
	 */

	if (!options['k']) {
		reserve(tkidf); /* keywords in upper case */
	}
	if (options['k'] || options['K']) {
		reserve(tkidf_lc);  /* add keywords in lower case */
	}
	add_conditionals();

	init_scope();
	init_types();
	add_standards();
	open_scope(OPENSCOPE);
	GlobalScope = CurrentScope;
	close_scope();
	nestlow = -1;
}

static void
ParseAndCheck(src, specfile)
	char	*src;
	int	specfile;
{
	t_def	*df = 0;
	int	spec_seen = 0;

	CurrentScope = PervasiveScope;
	if (specfile) {
		char *s = get_basename(src);
		t_idf *id = str2idf(s, 1);
		df = lookup(id, GlobalScope, D_IMPORTED);
		free(s);
		if (df) {
			CurrDef = df;
			spec_seen = 1;
			CurrentScope = df->bod_scope;
		}
	}
	if (! spec_seen || ! specfile) {
		if (! InsertFile(src, (char **) 0, &src)) {
			err_occurred = 1;
			fprintf(stderr,"%s: cannot open %s\n", ProgName, src);
			return;
		}
		LineNumber = 0;
		FileName = src;
		WorkingDir = get_dirname(src);
		CurrDef = 0;
#ifdef DEBUG
		if (options['l']) {
			LexScan();
			return;
		}
#endif /* DEBUG */

		LLlexinit();
		if (specfile) {
			Specification++;
			open_scope(CLOSEDSCOPE);
			UnitSpecification();
			chk_forwards();
			df = CurrDef;
			Specification--;
			if (err_occurred) return;
			if (df->df_kind == D_OBJECT) {
				error("object \"%s\" should have an implementation", df->df_idf->id_text);
				return;
			}
			else if (df->df_kind != D_MODULE) {
				error("\"%s\" should be a module", df->df_idf->id_text);
				return;
			}
			CurrentScope->sc_definedby = df;
		}
		else {
			/* start parser */
			CompUnit();
			if (err_occurred) return;
			df = CurrDef;
			CurrentScope = df->bod_scope;
		}
	}
	gen_module_names(df);
	close_scope();
}

static void
CollectInfo(df)
	t_def	*df;
{
	CurrDef = df;
	CurrentScope = df->bod_scope;
	walkdefs(df->bod_scope->sc_def,
		D_FUNCTION|D_OPERATION,
		chk_blocking);
	if (df->df_kind == D_OBJECT) {
		walkdefs(df->bod_scope->sc_def,
			D_OPERATION,
			chk_writes);
	}
}

static void
ProcessInfo(df)
	t_def	*df;
{
	walkdefs(df->bod_scope->sc_def,
		D_FUNCTION|D_PROCESS|D_OPERATION,
		strategy_def);
}

static void
Translate(df, specfile)
	t_def	*df;
	int	specfile;
{
	CurrDef = df;
	if (! specfile) {
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			prepare_df);
		if (options['O']) {
		    walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			do_SR);
		}
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			simplify_df);
		if (options['V']) {
		    walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			do_LV);
		}
		CurrentScope = df->bod_scope;
		if (df->df_kind == D_OBJECT ||
		    (df->df_flags & D_DATA)) {
			prepare_df(df);
			simplify_df(df);
		}
	}
	init_gen();
	gen_dependencies(df);
	gen_outfiles(df);
	gen_stringtab(df);
	walkdefs(df->bod_scope->sc_def,
		D_ISTYPE|D_MODULE|D_PROCESS|D_VARIABLE,
		generate_descrs);
	fprintf(fh, "extern char *%s;\n", df->mod_fn);
	fprintf(fc, "char *%s = \"%s.imp\";\n", df->mod_fn, df->mod_file);
	if (specfile) {
		walkdefs(df->bod_scope->sc_def,
			D_CONST,
			gen_const);
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			gen_proto);
	}
	else {
		if (df->df_kind == D_OBJECT && (df->df_flags & D_PARTITIONED)) {
			fprintf(fc, "static po_p %s;\n", df->df_name);
		}
		walkdefs(df->bod_scope->sc_def,
			D_OPERATION|D_FUNCTION|D_PROCESS,
			gen_localdescrs);
		if (df->mod_funcaddrcnt) {
			df->mod_funcaddrname = gen_name("fnc_", "addr", 0);
			fprintf(fc, "static int %s[%d];\n",
				df->mod_funcaddrname,
				df->mod_funcaddrcnt);
		}
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION|D_CONST,
			gen_const);
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS|D_OPERATION,
			gen_proto);
		walkdefs(df->bod_scope->sc_def,
			D_FUNCTION|D_PROCESS,
			gen_func);
		walkdefs(df->bod_scope->sc_def,
			D_OPERATION,
			gen_func);
		if (df->df_kind == D_MODULE) {
		    if (df->df_flags & D_DATA) {
			walkdefs(df->bod_scope->sc_def, D_VARIABLE, gen_data);
			gen_func(df);
		    }
		    else def_endlist(&df->bod_transdep);
		}
		if (df->df_kind == D_OBJECT) {
			CurrentScope = df->bod_scope;
			gen_proto(df);
			gen_func(df);
		}
	}
	gen_modinit(df);

	/* Allow for more than one inclusion of .h files resulting
	   from generic modules/objects.
	*/
	if (df->df_flags & D_GENERIC) {
		fprintf(fh, "#undef %s____SEEN\n", df->df_idf->id_text);
	}
	/* Close the #ifdef produced in gen_outfiles(). */
	fputs("#endif\n", fh);
	put_db(df);
	fflush(fc);
	fflush(fh);
	if (ferror(fc) || ferror(fh)) {
		fprintf(stderr, "%s: Write error\n", ProgName);
		exit(1);
	}
	fclose(fc);
	fclose(fh);
}

#ifdef DEBUG
static void
LexScan()
{
	t_token	*tkp = &dot;

	while (LLlex() > 0) {
		printf(">>> %s ", symbol2str(tkp->tk_symb));
		switch(tkp->tk_symb) {

		case IDENT:
			printf("%s\n", tkp->tk_idf->id_text);
			break;

		case INTEGER:
			printf("%ld\n", tkp->tk_int);
			break;

		case REAL:
			printf("%s\n", tkp->tk_real->r_real);
			break;

		case STRING:
			printf("\"%s\"\n", tkp->tk_string->s_str);
			break;

		default:
			printf("\n");
		}
	}
}
#endif

static struct stdproc stdprocs_uc[] = {
	{ "ABS",	S_ABS },
	{ "CAP",	S_CAP },
	{ "CHR",	S_CHR },
	{ "FLOAT",	S_FLOAT },
	{ "VAL",	S_VAL },
	{ "TRUNC",	S_TRUNC },
	{ "ORD",	S_ORD },
	{ "ODD",	S_ODD },
	{ "MAX",	S_MAX },
	{ "MIN",	S_MIN },
	{ "NCPUS",	S_NCPUS },
	{ "MYCPU",	S_MYCPU },
	{ "ASSERT",	S_ASSERT },
	{ "SIZE",	S_SIZE },
	{ "LB",		S_LB },
	{ "UB",		S_UB },
	{ "FROM",	S_FROM },
	{ "ADDNODE",	S_ADDNODE },
	{ "DELETENODE",	S_DELETENODE },
	{ "DELETE",	S_DELETE },
	{ "INSERT",	S_INSERT },
	{ 0,		0 }
};

static struct stdproc stdprocs[] = {
	{ "Read",	S_READ },
	{ "Write",	S_WRITE },
	{ "WriteLine",	S_WRITELN },
	{ "Strategy",	S_STRATEGY },
	{ 0,		0 }
};

static struct stdproc stdprocs_lc[] = {
	{ "abs",	S_ABS },
	{ "cap",	S_CAP },
	{ "chr",	S_CHR },
	{ "float",	S_FLOAT },
	{ "val",	S_VAL },
	{ "trunc",	S_TRUNC },
	{ "ord",	S_ORD },
	{ "odd",	S_ODD },
	{ "max",	S_MAX },
	{ "min",	S_MIN },
	{ "ncpus",	S_NCPUS },
	{ "mycpu",	S_MYCPU },
	{ "assert",	S_ASSERT },
	{ "size",	S_SIZE },
	{ "lb",		S_LB },
	{ "ub",		S_UB },
	{ "from",	S_FROM },
	{ "addnode",	S_ADDNODE },
	{ "deletenode",	S_DELETENODE },
	{ "delete",	S_DELETE },
	{ "insert",	S_INSERT },
	{ 0,		0 }
};

static void
add_procs(p)
	struct stdproc
		*p;
{
	for (; p->st_nam != 0; p++) {
		t_def	*df;

		df = define(str2idf(p->st_nam, 0), CurrentScope, D_FUNCTION);
		df->df_type = std_type;
		df->df_stdname = p->st_con;
		df->df_flags |= D_DEFINED;
	}
}

static void
add_standards()
{
	t_def	*df;

	add_procs(stdprocs);
	if (!options['k']) {
		add_procs(stdprocs_uc);
	}
	if (options['k'] || options['K']) {
		add_procs(stdprocs_lc);
	}
	if (! options['k']) {
		df = define(str2idf("NIL", 0), CurrentScope, D_CONST);
		df->df_type = nil_type;
		df->df_flags |= D_DEFINED;
		df->con_const = mk_leaf(Value, INTEGER);
		df->con_const->nd_int = 0;
		df->con_const->nd_type = nil_type;
	}
	if (options['k'] || options['K']) {
		df = define(str2idf("nil", 0), CurrentScope, D_CONST);
		df->df_type = nil_type;
		df->df_flags |= D_DEFINED;
		df->con_const = mk_leaf(Value, INTEGER);
		df->con_const->nd_int = 0;
		df->con_const->nd_type = nil_type;
	}
}

void
No_Mem()
{
	fatal("out of memory");
}

static void
gen_module_names(df)
	t_def	*df;
{
	t_dflst	dl;

	def_enlist(&(df->mod_imports), df);
	def_endlist(&(df->mod_imports));
	def_walklist(df->mod_imports, dl, df) {
		CurrentScope = df->bod_scope;
		if (! df->mod_name) {
		    if (df->df_flags & D_MAIN) {
			df->mod_name = mk_str("ini_", "OrcaMain", (char *) 0);
		    }
		    else df->mod_name = gen_name("ini_", df->df_idf->id_text, 0);
		}
	}
}

static void
test_orca_generated(fn)
	char	*fn;
{
	FILE	*f = fopen(fn, "r");
	char	buf[80];

	if (f == NULL) return;
	if (fgets(buf, 80, f) == 0) return;
	if (! strcmp(buf, OrcaString)) return;
	fatal("%s exists and is not produced by the Orca compiler", fn);
}

extern char	*Version;

static void
gen_outfiles(df)
	t_def	*df;
{
	t_dflst l;
	t_def	*mdef;
	int	generic = df->df_flags & D_GENERIC;
	char	*dsth, *dstc;

	/* Determine output file names. */
	if (options['N'] || err_occurred) {
		dsth = "/dev/null";
		dstc = dsth;
	}
	else {
		char *hsuf = generic ? ".gh" : ".h";
		char *csuf = generic ? ".gc" : ".c";
		unsigned int len = strlen(df->mod_file)+4;

		dsth = Malloc(len);
		strcpy(dsth, df->mod_file);
		strcat(dsth, hsuf);
		dstc = Malloc(len);
		strcpy(dstc, df->mod_file);
		strcat(dstc, csuf);
	}

	/* Open output file names. */
	test_orca_generated(dsth);
	test_orca_generated(dstc);
	fh = fopen(dsth, "w");
	fc = fopen(dstc, "w");
	if (fh == NULL || fc == NULL) {
		fatal("Could not open output file");
	}
	fprintf(fh, "%s\n/* %s. */\n\n", OrcaString, Version);
	fprintf(fc, "%s\n/* %s. */\n\n", OrcaString, Version);

	if (! options['N'] && ! err_occurred) {
		free(dsth);
		free(dstc);
	}
	if (! (CurrDef->df_flags & D_GENERIC)) {
		fputs("#include <interface.h>\n", fc);
		fprintf(fc, "#include \"%s.h\"\n", df->mod_file);
	}
	fprintf(fh, "#ifndef %s____SEEN\n", df->df_idf->id_text);
	fprintf(fh, "#define %s____SEEN\n", df->df_idf->id_text);

	def_walklist(df->mod_hincludes, l, mdef) {
		if (!(mdef->df_flags & D_GENERIC)) {
			fprintf(fh, "#include \"%s.h\"\n", mdef->mod_file);
		}
	}
	def_walklist(df->mod_cincludes, l, mdef) {
		if (!(mdef->df_flags & D_GENERIC)
		    && ! t_dflstmember(df->mod_hincludes, mdef)) {
			fprintf(fc, "#include \"%s.h\"\n", mdef->mod_file);
		}
	}
	if ((df->df_flags & D_INOUT_NEEDED) &&
	    ! (df->df_flags & D_INOUT_DONE)) {
		fprintf(fc, "#include \"InOut.h\"\n");
	}
}

static void
gen_dependencies(depdef)
	t_def	*depdef;
{
	FILE	*f = stdout;
	t_dflst	l;
	t_def	*df;

	if (! options['A'] || ! depdef) return;
	if (dep_filename) {
		f = fopen(dep_filename, "w");
		if (! f) fatal("could not open %s", dep_filename);
	}
	def_walklist(depdef->mod_imports, l, df) {
		if (options['m']) {
			fprintf(f, "%s.%sc:\t%s/%s.spf\n",
				depdef->df_idf->id_text,
				depdef->df_flags & D_GENERIC ? "g" : "",
				df->mod_dir[0] != '\0' ? df->mod_dir : ".",
				df->mod_file);
			if (df == depdef) continue;
			fprintf(f, "%s.%sc:\t%s/.%s.db\n",
				depdef->df_idf->id_text,
				depdef->df_flags & D_GENERIC ? "g" : "",
				df->mod_dir[0] != '\0' ? df->mod_dir : ".",
				df->mod_file);
		}
		else {
			fprintf(f, "%s/%s.spf\n",
				df->mod_dir[0] != '\0' ? df->mod_dir : ".",
				df->mod_file);
			if (df == depdef) continue;
			fprintf(f, "%s/.%s.db\n",
				df->mod_dir[0] != '\0' ? df->mod_dir : ".",
				df->mod_file);
		}
	}
	if (dep_filename) fclose(f);
}

static void
end_lists(df)
	t_def	*df;
{
	def_endlist(&(df->mod_funcaddrs));
	def_endlist(&(df->mod_reductionfuncs));
	def_endlist(&(df->mod_hincludes));
	def_endlist(&(df->mod_cincludes));
}
