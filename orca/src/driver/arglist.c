/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: arglist.c,v 1.7 1997/04/02 09:10:23 ceriel Exp $ */

/*   A R G U M E N T   L I S T   H A N D L I N G   */

#include "ansi.h"

#include <stdio.h>
#include <ctype.h>
#include <alloc.h>

#include "arglist.h"
#include "defaults.h"

void
append(arg, ap)
	char		*arg;
	struct arglist	*ap;
{
	if (ap->maxargc == ap->argc) {
		ap->maxargc += 10;
		ap->args = (char **)
			Realloc((char *)(ap->args), (ap->maxargc+2) * sizeof(char *));
	}
	ap->args[++(ap->argc)] = arg;
	ap->args[ap->argc+1] = 0;
}

void
split_and_append(str, ap)
	char	*str;
	struct arglist	*ap;
{
	/*	Split a space-separated string into pieces and add the
		pieces to the arglist indicated by ap.
	*/

	register char *p;
	register char *new;

	if (! str) return;
	for (;;) {
		int len = 0;
		p = str;
		while (*p && ! isspace(*p)) {
			if (*p == '$') {
				char *id = ++p;
				if (*p == '_' || isalpha(*p)) {
					char *s;
					char buf[512];
					int i = 0;

					while (*p == '_' || isalnum(*p)) {
						buf[i++] = *p;
						if (i >= 511) break;
						p++;
					}
					buf[i] = '\0';
					s = get_value(buf);
					if (s) {
						len += strlen(s);
						continue;
					}
				}
				p = id-1;
			}
			len++;
			p++;
		}
		if (p == str) {
			if (! *p) break;
			str++;
			continue;
		}
		new = Malloc(len + 1);
		new[len] = '\0';
		p = str;
		len = 0;
		while (*p && ! isspace(*p)) {
			if (*p == '$') {
				char *id = ++p;
				if (*p == '_' || isalpha(*p)) {
					char *s;
					char buf[512];
					int i = 0;

					while (*p == '_' || isalnum(*p)) {
						buf[i++] = *p;
						if (i >= 511) break;
						p++;
					}
					buf[i] = '\0';
					s = get_value(buf);
					if (s) {
						strcpy(&new[len], s);
						len += strlen(s);
						continue;
					}
				}
				p = id-1;
			}
			new[len++] = *p++;
		}
		append(new, ap);
		while (isspace(*p)) p++;
		if (! *p) break;
		str = p;
	}
}

void
pr_vec(vec, f, termch)
	struct arglist *vec;
	FILE	*f;
	int	termch;
{
	register char **ap = &vec->args[1];

	while (*ap) {
		fprintf(f, "%s", *ap);
		ap++;
		if (*ap) putc(' ', f);
	}
	putc(termch, f);
	fflush(f);
}

int
append_unique(f, args)
	char		*f;
	struct arglist	*args;
{
	/*	Add file "f" to the argument list "args", but only if it
		is not present yet.
	*/

	register int i = args->argc;
	register char **srcp = &args->args[1];

	while (i > 0) {
		if (! strcmp(*srcp, f)) {
			return 0;
		}
		i--;
		srcp++;
	}
	append(f, args);
	return 1;
}
