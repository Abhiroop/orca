/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __F_INFO_H__
#define __F_INFO_H__

/* F I L E   D E S C R I P T O R   S T R U C T U R E */

/* $Id: f_info.h,v 1.6 1997/07/02 14:12:06 ceriel Exp $ */

struct f_info {
	unsigned int
		f_lineno;
	int	f_nestlow;
	char	*f_filename;
	char	*f_workingdir;
};

#define LineNumber	file_info.f_lineno
#define FileName	file_info.f_filename
#define WorkingDir	file_info.f_workingdir
#define nestlow		file_info.f_nestlow

extern struct f_info file_info;
#endif /* __F_INFO_H__ */
