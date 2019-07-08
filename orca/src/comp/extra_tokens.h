/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __EXTRA_TOKENS_H__
#define __EXTRA_TOKENS_H__

/* $Id: extra_tokens.h,v 1.16 1997/05/15 12:01:59 ceriel Exp $ */

#include	"Lpars.h"

/* Extra tokens, used internally. */

#define ORBECOMES	(LL_MAXTOKNO+1)
#define ANDBECOMES	(LL_MAXTOKNO+2)
#define A_CHECK		(LL_MAXTOKNO+3)
#define U_CHECK		(LL_MAXTOKNO+4)
#define G_CHECK		(LL_MAXTOKNO+5)
#define INIT		(LL_MAXTOKNO+6)
#define ALIAS_CHK	(LL_MAXTOKNO+7)
#define TMPBECOMES	(LL_MAXTOKNO+8)
#define ARR_INDEX	(LL_MAXTOKNO+9)
#define ARR_SIZE	(LL_MAXTOKNO+10)
#define LSHBECOMES	(LL_MAXTOKNO+11)
#define RSHBECOMES	(LL_MAXTOKNO+12)
#define FOR_UPDATE	(LL_MAXTOKNO+13)
#define CHECK		(LL_MAXTOKNO+14)
#define UPDATE		(LL_MAXTOKNO+15)
#define COND_EXIT	(LL_MAXTOKNO+16)
#define ALIASBECOMES	(LL_MAXTOKNO+17)
#define FROM_CHECK	(LL_MAXTOKNO+18)
#define DIV_CHECK	(LL_MAXTOKNO+19)
#define MOD_CHECK	(LL_MAXTOKNO+20)
#define CPU_CHECK	(LL_MAXTOKNO+21)
#endif /* __EXTRA_TOKENS_H__ */
