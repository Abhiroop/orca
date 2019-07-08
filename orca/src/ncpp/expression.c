/* LLgen generated code from source /usr/proj/em/Src/lang/cem/cpp.ansi/expression.g */
#include "Lpars.h"
#define LLNOFIRSTS
#if __STDC__ || __cplusplus
#define LL_ANSI_C 1
#endif
#define LL_LEXI LLlex
/* $Id: expression.c,v 1.1 1999/10/11 14:23:20 ceriel Exp $ */
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
/* $Id: expression.c,v 1.1 1999/10/11 14:23:20 ceriel Exp $ */
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
#define LL_LEXI LLlex
# line 10 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"

#include	"arith.h"
#include	"LLlex.h"

extern arith ifval;
#if LL_ANSI_C
static void LL1_constant_expression(
# line 134 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL2_primary(
# line 24 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL3_constant(
# line 127 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL4_expression(
# line 77 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL5_unary(
# line 31 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL6_unop(
# line 91 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int *oper) ;
static void LL7_binary_expression(
# line 41 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int maxrank ,arith *pval ,int *is_uns) ;
static void LL8_binop(
# line 122 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int *oper) ;
static void LL9_conditional_expression(
# line 55 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL10_assignment_expression(
# line 71 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns) ;
static void LL11_multop(void);
static void LL12_addop(void);
static void LL13_shiftop(void);
static void LL14_relop(void);
static void LL15_eqop(void);
static void LL16_arithop(void);
#else
static LL1_constant_expression();
static LL2_primary();
static LL3_constant();
static LL4_expression();
static LL5_unary();
static LL6_unop();
static LL7_binary_expression();
static LL8_binop();
static LL9_conditional_expression();
static LL10_assignment_expression();
static LL11_multop();
static LL12_addop();
static LL13_shiftop();
static LL14_relop();
static LL15_eqop();
static LL16_arithop();
#endif
#if LL_ANSI_C
void
#endif
LL0_if_expression(
#if LL_ANSI_C
void
#endif
) {
# line 18 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 int is_unsigned = 0; 
LL1_constant_expression(
# line 20 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&ifval, &is_unsigned);
}
static
#if LL_ANSI_C
void
#endif
LL2_primary(
#if LL_ANSI_C
# line 24 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 24 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns; 
#endif
{
switch(LLcsymb) {
default:
LL3_constant(
# line 26 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
break;
case /* '(' */ 28 : ;
LLtincr(29);
LL_SAFE('(');
LL4_expression(
# line 28 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
LLtdecr(29);
LL_SCANDONE(')');
break;
}
}
static
#if LL_ANSI_C
void
#endif
LL5_unary(
#if LL_ANSI_C
# line 31 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 31 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns; 
#endif
{
# line 32 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int oper;
L_2: ;
switch(LLcsymb) {
case /* '-' */ 33 : ;
case /* '!' */ 34 : ;
case /* '~' */ 35 : ;
LL6_unop(
# line 34 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&oper);
LLread();
LL5_unary(
# line 35 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
# line 36 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{ ch3mon(oper, pval, is_uns); }
break;
case /*  INTEGER  */ 4 : ;
case /* '(' */ 28 : ;
L_3: ;
LLsdecr(0);
LL2_primary(
# line 38 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
LLread();
break;
default: if (LLskip()) goto L_2;
goto L_3;
}
}
static
#if LL_ANSI_C
void
#endif
LL7_binary_expression(
#if LL_ANSI_C
# line 41 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int maxrank ,arith *pval ,int *is_uns)  
#else
# line 41 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 maxrank,pval,is_uns) int maxrank; arith *pval; int *is_uns; 
#endif
{
# line 42 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int oper; arith val1; int u;
LLsincr(0);
LLsincr(1);
LL5_unary(
# line 44 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
for (;;) {
L_1 : {switch(LLcsymb) {
case /*  NOTEQUAL  */ 15 : ;
case /*  AND  */ 16 : ;
case /*  LEFT  */ 20 : ;
case /*  LESSEQ  */ 21 : ;
case /*  EQUAL  */ 22 : ;
case /*  GREATEREQ  */ 23 : ;
case /*  RIGHT  */ 24 : ;
case /*  OR  */ 25 : ;
case /* '-' */ 33 : ;
case /* '*' */ 36 : ;
case /* '/' */ 37 : ;
case /* '%' */ 38 : ;
case /* '+' */ 39 : ;
case /* '<' */ 40 : ;
case /* '>' */ 41 : ;
case /* '&' */ 42 : ;
case /* '^' */ 43 : ;
case /* '|' */ 44 : ;
# line 45 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
if ((rank_of(DOT) <= maxrank)) goto L_2;
case /*  EOFILE  */ 0 : ;
case /* ')' */ 29 : ;
case /* '?' */ 30 : ;
case /* ':' */ 31 : ;
case /* ',' */ 32 : ;
break;
default:{int LL_1=LLnext(-6);
;if (!LL_1) {
break;
}
else if (LL_1 & 1) goto L_1;}
L_2 : ;
LLsincr(2);
LL8_binop(
# line 46 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&oper);
LLread();
LLsdecr(2);
LL7_binary_expression(
# line 47 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
rank_of(oper)-1, &val1, &u);
# line 48 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{
			ch3bin(pval, is_uns, oper, val1, u);
		}
continue;
}
}
LLsdecr(1);
break;
}
}
static
#if LL_ANSI_C
void
#endif
LL9_conditional_expression(
#if LL_ANSI_C
# line 55 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 55 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns;
#endif
{
# line 56 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith val1 = 0, val2 = 0; int u;
LLtincr(30);
LL7_binary_expression(
# line 59 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
rank_of('?') - 1, pval, is_uns);
L_1 : {switch(LLcsymb) {
case /*  EOFILE  */ 0 : ;
case /* ')' */ 29 : ;
case /* ':' */ 31 : ;
case /* ',' */ 32 : ;
LLtdecr(30);
break;
default:{int LL_2=LLnext(63);
;if (!LL_2) {
LLtdecr(30);
break;
}
else if (LL_2 & 1) goto L_1;}
case /* '?' */ 30 : ;
LLtdecr(30);
LLtincr(31);
LLsincr(3);
LL_SAFE('?');
LL4_expression(
# line 61 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&val1, is_uns);
LLtdecr(31);
LL_SCANDONE(':');
LLread();
LLsdecr(3);
LL10_assignment_expression(
# line 63 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&val2, &u);
# line 64 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{ if (*pval) *pval = val1;
		  else { *pval = val2; *is_uns = u; }
		}
}
}
}
static
#if LL_ANSI_C
void
#endif
LL10_assignment_expression(
#if LL_ANSI_C
# line 71 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 71 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns;
#endif
{
LL9_conditional_expression(
# line 73 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
}
static
#if LL_ANSI_C
void
#endif
LL4_expression(
#if LL_ANSI_C
# line 77 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 77 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns;
#endif
{
# line 78 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith val1;
	 int is_uns1;
	
LLtincr(32);
LLread();
LL10_assignment_expression(
# line 82 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval,is_uns);
for (;;) {
L_1 : {switch(LLcsymb) {
case /* ')' */ 29 : ;
case /* ':' */ 31 : ;
break;
default:{int LL_3=LLnext(44);
;if (!LL_3) {
break;
}
else if (LL_3 & 1) goto L_1;}
case /* ',' */ 32 : ;
LL_SAFE(',');
LLread();
LL10_assignment_expression(
# line 84 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
&val1, &is_uns1);
# line 85 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{
			ch3bin(pval, is_uns, ',', val1, is_uns1);
		}
continue;
}
}
LLtdecr(32);
break;
}
}
static
#if LL_ANSI_C
void
#endif
LL6_unop(
#if LL_ANSI_C
# line 91 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int *oper)  
#else
# line 91 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 oper) int *oper; 
#endif
{
# line 93 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{*oper = DOT;}
}
static
#if LL_ANSI_C
void
#endif
LL11_multop(
#if LL_ANSI_C
void
#endif
) {
}
static
#if LL_ANSI_C
void
#endif
LL12_addop(
#if LL_ANSI_C
void
#endif
) {
}
static
#if LL_ANSI_C
void
#endif
LL13_shiftop(
#if LL_ANSI_C
void
#endif
) {
}
static
#if LL_ANSI_C
void
#endif
LL14_relop(
#if LL_ANSI_C
void
#endif
) {
}
static
#if LL_ANSI_C
void
#endif
LL15_eqop(
#if LL_ANSI_C
void
#endif
) {
}
static
#if LL_ANSI_C
void
#endif
LL16_arithop(
#if LL_ANSI_C
void
#endif
) {
switch(LLcsymb) {
default:
LL11_multop();
break;
case /* '-' */ 33 : ;
case /* '+' */ 39 : ;
LL12_addop();
break;
case /*  LEFT  */ 20 : ;
case /*  RIGHT  */ 24 : ;
LL13_shiftop();
break;
case /* '&' */ 42 : ;
LL_SAFE('&');
break;
case /* '^' */ 43 : ;
LL_SAFE('^');
break;
case /* '|' */ 44 : ;
LL_SAFE('|');
break;
}
}
static
#if LL_ANSI_C
void
#endif
LL8_binop(
#if LL_ANSI_C
# line 122 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
int *oper)  
#else
# line 122 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 oper) int *oper; 
#endif
{
switch(LLcsymb) {
case /*  LEFT  */ 20 : ;
case /*  RIGHT  */ 24 : ;
case /* '-' */ 33 : ;
case /* '*' */ 36 : ;
case /* '/' */ 37 : ;
case /* '%' */ 38 : ;
case /* '+' */ 39 : ;
case /* '&' */ 42 : ;
case /* '^' */ 43 : ;
case /* '|' */ 44 : ;
LL16_arithop();
break;
default:
LL14_relop();
break;
case /*  NOTEQUAL  */ 15 : ;
case /*  EQUAL  */ 22 : ;
LL15_eqop();
break;
case /*  AND  */ 16 : ;
LL_SAFE(AND);
break;
case /*  OR  */ 25 : ;
LL_SAFE(OR);
break;
}
# line 124 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{*oper = DOT;}
}
static
#if LL_ANSI_C
void
#endif
LL3_constant(
#if LL_ANSI_C
# line 127 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 127 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns;
#endif
{
LL_SSCANDONE(INTEGER);
# line 129 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
{*pval = dot.tk_val;
	 *is_uns = dot.tk_unsigned;
	}
}
static
#if LL_ANSI_C
void
#endif
LL1_constant_expression(
#if LL_ANSI_C
# line 134 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
arith *pval ,int *is_uns)  
#else
# line 134 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
 pval,is_uns) arith *pval; int *is_uns;
#endif
{
LL10_assignment_expression(
# line 135 "/usr/proj/em/Src/lang/cem/cpp.ansi/expression.g"
pval, is_uns);
}


