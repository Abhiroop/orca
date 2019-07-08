/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: closure.c,v 1.29 1997/11/06 08:40:04 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

#include	<stdio.h>
#include	<assert.h>
#include	<alloc.h>

#include	"closure.h"
#include	"idf.h"
#include	"scope.h"
#include	"node.h"
#include	"type.h"
#include	"error.h"
#include	"options.h"
#include	"main.h"
#include	"class.h"
#include	"generate.h"
#include	"flexarr.h"

/* Tunable constant. */
#define LOOPFACTOR	16

#define	XNONE	0
#define	XARG	1
#define XOBJECT	2
#define XLAST	3

struct obj {
	struct obj *o_next;
	int	o_kind;
	t_def	*o_df;
	int	o_parno;
	t_def	*o_process;
	double	o_score;
	double	o_accesses;
	double	o_uncertainty;
};

/* Object table: */
static struct obj *obj_list;

/* types of patterns: */

#define ALT	0	/* alternative (if, case, etc) */
#define SEQ	1	/* sequence (for, while, etc) */
#define INV	2	/* object invocation */
#define CALL	3	/* procedure call */
#define PFORK	4	/* process fork */

struct alternative {
	struct alternative *a_next;
	double a_score;
	struct pattern *a_alts;
};

struct sequence {
	struct pattern *s_seq;
};

struct invocation {
	int i_IsRead;
	int i_IsArg;
	t_def *i_df;
	int i_parno;
	t_def *i_process;
};

struct call {	/* also used for fork */
	t_def *c_procname;
	char  *c_name;
	struct obj *c_args;
};

struct pattern {
	int p_kind;
	struct pattern *p_next;
	union {
		struct alternative	*p_alt;
		struct sequence		p_seq;
		struct invocation	p_inv;
		struct call		p_call;
	} p_val;
};

#define p_alts		p_val.p_alt
#define p_sequence	p_val.p_seq.s_seq
#define p_IsRead	p_val.p_inv.i_IsRead
#define p_IsArg		p_val.p_inv.i_IsArg
#define p_df		p_val.p_inv.i_df
#define p_parno		p_val.p_inv.i_parno
#define p_process	p_val.p_inv.i_process
#define p_procname	p_val.p_call.c_procname
#define p_name		p_val.p_call.c_name
#define p_args		p_val.p_call.c_args

_PROTOTYPE(static t_def *get_def,	(char **));
_PROTOTYPE(static t_def *find_pardef,	(int));
_PROTOTYPE(static t_def *get_procdef,	(char *, char *));
_PROTOTYPE(static int get_num,		(char **));
_PROTOTYPE(static struct pattern *malloc_pattern,
					(void));
_PROTOTYPE(static void get_args,	(struct obj *, char **));
_PROTOTYPE(static struct pattern *get_patterns,
					(char **));
_PROTOTYPE(static void store_pat,	(t_def *));
_PROTOTYPE(static void clean_pat,	(t_def *));
_PROTOTYPE(static void cleanup_pat,	(struct pattern *));
_PROTOTYPE(static void handle_process,	(t_def *));
_PROTOTYPE(static void handle_funcs,	(t_def *));
_PROTOTYPE(static void finish_funcs,	(t_def *));
_PROTOTYPE(static void enter_object,	(t_def *, t_def *));
_PROTOTYPE(static void subst_args,	(struct call *, struct obj *));
_PROTOTYPE(static struct pattern *inline_patterns,
					(struct pattern *, struct obj *, int));
_PROTOTYPE(static void do_analysis,	(t_def *, struct pattern *));
_PROTOTYPE(static void analyse,		(struct pattern *, t_def *, t_def *,
					 double *, double *, double *));
_PROTOTYPE(static void gen_pattern,	(struct pattern *));
_PROTOTYPE(static void gen_alts,	(struct alternative *));
_PROTOTYPE(static void gen_args,	(struct obj *));
#ifdef DEBUG
_PROTOTYPE(static void print_pat,	(struct pattern *));
_PROTOTYPE(static void print_alts,	(struct alternative *));
_PROTOTYPE(static void print_args,	(struct obj *));
#endif

_PROTOTYPE(double fabs, (double));

#if ! defined(__STDC__) || __STDC__ == 0
extern char *memset(), *memcpy();
#endif

static t_def *current_process;

static struct pattern *
malloc_pattern()
{
	struct pattern
		*pat;

	pat = (struct pattern *) Malloc(sizeof(struct pattern));
	memset((char *) pat, 0, sizeof(struct pattern));
	return pat;
}

static t_def *
get_def(pbuf)
	char	**pbuf;
{
	char	*buf = *pbuf;
	int	sv;
	t_idf	*id;

	while (in_idf(*buf)) buf++;
	sv = *buf;
	*buf = 0;
	id = str2idf(*pbuf, 1);
	*buf = sv;
	*pbuf = buf;
	return lookup(id, current_process->bod_scope, 0);
}

static int
get_num(pbuf)
	char	**pbuf;
{
	char	*buf = *pbuf;
	int	retval = 0;

	while (is_dig(*buf)) {
		retval = retval * 10 + (*buf++ - '0');
	}
	*pbuf = buf;
	return retval;
}

static t_def *
find_pardef(parno)
	int	parno;
{
	t_dflst	l;
	t_def	*df;

	assert(parno > 0);
	def_walklist(current_process->df_type->prc_params, l, df) {
		if (--parno == 0) return df;
	}
	assert(0);
	return (t_def *) 0;
}

static t_def *
get_procdef(pb, pe)
	char	*pb, *pe;
{
	char	*p = pb;
	t_def	*df;

	while (*p != '.') {
		assert(*p);
		p++;
	}
	*p = '\0';
	*pe = '\0';
	df = lookup(str2idf(pb, 1), GlobalScope, 0);
	if (df) {
	   df = lookup(str2idf(p+1,1), df->bod_scope, 0);
	}
	*p = '.';
	*pe = '(';
	return df;
}

static void
get_args(argtab, bufp)
	struct obj
		*argtab;
	char	**bufp;
{
	char	*buf = *bufp;

	while (1) {
		switch(*buf) {
		case ')':
			argtab->o_kind = XLAST;
			*bufp = buf+1;
			return;
		case '*':
		case '?':
			argtab->o_kind = XNONE;
			buf++;
			if (*buf == ',') buf++;
			break;
		case '@':
			argtab->o_kind = XOBJECT;
			buf++;
			argtab->o_process = current_process;
			argtab->o_df = get_def(&buf);
			enter_object(argtab->o_process, argtab->o_df);
			if (*buf == ',') buf++;
			break;
		case '#':
			argtab->o_kind = XARG;
			buf++;
			argtab->o_parno = get_num(&buf);
			argtab->o_df = find_pardef(argtab->o_parno);
			if (current_process->df_kind == D_PROCESS) {
				argtab->o_process = current_process;
				enter_object(argtab->o_process, argtab->o_df);
			}
			if (*buf == ',') buf++;
			break;
		}
		argtab++;
	}
}

static struct pattern *
get_patterns(bufp)
	char	**bufp;
{
	char	*buf = *bufp,
		*p;
	struct pattern
		*pat;
	struct alternative
		**altp;

	switch (*buf) {
	case ']':
	case '}':
	case '|':
	case '\0':
		return (struct pattern *) 0;
	}

	pat =  malloc_pattern();

	switch (*buf) {
	case '[':
		pat->p_kind = ALT;
		buf++;
		altp = &pat->p_alts;
		while (1) {
			*altp = (struct alternative *) Malloc(sizeof(struct alternative));
			(*altp)->a_alts = get_patterns(&buf);
			(*altp)->a_next = 0;
			altp = &((*altp)->a_next);
			if (*buf == ']') break;
			assert (*buf == '|');
			buf++;
		}
		buf++;
		break;
	case '{':
		pat->p_kind = SEQ;
		buf++;
		pat->p_sequence = get_patterns(&buf);
		assert(*buf == '}');
		buf++;
		break;
	case 'f':
	case 'p':
		pat->p_kind = (*buf == 'f' ? CALL : PFORK);
		buf += 2;
		p = buf;
		while (*buf != '(') buf++;
		pat->p_procname = get_procdef(p, buf);
		pat->p_name = Malloc(buf - p + 1);
		strncpy(pat->p_name, p, buf-p);
		pat->p_name[buf-p] = '\0';
		if (! pat->p_procname) {
			while (*buf != ')') buf++;
			buf++;
		}
		else {
			pat->p_args = (struct obj *)
			   Malloc((unsigned)((pat->p_procname->df_type->prc_nparams+1) * sizeof(struct obj)));
			buf++;
			get_args(&(pat->p_args[0]), &buf);
		}
		break;
	case '@':
	case '#':
		pat->p_kind = INV;
		if (*buf++ == '@') {
			pat->p_df = get_def(&buf);
			pat->p_process = current_process;
		}
		else {
			pat->p_IsArg = 1;
			pat->p_parno = get_num(&buf);
			pat->p_df = find_pardef(pat->p_parno);
		}

		/* if we've just scanned a shared (object) parameter of
		 * a process, set pat->p_process, causing the parameter object
		 * to be entered in the object table
		 */
		if(pat->p_IsArg && current_process->df_kind == D_PROCESS) {
			pat->p_process = current_process;
		}
		assert(*buf == '$');
		buf++;
		assert(*buf == 'R' || *buf == 'W');
		pat->p_IsRead = (*buf == 'R');
		buf++;
		assert(*buf == '(');
		buf++;
		/* skip over arguments, must be '*'s */
		while (*buf != ')' ) {
			assert(*buf == '*' || *buf == ',' || *buf == '?');
			buf++;
		}
		buf++;
		if (pat->p_process) {
			enter_object(pat->p_process, pat->p_df);
		}
		break;
	  default:
		crash("get_patterns");
	}
	pat->p_next = get_patterns(&buf);
	*bufp = buf;
	return pat;
}

static void
store_pat(df)
	t_def	*df;
{
	char	*buf;

	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		walkdefs(df->bod_scope->sc_def, D_PROCESS|D_FUNCTION,
			store_pat);
		break;
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		/* Fall through */
	case D_PROCESS:
		buf = df->prc_patstr;
		current_process = df;
		if (buf && *buf) {
			df->prc_pattern = get_patterns(&buf);
			DO_DEBUG(options['P'],
				 (printf("Pattern of %s: ", df->df_idf->id_text),
				 print_pat(df->prc_pattern),
				 printf("\n\n")));
		}
		else {
			DO_DEBUG(options['P'],
				(printf("no pattern for %s\n", df->df_idf->id_text)));
		}
		break;
	}
}

static void
clean_pat(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		walkdefs(df->bod_scope->sc_def, D_PROCESS|D_FUNCTION,
			clean_pat);
		break;
	case D_FUNCTION:
		if (df->df_flags & D_GENERICPAR) break;
		/* Fall through */
	case D_PROCESS:
		if (df->prc_pattern) {
			cleanup_pat(df->prc_pattern);
			df->prc_pattern = 0;
		}
		break;
	}
}

static void
cleanup_pat(pat)
	struct pattern
		*pat;
{
	struct pattern
		*next;

	for (; pat; pat = next) {
		next = pat->p_next;
		switch(pat->p_kind) {
		case ALT: {
				struct alternative *a, *n;

				for (a = pat->p_alts; a; a = n) {
					n = a->a_next;
					cleanup_pat(a->a_alts);
					free((char *) a);
				}
			}
			break;
		case CALL:
		case PFORK:
			if (pat->p_name) free(pat->p_name);
			if (pat->p_args) free((char *) (pat->p_args));
			break;

		case SEQ:
			cleanup_pat(pat->p_sequence);
			break;
		}
		free((char *) pat);
	}
}

static void
handle_process(df)
	t_def	*df;
{
	struct pattern
		*p;

	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		if (df->df_flags & D_IMPL_SEEN) {
		    walkdefs(df->bod_scope->sc_def, D_PROCESS, handle_process);
		}
		break;
	case D_PROCESS:
		if (! df->prc_pattern) break;
		DO_DEBUG(options['P'],
			(printf("Pattern of %s:\n", df->df_idf->id_text),
			 print_pat(df->prc_pattern),
			 printf("\n\n")));
		p = inline_patterns(df->prc_pattern, (struct obj *) 0, 1);
		DO_DEBUG(options['P'],
			(printf("Closure of %s:\n", df->df_idf->id_text),
			 print_pat(p),
			 printf("\n\n")));
		do_analysis(df, p);
		cleanup_pat(p);
		break;
	}
}

static void
handle_funcs(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		if (df->df_flags & D_IMPL_SEEN) {
		    walkdefs(df->bod_scope->sc_def, D_FUNCTION, handle_funcs);
		}
		break;
	case D_FUNCTION:
		if (! df->prc_pattern) break;
		if (! (df->df_flags & D_EXPORTED)) break;
		DO_DEBUG(options['P'],
			(printf("Pattern of %s:\n", df->df_idf->id_text),
			 print_pat(df->prc_pattern),
			 printf("\n\n")));
		df->prc_exp_pattern =
			inline_patterns(df->prc_pattern, (struct obj *) 0, 100);
		DO_DEBUG(options['P'],
			(printf("Closure of %s:\n", df->df_idf->id_text),
			 print_pat(df->prc_exp_pattern),
			 printf("\n\n")));

		break;
	}
}

static p_flex	cf;

#define addchtostr(ch)  do { char *p = flex_next(cf); *p = (ch); } while (0)
_PROTOTYPE(static void addstrtostr, (char *));

static void
addstrtostr(s)
	char	*s;
{
	while (*s) {
		char	*p = flex_next(cf);
		*p = *s;
		s++;
	}
}

static void
gen_pattern(pat)
	struct pattern
		*pat;
{
	int	kind;

	while (pat != 0) {
		kind = pat->p_kind;
		switch (kind) {
		  case ALT:
			addchtostr('[');
			gen_alts(&pat->p_alts[0]);
			addchtostr(']');
			break;
		  case SEQ:
			addchtostr('{');
			gen_pattern(pat->p_sequence);
			addchtostr('}');
			break;
		  case INV:
			if (pat->p_IsArg) {
				char buf[20];
				sprintf(buf, "#%d", pat->p_parno);
				addstrtostr(buf);
			} else {
				addchtostr('@');
				addstrtostr(pat->p_df->df_idf->id_text);
			}
			addchtostr('$');
			if (pat->p_IsRead) {
				addchtostr('R');
			} else {
				addchtostr('W');
			}
			addchtostr('(');
			addchtostr(')');
			break;
		  case CALL:
		  case PFORK:
			addchtostr(kind == CALL ? 'f' : 'p');
			addchtostr('-');
			addstrtostr(pat->p_name);
			addchtostr('(');
			if (pat->p_args) gen_args(pat->p_args);
			addchtostr(')');
			break;
		  default:
			crash("gen_pattern");
		}
		pat = pat->p_next;
	}
}

static void
gen_alts(alts)
	struct alternative
		*alts;
{
	while (alts) {
		gen_pattern(alts->a_alts);
		alts = alts->a_next;
		if (alts) addchtostr('|');
	}
}


static void
gen_args(argtab)
	struct obj
		*argtab;
{
	while (1) {
	   switch (argtab->o_kind) {
		case XLAST:
			return;
		case XNONE:
			addchtostr('*');
			break;
		case XOBJECT:
		case XARG:
			if (argtab->o_kind == XOBJECT) {
				addchtostr('@');
				addstrtostr(argtab->o_df->df_idf->id_text);
			} else {
				char buf[20];
				sprintf(buf, "#%d", argtab->o_parno);
				addstrtostr(buf);
			}
			break;
		}
		argtab++;
		if (argtab->o_kind != XLAST) {
			addchtostr(',');
		}
	}
}

static void
finish_funcs(df)
	t_def	*df;
{
	switch(df->df_kind) {
	case D_MODULE:
	case D_OBJECT:
		if (df->df_flags & D_IMPL_SEEN) {
		    walkdefs(df->bod_scope->sc_def, D_FUNCTION, finish_funcs);
		}
		break;
	case D_FUNCTION:
		if (! df->prc_exp_pattern) break;
		cf = flex_init(sizeof(char), 32);
		gen_pattern(df->prc_exp_pattern);
		addchtostr('\0');
		df->prc_patstr = flex_finish(cf, (unsigned int *) 0);
		cleanup_pat(df->prc_pattern);
		df->prc_pattern = df->prc_exp_pattern;
		break;
	}
}

void
closure_main()
{
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, store_pat);
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, handle_funcs);
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, finish_funcs);
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, handle_process);
	walkdefs(GlobalScope->sc_def, D_MODULE|D_OBJECT, clean_pat);
}

static void
enter_object(process, o)
	t_def	*process, *o;
{
	struct obj
		*optr = obj_list,
		*prev = 0;

	while (optr) {
		if (optr->o_df == o && optr->o_process == process) return;
		prev = optr;
		optr = optr->o_next;
	}
	optr = (struct obj *) Malloc(sizeof(struct obj));
	memset((char *) optr, 0, sizeof(struct obj));
	optr->o_df = o;
	optr->o_process = process;
	if (prev) prev->o_next = optr;
	else obj_list = optr;
}

/***********************************************************
 closure routines
************************************************************/

static void
subst_args(calls, arglist)
	struct call
		*calls;
	struct obj
		*arglist;
{
	struct obj
		*p;

	for (p = &calls->c_args[0]; p->o_kind != XLAST; p++) {
		if (p->o_kind == XARG) {
			struct obj *pa = &arglist[p->o_parno-1];

			p->o_kind = pa->o_kind;
			p->o_df = pa->o_df;
			p->o_process = pa->o_process;
			p->o_parno = pa->o_parno;
		}
	}
}

static struct pattern *
inline_patterns(pat, arglist, recur)
	struct pattern
		*pat;
	struct obj
		arglist[];
	int	recur;
{
	int	newkind;
	struct pattern
		*cp, *inl_body;
	struct alternative
		*ap, **nap;
	int	sz;
	struct obj
		*p;
	struct pattern
		*savpat;
	int	stoprecur;

	if (pat == 0) return 0;
	cp =  malloc_pattern();
	cp->p_kind = pat->p_kind;
	switch (cp->p_kind) {
	case ALT:
		nap = &cp->p_alts;
		ap = pat->p_alts;
		while (ap) {
			*nap = (struct alternative *) Malloc(sizeof(struct alternative));
			(*nap)->a_alts = inline_patterns(ap->a_alts, arglist, recur);
			(*nap)->a_next = 0;
			nap = &((*nap)->a_next);
			ap = ap->a_next;
		}
		break;
	  case SEQ:
		cp->p_sequence = inline_patterns(pat->p_sequence, arglist, recur);
		break;
	  case INV:
		cp->p_val.p_inv = pat->p_val.p_inv;
		if (arglist && cp->p_IsArg) {
			cp->p_df = arglist[cp->p_parno-1].o_df;
			cp->p_process = arglist[cp->p_parno-1].o_process;
			newkind = arglist[cp->p_parno-1].o_kind;
			cp->p_parno = arglist[cp->p_parno-1].o_parno;
			cp->p_IsArg = (newkind == XARG);
		}
		break;
	  case CALL:
		cp->p_val.p_call = pat->p_val.p_call;
		if (pat->p_name) {
			cp->p_name = Malloc(strlen(pat->p_name)+1);
			strcpy(cp->p_name, pat->p_name);
		}
		if (! cp->p_procname) break;
		sz = sizeof(struct obj);

		for (p = pat->p_args;
		     p->o_kind != XLAST;
		     p++) {
			sz += sizeof(struct obj);
		}
		cp->p_args = (struct obj *) Malloc((unsigned) sz);
		memcpy((char *)(cp->p_args), (char *)(pat->p_args), sz);

		if (arglist) subst_args(&cp->p_val.p_call, arglist);
		if (! cp->p_procname->prc_pattern || ! recur) break;
		stoprecur = 0;
		if (! (cp->p_procname->df_scope->sc_def->df_flags & D_IMPL_SEEN)) {
			/* This function is not from the current compilation, so
			   it is already an expanded pattern. In-line it but
			   stop further expansion.
			*/
			stoprecur = 1;
		}
		savpat = cp->p_procname->prc_pattern;

		/* Recursive calls are delt with as follows:
		   The first time a function is expanded, its D_BUSY flag is
		   set. The second time (when the D_BUSY flag is already set),
		   it is expanded but its pattern is set to 0, so that it
		   will not be expanded again.
		*/
		if (! (cp->p_procname->df_flags & D_BUSY)) {
			cp->p_procname->df_flags |= D_BUSY;
		}
		else cp->p_procname->prc_pattern = 0;

		inl_body = inline_patterns(savpat,
					   &cp->p_args[0], stoprecur ? stoprecur :  recur-1);

		if (! cp->p_procname->prc_pattern) {
			cp->p_procname->prc_pattern = savpat;
		}
		else {
			assert(cp->p_procname->df_flags & D_BUSY);
			cp->p_procname->df_flags &= ~D_BUSY;
		}
		free(cp->p_args);
		if (cp->p_name) free(cp->p_name);
		free(cp);
		/* find last element of body */
		for (cp = inl_body; cp->p_next != 0; cp = cp->p_next);
		cp->p_next = inline_patterns(pat->p_next,arglist, recur);
		return inl_body;  /* pointer to first element */

	  case PFORK:
		cp->p_val.p_call = pat->p_val.p_call;
		if (pat->p_name) {
			cp->p_name = Malloc(strlen(pat->p_name)+1);
			strcpy(cp->p_name, pat->p_name);
		}
		if (! cp->p_procname) break;
		{	struct obj *p;
			int sz = sizeof(struct obj);

			for (p = pat->p_args;
			     p->o_kind != XLAST;
			     p++) {
				sz += sizeof(struct obj);
			}
			cp->p_args = (struct obj *) Malloc((unsigned) sz);
			memcpy((char *)(cp->p_args), (char *)(pat->p_args), sz);
		}
		break;
	  default:
		crash("inline_patterns");
	}
	cp->p_next = inline_patterns(pat->p_next, arglist, recur);
	return cp;
}

/* Analyse pattern of given process */
static void
do_analysis(proc, root)
	t_def	*proc;
	struct pattern
		*root;
{
	double	n, w, u;
	struct obj
		*o;

	for (o = obj_list; o; o = o->o_next) {
		if (proc != o->o_process) continue;
		analyse(root, o->o_process, o->o_df, &n, &w, &u);
		if (w > 32768.0) {
			/* scale down ... */
			double scale = w / 32768.0;
			n /= scale;
			w /= scale;
			u /= scale;
		}
		o->o_score = n;
		o->o_accesses = w;
		o->o_uncertainty = u;
		DO_DEBUG(options['P'],
			(printf("Scores for %s@%s", o->o_process->df_idf->id_text,
				o->o_df->df_idf->id_text),
			 printf(" score=%g accesses=%g uncertainty=%g\n",
				n, w, u)));
	}
}

static void
analyse(pat, proc, df, sc, wr, un)
	struct pattern
		*pat;
	t_def	*proc,
		*df;
	double	*sc, *wr, *un;
{
	/* See how object <proc>@<df> is being used within
	   the given pattern;
	*/

	int	kind;
	double	score = 0,
		accesses = 0,
		uncertainty = 0;
	double	subscore, subaccesses;
	int	nalt;
	struct pattern
		*p;
	double	s, acc, unc, average, maxdiff;
	struct alternative
		*altp;

	for (p = pat; p != (struct pattern *) 0; p = p->p_next) {
		kind = p->p_kind;
		switch (kind) {
		case ALT:
			nalt = subscore = subaccesses = 0;
			unc = 0;
			for (altp = p->p_alts; altp; altp = altp->a_next) {
				nalt++;
				analyse(altp->a_alts, proc, df,
				   &s, &acc, &unc);
				altp->a_score = s;	/* remember score */
				subscore += s;
				subaccesses += acc;
			}
			if (nalt >0 ) {
				if (nalt == 1) {
					  /* pattern [pat] actually means
					   * [pat | empty]
					   */
					average = (subscore/2);
					accesses += (subaccesses/2);
					maxdiff = unc; /* retain uncertainty */
				} else {
					average = (subscore / nalt);
					accesses += (subaccesses / nalt);
					/* compute maximum difference between
					 * average and score.
					 */
					maxdiff = 0;
					for (altp = p->p_alts; altp; altp = altp->a_next) {
						if (fabs(average-altp->a_score) > maxdiff) {
							maxdiff = fabs(average-altp->a_score);
						}
					}
				}
				score += average;
				uncertainty += maxdiff;
			}
			break;
		  case SEQ:
			analyse(p->p_sequence, proc, df,
			   &s, &acc, &unc);
			score += LOOPFACTOR * s;
			accesses += LOOPFACTOR * acc;
			uncertainty += LOOPFACTOR * unc;
			break;
		  case INV:
			if (p->p_df == df && proc == p->p_process) {
				if (p->p_IsRead) {
					score += 1.0;
				} else {
					score -= 1.0;
				}
				accesses += 1.0;
			}
			break;
		  case CALL:
			break; /* ignore further (recursive) calls */
		  case PFORK:
			break;
		  default:
			crash("analyse");
		}
	}
	*sc = score;
	*wr = accesses;
	*un = uncertainty;
}


/***********************************************************
 Generate calls to score function
 A call __Score(obj, score) is generated for each shared obect
 declared as local variable of the process with the given name.
************************************************************/

void
gen_score_calls(df)
	t_def	*df;
{
	struct obj
		*obj;

	for (obj = obj_list; obj != 0; obj = obj->o_next) {
		if (df == obj->o_process
		    && (obj->o_df->df_flags & D_SHAREDOBJ)
		    && ! is_shared_param(obj->o_df)) {
			fprintf(fc, "    __Score(%s%s, &%s, (double) %g, (double) %g, (double) %g);\n",
				is_in_param(obj->o_df) ? "" : "&",
				obj->o_df->df_name,
				obj->o_df->df_type->tp_descr,
				obj->o_score,
				obj->o_accesses,
				obj->o_uncertainty);
		}
	}
}

void
gen_erocs_calls(df)
	t_def	*df;
{
	struct obj
		*obj;

	for (obj = obj_list; obj != 0; obj = obj->o_next) {
		if (df == obj->o_process
		    && (obj->o_df->df_flags & D_SHAREDOBJ)
		    && ! is_shared_param(obj->o_df)) {
			fprintf(fc, "    __erocS(%s%s, &%s, (double) %g, (double) %g, (double) %g);\n",
				is_in_param(obj->o_df) ? "" : "&",
				obj->o_df->df_name,
				obj->o_df->df_type->tp_descr,
				obj->o_score,
				obj->o_accesses,
				obj->o_uncertainty);
		}
	}
}

char *
gen_shargs(df)
	t_def	*df;
{
	t_dflst	l;
	t_def	*df1;
	struct obj
		*obj;
	char	*nm = gen_name("pa_", df->df_idf->id_text, 0);

	fprintf(fc, "static sh_args %s[] = {\n", nm);
	def_walklist(df->df_type->prc_params, l, df1) {
		for (obj = obj_list; obj != 0; obj = obj->o_next) {
			if (df == obj->o_process
			    && df1 == obj->o_df
			    && is_shared_param(df1)) {
				fprintf(fc, "\t{ %g, %g, %g }, \n",
					obj->o_score,
					obj->o_accesses,
					obj->o_uncertainty);
				break;
			}
		}
		if (! obj) {
			fprintf(fc, "\t{ 0, 0, 0 },\n");
		}
	}
	fprintf(fc, "\t{ 0, 0, 0 }\n};\n");
	return nm;
}


/***********************************************************
 print routines
************************************************************/

#ifdef DEBUG

static void
print_pat(pat)
	struct pattern
		*pat;
{
	int	kind;

	while (pat != 0) {
		kind = pat->p_kind;
		switch (kind) {
		  case ALT:
			printf("[");
			print_alts(&pat->p_alts[0]);
			printf("]");
			break;
		  case SEQ:
			printf("{");
			print_pat(pat->p_sequence);
			printf("}");
			break;
		  case INV:
			if (pat->p_IsArg) {
				printf("#%d", pat->p_parno);
			} else {
				printf("%s", pat->p_process->df_idf->id_text);
				printf("@");
				if (pat->p_df) printf("%s", pat->p_df->df_idf->id_text);
				else printf("?");
			}
			if (pat->p_IsRead) {
				printf("$R()");
			} else {
				printf("$W()");
			}
			break;
		  case CALL:
		  case PFORK:
			if (! pat->p_procname) {
				printf("<unknown callee>");
			}
			else {
				printf("%s(", pat->p_procname->df_idf->id_text);
				print_args(pat->p_args);
				printf(")");
			}
			break;
		  default:
			crash("print_pat");
		}
		pat = pat->p_next;
	}
}

static void
print_alts(alts)
	struct alternative
		*alts;
{
	while (alts) {
		print_pat(alts->a_alts);
		alts = alts->a_next;
		if (alts) printf("|");
	}
}


static void
print_args(argtab)
	struct obj
		*argtab;
{
	while (1) {
	   switch (argtab->o_kind) {
		case XLAST:
			return;
		case XNONE:
			printf("*");
			break;
		case XOBJECT:
		case XARG:
			if (argtab->o_kind == XOBJECT) {
				printf("%s", argtab->o_process->df_idf->id_text);
				printf("@");
				printf("%s", argtab->o_df->df_idf->id_text);
			} else {
				printf("#%d", argtab->o_parno);
			}
			break;
		}
		argtab++;
		if (argtab->o_kind != XLAST) printf(",");
	}
}

#endif /* DEBUG */
