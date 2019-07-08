/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: error.h,v 1.5 1996/07/04 08:52:05 ceriel Exp $ */

#ifndef __error_h__
#define __error_h__

/* Run-time exceptions: */
#define FROM_EMPTY_SET		0
#define ARRAY_BOUND_ERROR	1
#define LOCAL_DEADLOCK		2
#define BAD_NODENAME		3
#define UNION_ERROR		4
#define BAD_CPU			5
#define CASE_ERROR		6
#define	ALIAS			7
#define FAILED_ASSERT		8
#define FALL_THROUGH		9
#define DIV_ZERO		10
#define RANGE			11
#define ILL_MOD			12

void m_liberr(char *lib, char *s);
void m_syserr(char *s);

#endif /* __error_h__ */
