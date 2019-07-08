/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* E R R O R   A N D   D I A G N O S T I C   R O U T I N E S */

/* $Id: error.c,v 1.10 1997/05/15 12:01:56 ceriel Exp $ */

/*	This file contains the error-message and diagnostic giving functions.
	Be aware that they are called with a variable number of arguments!
*/

#include	"ansi.h"
#include	"errout.h"
#include	"debug.h"

#include	<stdio.h>

#include	"error.h"
#include	"f_info.h"
#include	"options.h"

/* error classes */
#define	ERROR		1
#define	WARNING		2
#define	LEXERROR	3
#define	LEXWARNING	4
#define	CRASH		5
#define	FATAL		6
#ifdef DEBUG
#define VDEBUG		7
#endif

_PROTOTYPE(static void _error, (int, t_pos *, char *, va_list));

int err_occurred;

/*	There are three general error-message functions:
		lexerror()	lexical and pre-processor error messages
		error()		syntactic and semantic error messages
		pos_error()	errors with given position
	The difference lies in the place where the file name and line
	number come from.
	Lexical errors report from the global variables LineNumber and
	FileName, pos errors get their information from the position supplied,
	whereas other errors use the information in the token.
*/

#if __STDC__
#ifdef DEBUG
/*VARARGS1*/
void
debug	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(VDEBUG, NULLPOS, fmt, ap);
	}
	va_end(ap);
}
#endif /* DEBUG */

/*VARARGS1*/
void
error	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(ERROR, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS2*/
void
pos_error
	(
	t_pos	*pos,
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(ERROR, pos, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS1*/
void
warning	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(WARNING, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS2*/
void
pos_warning
	(
	t_pos	*pos,
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(WARNING, pos, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS1*/
void
lexerror
	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(LEXERROR, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS1*/
void
lexwarning
	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(LEXWARNING, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS1*/
void
fatal	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(FATAL, NULLPOS, fmt, ap);
	}
	va_end(ap);
	exit(1);
}

/*VARARGS1*/
void
crash	(
	char	*fmt,
	...
	)
{
	va_list	ap;

	va_start(ap, fmt);
	{
		_error(CRASH, NULLPOS, fmt, ap);
	}
	va_end(ap);
#ifdef DEBUG
	abort();
#else
	exit(1);
#endif
}
#else	/* if not __STDC__ */
#ifdef DEBUG
/*VARARGS*/
void
debug(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(VDEBUG, NULLPOS, fmt, ap);
	}
	va_end(ap);
}
#endif /* DEBUG */

/*VARARGS*/
void
error(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(ERROR, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
pos_error(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		t_pos	*pos = va_arg(ap, t_pos *);
		char	*fmt = va_arg(ap, char *);

		_error(ERROR, pos, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
warning(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(WARNING, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
pos_warning(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		t_pos	*pos = va_arg(ap, t_pos *);
		char	*fmt = va_arg(ap, char *);

		_error(WARNING, pos, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
lexerror(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(LEXERROR, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
lexwarning(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(LEXWARNING, NULLPOS, fmt, ap);
	}
	va_end(ap);
}

/*VARARGS*/
void
fatal(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(FATAL, NULLPOS, fmt, ap);
	}
	va_end(ap);
	exit(1);
}

/*VARARGS*/
void
crash(va_alist)
	va_dcl
{
	va_list	ap;

	va_start(ap);
	{
		char	*fmt = va_arg(ap, char *);

		_error(CRASH, NULLPOS, fmt, ap);
	}
	va_end(ap);
#ifdef DEBUG
	abort();
#else
	exit(1);
#endif
}
#endif /* not __STDC__ */

static void
_error(class, pos, fmt, ap)
	int	class;
	t_pos	*pos;
	char	*fmt;
	va_list	ap;
{
	/*	_error attempts to limit the number of error messages
		for a given line to MAXERR_LINE.
	*/
	unsigned int
		ln = 0;
	char	*remark = 0;
	char	*fn = 0;
	FILE	*f = ERROUT;

#ifndef DEBUG
	/* check visibility of message */
	if (class == ERROR || class == WARNING) {
		if (token_nmb < tk_nmb_at_last_syn_err + ERR_SHADOW)
			/* warning or error message overshadowed */
			return;
	}
#endif
	/*	Since name and number are gathered from different places
		depending on the class, we first collect the relevant
		values and then decide what to print.
	*/
	/* preliminaries */
	switch (class)	{
	case ERROR:
	case LEXERROR:
	case CRASH:
	case FATAL:
		err_occurred = 1;
		break;
	}

	/* the remark */
	switch (class)	{
	case WARNING:
	case LEXWARNING:
		if (options['w']) return;
		remark = "(warning)";
		break;
	case CRASH:
		remark = "CRASH\007";
		break;
	case FATAL:
		remark = "fatal error --";
		break;
#ifdef DEBUG
	case VDEBUG:
		if (! options['d']) return;
		remark = "(debug)";
		break;
#endif /* DEBUG */
	}

	/* the place */
	switch (class)	{
	case WARNING:
	case ERROR:
		ln = pos ? pos->pos_lineno : dot.tk_pos.pos_lineno;
		fn = pos ? pos->pos_filename : dot.tk_pos.pos_filename;
		break;
	case LEXWARNING:
	case LEXERROR:
	case FATAL:
		ln = LineNumber;
		fn = FileName;
		break;
	case CRASH:
#ifdef DEBUG
	case VDEBUG:
#endif /* DEBUG */
		ln = LineNumber;
		fn = FileName;
		f = stdout;
		break;
	}

	if (fn) fprintf(f, "\"%s\", line %u: ", fn, ln);

	if (remark) fprintf(f, "%s ", remark);

	vfprintf(f, fmt, ap);		/* contents of error */
	fprintf(f, "\n");
}
