/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S P E C I F I C A T I O N S */

/* $Id: specfile.c,v 1.24 1998/06/26 07:55:07 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#include	<stdio.h>
#include	<assert.h>
#include	<alloc.h>

#include	"specfile.h"
#include	"input.h"
#include	"scope.h"
#include	"LLlex.h"
#include	"f_info.h"
#include	"main.h"
#include	"misc.h"
#include	"error.h"
#include	"generate.h"
#include	"conditional.h"

int Specification;

#if ! defined(__STDC__) || __STDC__ == 0
extern char	*strcat(), *strncpy();
extern int GenericSpecification();
extern int UnitSpecification();
#else
_PROTOTYPE(void GenericSpecification, (void));
_PROTOTYPE(void UnitSpecification, (void));
#endif

_PROTOTYPE(static int GetFile, (char *));

static int
GetFile(name)
	char	*name;
{
	/*	Try to find a file with basename "name" and extension ".spf",
		in the directories mentioned in "DEFPATH".
	*/
#define BSZ	256
	char	buf[BSZ];

	(void) strncpy(buf, name, BSZ-5);
	buf[BSZ-5] = '\0';			/* maximum length */
	(void) strcat(buf, ".spf");
	DEFPATH[0] = WorkingDir;
	if (! InsertFile(buf, DEFPATH, &(FileName))) {
		/* 14 character file name limit? In that case, we could
		   have 10 characters for the module name.
		*/
		(void) strncpy(buf, name, 10);
		buf[10] = '\0';
		(void) strcat(buf, ".spf");
		if (! InsertFile(buf, DEFPATH, &(FileName))) {
			error("could not find specification for \"%s\"",
				name);
			return 0;
		}
	}
	WorkingDir = get_dirname(FileName);
	LineNumber = 0;
	return 1;
}

t_def *
get_specification(id, is_import, kind, generic, inst_id)
	t_idf	*id,
		*inst_id;
	int	is_import,
		kind,
		generic;
{
	/*	Return a pointer to the "def" structure of the
		module or object specification indicated by "id".
		We may have to read the module specification itself.
		"kind" indicates whether a module or an object is expected.
		"inst_id" is only used when this is an instantiation, for
		name generation.
	*/
	t_def	*df;
	t_scope	*newsc = CurrentScope;
	static int
		level;

	level++;
	df = lookup(id, GlobalScope, D_IMPORTED);
	if (!df) {
		/* Read specification.
		*/
		char	*name;

		if (inst_id) {
			t_def	*sav = CurrDef;
			/* Tricky: we could be compiling a generic, but
			   the instantiation could be from a non-generic,
			   in which case the name is still fixed (may
			   not use _concat).
			*/
			if ((CurrDef->df_flags & D_GENERIC) &&
			    ! (CurrentScope->sc_definedby->df_flags & D_GENERIC)) {
				CurrDef = CurrentScope->sc_definedby;
			}
			name = gen_name("", inst_id->id_text, 0);
			CurrDef = sav;
		}
		else	name = id->id_text;
		open_scope(CLOSEDSCOPE);
		newsc = CurrentScope;
		newsc->sc_name = id->id_text;
		newsc->sc_genname = name;
		Specification++;

		if (!is_anon_idf(id) && GetFile(id->id_text)) {
			t_token	savdot;

			savdot = dot;

			nestlow = nestlevel;
			if (generic) GenericSpecification();
			else {
				UnitSpecification();
			}
			chk_forwards();
			df = lookup(id, GlobalScope, D_IMPORTED);
			if (! df) {
				error("specification for \"%s\" expected",
					id->id_text);
			}
			dot = savdot;
			{
				/* Read ahead and push back to get FileName
				   and Line number right
				*/
				int ch;
				LoadChar(ch);
				if (ch != EOI) PushBack();
			}

			if (df) {
			    if (! (df->df_kind & (D_ERROR|kind))) {
				char *s = (kind & D_MODULE) ?
					"MODULE" :
					"OBJECT";

				fatal("%s \"%s\" should have a %s specification", s, id->id_text, s);
			    }
			    if (!generic && (df->df_flags & D_GENERIC)) {
				if (! is_import) {
					error("non-generic \"%s\" has a generic specification", id->id_text);
				}
				else error("\"%s\" is generic and cannot be imported (from)", id->id_text);
			    }
			}

		}
		else	df = lookup(id, GlobalScope, D_IMPORTED);
		close_scope();
		Specification--;
		if (! df) {
			df = define(id, GlobalScope, D_ERROR);
			df->bod_scope = newsc;
			newsc->sc_definedby = df;
		}
	}
	else if (df->df_flags & D_BUSY) {
		if (! is_anon_idf(id)) error("specification of \"%s\" depends on itself",
			id->id_text);
	}
	else if (is_import && df == CurrDef && level == 1) {
		if (! is_anon_idf(id)) error("it is illegal to import from \"%s\" into itself", id->id_text);
		df->df_kind = D_ERROR;
	}

	if (df && CurrDef) {
		if (df->df_kind != D_ERROR) {
			def_enlist(&(CurrDef->mod_imports), df);
			if (CurrDef->bod_scope == CurrentScope) {
				if (Specification) {
				    def_enlist(&(CurrDef->mod_hincludes), df);
				}
				else {
				    def_enlist(&(CurrDef->mod_cincludes), df);
				}
				if (! strcmp(id->id_text, "InOut")) {
				    CurrDef->df_flags |= D_INOUT_DONE;
				}
			}
		}
	}

	assert(df);
	level--;
	return df;
}
