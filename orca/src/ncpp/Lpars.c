/* LLgen generated code from source . */
#include "Lpars.h"
#define LLNOFIRSTS
#if __STDC__ || __cplusplus
#define LL_ANSI_C 1
#endif
#define LL_LEXI LLlex
/* $Id: Lpars.c,v 1.1 1999/10/11 14:23:00 ceriel Exp $ */
#ifdef LL_DEBUG
#include <assert.h>
#include <stdio.h>
#define LL_assert(x)	assert(x)
#else
#define LL_assert(x)	/* nothing */
#endif

extern int LLsymb;

#define LL_SAFE(x)	/* Nothing */
#define LL_SSCANDONE(x)	if (LLsymb != x) LLsafeerror(x)
#define LL_SCANDONE(x)	if (LLsymb != x) LLerror(x)
#define LL_NOSCANDONE(x) LLscan(x)
#ifdef LL_FASTER
#define LLscan(x)	if ((LLsymb = LL_LEXI()) != x) LLerror(x)
#endif

extern unsigned int LLscnt[];
extern unsigned int LLtcnt[];
extern int LLcsymb;

#define LLsdecr(d)	{LL_assert(LLscnt[d] > 0); LLscnt[d]--;}
#define LLtdecr(d)	{LL_assert(LLtcnt[d] > 0); LLtcnt[d]--;}
#define LLsincr(d)	LLscnt[d]++
#define LLtincr(d)	LLtcnt[d]++

#if LL_ANSI_C
extern int LL_LEXI(void);
extern void LLread(void);
extern int LLskip(void);
extern int LLnext(int);
extern void LLerror(int);
extern void LLsafeerror(int);
extern void LLnewlevel(unsigned int *);
extern void LLoldlevel(unsigned int *);
#ifndef LL_FASTER
extern void LLscan(int);
#endif
#ifndef LLNOFIRSTS
extern int LLfirst(int, int);
#endif
#else /* not LL_ANSI_C */
extern LLread();
extern int LLskip();
extern int LLnext();
extern LLerror();
extern LLsafeerror();
extern LLnewlevel();
extern LLoldlevel();
#ifndef LL_FASTER
extern LLscan();
#endif
#ifndef LLNOFIRSTS
extern int LLfirst();
#endif
#endif /* not LL_ANSI_C */
#define LL_SSIZE 6
#define LL_NSETS 4
#define LL_NTERMINALS 45
#if LL_ANSI_C
void LL0_if_expression(void);
#endif
#if LL_ANSI_C
void If_expr(void)
#else
If_expr()
#endif
 {
	unsigned int s[LL_NTERMINALS+LL_NSETS+2];
	LLnewlevel(s);
	LLread();
LL0_if_expression();
	LL_SCANDONE(EOFILE);
	LLoldlevel(s);
}
static char LLsets[] = {
'\20','\0','\0','\20','\16','\0',
'\0','\200','\361','\3','\362','\37',
'\20','\200','\361','\23','\376','\37',
'\20','\200','\361','\123','\376','\37',
0 };
#define LLindex (LL_index+1)
static short LL_index[] = {0,0,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
34,
-1,
-1,
-1,
38,
42,
-1,
28,
29,
36,
39,
32,
33,
-1,
37,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
31,
-1,
40,
-1,
41,
30,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
43,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
44,
-1,
35,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
0,
1,
2,
3,
4,
5,
6,
7,
8,
9,
10,
11,
12,
13,
14,
15,
16,
17,
18,
19,
20,
21,
22,
23,
24,
25,
26,
27,
0 };
#define LL_NEWMESS
/* 
 * Some grammar independent code.
 * This file is copied into Lpars.c.
 */

#ifndef lint
static char *rcsid = "$Id: Lpars.c,v 1.1 1999/10/11 14:23:00 ceriel Exp $";
#endif

unsigned int	LLtcnt[LL_NTERMINALS];
unsigned int	LLscnt[LL_NSETS];
int		LLcsymb, LLsymb;
static int	LLlevel;

#if LL_ANSI_C
#define LL_VOIDCST (void)
void	LLmessage(int);
#else
#define LL_VOIDCST
#endif
#ifdef LL_USERHOOK
#if LL_ANSI_C
static int	LLdoskip(int);
static int	LLuserhook(int, int*);
#else
static int	LLdoskip();
static int	LLuserhook();
#endif
#endif

#ifndef LL_FASTER
#if LL_ANSI_C
void LLscan(int t)
#else
LLscan(t)
	int	t;
#endif
{
	/*
	 * Check if the next symbol is equal to the parameter
	 */
	if ((LLsymb = LL_LEXI()) == t) {
		return;
	}
	/*
	 * If we come here, an error has been detected
	 */
	LLerror(t);
}
#endif

#if LL_ANSI_C
void LLread(void) {
#else
LLread() {
#endif
	for (;;) {
		if ((LLcsymb = LLindex[(LLsymb = LL_LEXI())]) >= 0) return;
		LLmessage(0);
	}
	/* NOTREACHED */
}

#if LL_ANSI_C
void LLerror(int t)
#else
LLerror(t)
	int	t;
#endif
{
	register int i;

	if (t == EOFILE && LLsymb <= 0) return;
#ifdef LL_NEWMESS
	if (t == EOFILE) {
#ifdef LL_USERHOOK
		static int lst[] = { EOFILE, 0 };
		LL_VOIDCST LLuserhook(EOFILE, lst);
#endif /* LL_USERHOOK */
		if (LLsymb != EOFILE && LLsymb > 0) {
			LLmessage(-1);
			while ((LLsymb = LL_LEXI()) > 0 && LLsymb != EOFILE) 
				/* nothing */ ;
		}
		return;
	}
#endif
	if ((LLcsymb = LLindex[LLsymb]) < 0) {
		LLmessage(0);
		LLread();
	}
	i = LLindex[t];
	LLtcnt[i]++;
#ifdef LL_USERHOOK
	LL_VOIDCST LLdoskip(t);
#else
	LL_VOIDCST LLskip();
#endif
	LLtcnt[i]--;
	if (LLsymb != t) LLmessage(t);
}

#if LL_ANSI_C
void LLsafeerror(int t)
#else
LLsafeerror(t)
	int	t;
#endif
{
	if (t == EOFILE && LLsymb <= 0) return;
#ifdef LL_NEWMESS
	if (t == EOFILE) {
#ifdef LL_USERHOOK
		static int lst[] = { EOFILE, 0 };
		LL_VOIDCST LLuserhook(EOFILE, lst);
#endif /* LL_USERHOOK */
		if (LLsymb != EOFILE && LLsymb > 0) {
			LLmessage(-1);
			while ((LLsymb = LL_LEXI()) > 0 && LLsymb != EOFILE) 
				/* nothing */ ;
		}
		return;
	}
#endif
	LLmessage(t);
}

#ifndef LLNOFIRSTS
#if LL_ANSI_C
int LLfirst(int x, int d) {
#else
int LLfirst(x, d) {
#endif
	register int i;

	return (i = LLindex[x]) >= 0 &&
		(LLsets[d + (i >> 3)] & (1 << (i & 07)));
}
#endif

#if LL_ANSI_C
int LLnext(int n)
#else
int LLnext(n)
	int	n;
#endif
{
	/*	returns: 0 if the current symbol is'nt skipped, and it
			 is'nt a member of "n",
			 1 if we have a new symbol, but it is'nt a member,
			 2 if the current symbol is a member,
			 and 3 if we have a new symbol and it is a member.
		So, the low order bit indicates wether we have a new symbol,
		and the next bit indicates wether it is a member of "n".
	*/

	int retval = 0;

	if (LLskip()) retval = 1;
	if (n <= 0 && LLsets[(LLcsymb >> 3) - n] & (1 << (LLcsymb & 07))) {
		retval |= 2;
	}
	else if (n > 0 && LLcsymb == LLindex[n]) retval |= 2;
	return retval;
}

#if LL_ANSI_C
int LLskip(void) {
#else
int LLskip() {
#endif
	/*	returns 0 if the current symbol is'nt skipped, and
		1 if it is, t.i., we have a new symbol
	*/
#ifdef LL_USERHOOK
	return LLdoskip(0);
}

#if LL_ANSI_C
extern void LL_USERHOOK(int, int *);
static int LLuserhook(int e, int *list)
#else
static int LLuserhook(e, list)
	int e;
	int *list;
#endif
{
	int old = LLsymb;
	LL_USERHOOK(e, list);
	LLread();
	return LLsymb != old;
}

#if LL_ANSI_C
static void LLmklist(register int *list)
#else
static LLmklist(list)
	register int *list;
#endif
{
	char Xset[LL_SSIZE];
	register char *p;
	register int i;

	for (p = &Xset[0]; p < &Xset[LL_SSIZE]; ) *p++ = 0;
	for (i = 0; i < LL_NTERMINALS; i++) {
		if (LLtcnt[i] != 0) Xset[i >> 3] |= (1 << (i & 07));
	}
	for (i = LL_NSETS - 1; i >= 0; i--) if (LLscnt[i] != 0) {
		register char *q = &LLsets[LL_SSIZE * i];

		p = &Xset[0];
		while (p < &Xset[LL_SSIZE]) *p++ |= *q++;
	}
	for (i = 0; i < LL_NTERMINALS; i++) {
		if (Xset[i >> 3] & (1 << (i & 07))) {
			*list++ = LLtok[i];
		}
	}
	*list = 0;
}

#if LL_ANSI_C
static int LLdoskip(int e)
#else
static int LLdoskip(e)
	int	e;
#endif
{
	int LLx;
	int list[LL_NTERMINALS+1];
#endif /* LL_USERHOOK */
	register int i;
	int retval;
	int LLi, LLb;

	retval = 0;
#ifdef LL_USERHOOK
	LLmklist(list);
	LLx = LLuserhook(e, list);
	if (LLx) retval = 1;
#endif /* LL_USERHOOK */
	for (;;) {
		if (LLtcnt[LLcsymb] != 0) {
#ifdef LL_USERHOOK
			if (!e || !LLx || LLcsymb == LLindex[e])
#endif
			return retval;
		}
		LLi = LLcsymb >> 3;
		LLb = 1 << (LLcsymb & 07);
		for (i = LL_NSETS - 1; i >= 0; i--) {
			if (LLscnt[i] != 0) {
				if (LLsets[LL_SSIZE*i+LLi] & LLb) {
#ifdef LL_USERHOOK
					if (!e || !LLx || LLcsymb == LLindex[e])
#endif
					return retval;
				}
			}
		}
#ifdef LL_USERHOOK
		if (LLx) {
			LLx = LLuserhook(e, list);
			continue;
		}
#endif /* LL_USERHOOK */
		LLmessage(0);
		retval = 1;
		LLread();
	}
	/* NOTREACHED */
}

#if LL_ANSI_C
void LLnewlevel(unsigned int *LLsinfo) {
#else
LLnewlevel(LLsinfo) unsigned int *LLsinfo; {
#endif
	register int i;

	if (LLlevel++) {
		LLsinfo[LL_NSETS+LL_NTERMINALS] = (unsigned) LLsymb;
		LLsinfo[LL_NSETS+LL_NTERMINALS+1] = (unsigned) LLcsymb;
		for (i = LL_NTERMINALS - 1; i >= 0; i--) {
			LLsinfo[i] = LLtcnt[i];
			LLtcnt[i] = 0;
		}
		for (i = LL_NSETS - 1; i >= 0; i--) {
			LLsinfo[LL_NTERMINALS+i] = LLscnt[i];
			LLscnt[i] = 0;
		}
	}
	LLtincr(0);
}

#if LL_ANSI_C
void LLoldlevel(unsigned int *LLsinfo) {
#else
LLoldlevel(LLsinfo) unsigned int *LLsinfo; {
#endif
	register int i;

	LLtdecr(0);
#ifdef LL_DEBUG
	for (i = 0; i < LL_NTERMINALS; i++) LL_assert(LLtcnt[i] == 0);
	for (i = 0; i < LL_NSETS; i++) LL_assert(LLscnt[i] == 0);
#endif
	if (--LLlevel) {
		for (i = LL_NSETS - 1; i >= 0; i--) {
			LLscnt[i] = LLsinfo[LL_NTERMINALS+i];
		}
		for (i = LL_NTERMINALS - 1; i >= 0; i--) {
			LLtcnt[i] = LLsinfo[i];
		}
		LLsymb = (int) LLsinfo[LL_NSETS+LL_NTERMINALS];
		LLcsymb = (int) LLsinfo[LL_NSETS+LL_NTERMINALS+1];
	}
}
