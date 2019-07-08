/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef Math____SEEN
#define Math____SEEN

#include <math.h>
#include <interface.h>

#define f_Math__sin(a)		((t_longreal)sin(a))
#define f_Math__cos(a)		((t_longreal)cos(a))
#define f_Math__tan(a)		((t_longreal)tan(a))
#define f_Math__asin(a)		((t_longreal)asin(a))
#define f_Math__acos(a)		((t_longreal)acos(a))
#define f_Math__atan(a)		((t_longreal)atan(a))
#define f_Math__atan2(a,b)	((t_longreal)atan2(a,b))
#define f_Math__sinh(a)		((t_longreal)sinh(a))
#define f_Math__cosh(a)		((t_longreal)cosh(a))
#define f_Math__tanh(a)		((t_longreal)tanh(a))
#define f_Math__exp(a)		((t_longreal)exp(a))
#define f_Math__log(a)		((t_longreal)log(a))
#define f_Math__log10(a)	((t_longreal)log10(a))
#define f_Math__pow(a,b)	((t_longreal)pow(a,b))
#define f_Math__sqrt(a)		((t_longreal)sqrt(a))
#define f_Math__ceil(a)		((t_longreal)ceil(a))
#define f_Math__floor(a)	((t_longreal)floor(a))
#define f_Math__fabs(a)		((t_longreal)fabs(a))
#define f_Math__ldexp(a,b)	((t_longreal)ldexp(a,b))
#define f_Math__frexp(a,b)	((t_longreal)frexp(a,b))
#define f_Math__modf(a,b)	((t_longreal)modf(a,b))
#define f_Math__fmod(a,b)	((t_longreal)fmod(a,b))
#define f_Math__LeftShift(a,b)	((t_integer)(a << b))
#define f_Math__RightShift(a,b)	((t_integer)(a >> b))
#define f_Math__BitwiseAnd(a,b)	((t_integer)(a & b))
#define f_Math__BitwiseOr(a,b)	((t_integer)(a | b))
#define f_Math__BitwiseXor(a,b)	((t_integer)(a ^ b))
#define f_Math__IntToLongReal(n) \
				((t_longreal)(n))
#define f_Math__LongRealToInt(n) \
				((t_integer)(n))
#define f_Math__LongRealAbs(a)	((t_longreal)fabs(a))

#define ini_Math__Math()        /* nothing */

extern t_longreal f_Math__LongRealMin(t_longreal a,  t_longreal b);
extern t_longreal f_Math__LongRealMax(t_longreal a,  t_longreal b);

#endif
