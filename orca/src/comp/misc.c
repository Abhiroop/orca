/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* M I S C E L L A N E O U S	R O U T I N E S */

/* $Id: misc.c,v 1.6 1997/05/15 12:02:28 ceriel Exp $ */

#include	"debug.h"
#include	"ansi.h"

#if __STDC__
#include	<stdarg.h>
#else
#include	<varargs.h>
extern char	*strrchr();
#endif

#include	<stdio.h>
#include	<alloc.h>

#include	"misc.h"
#include	"f_info.h"
#include	"error.h"
#include	"flexarr.h"

t_idf *
gen_anon_idf()
{
	/*	A new idf is created out of nowhere, to serve as an
		anonymous name.
	*/
	static int
		name_cnt;
	char	buff[512];

	(void) sprintf(buff, "#%d on line %u",
			++name_cnt, LineNumber);
	return str2idf(buff, 1);
}

#if __STDC__
char *
mk_str	(
	char	*str,
	...
	)
{
#else
char *
mk_str(va_alist)
	va_dcl
{
	char	*str;
#endif
	va_list	ap;
	p_flex	f = flex_init(sizeof(char), 1024);
	char	*p;

#if __STDC__
	va_start(ap, str);
#else
	va_start(ap);
	str = va_arg(ap, char *);
#endif
	p = flex_next(f);
	if (str) {
		do {
			while (*str) {
				*p = *str++;
				p = flex_next(f);
			}
			str = va_arg(ap, char *);
		} while (str);
	}
	*p = 0;
	va_end(ap);
	return flex_finish(f, (uint *) 0);
}

char *
get_dirname(fn)
	char	*fn;
{
	char	*p;

	while ((p = strrchr(fn,'/')) && *(p + 1) == '\0') {
		/* remove trailing /'s */
		*p = '\0';
	}

	if (p) {
		*p = '\0';
		fn = Salloc(fn, (unsigned) (p - &fn[0] + 1));
		*p = '/';
		return fn;
	}
	return "";
}

char *
get_basename(nam)
	char	*nam;
{
	char	*p = strrchr(nam, '.');
	char	*f = p;
	char	*r;

	while (f > nam && *f != '/') f--;
	if (*f == '/') f++;

	r = Salloc(f, (unsigned) (p - f + 1));
	r[p-f] = '\0';
	return r;
}
