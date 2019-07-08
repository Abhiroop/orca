/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include <math.h>
#include <interface.h>
#include "Math.h"

t_longreal (f_Math__sin)(t_longreal a)
{
    return (t_longreal)sin(a);
}

t_longreal (f_Math__cos)(t_longreal a)
{
    return (t_longreal)cos(a);
}

t_longreal (f_Math__tan)(t_longreal a)
{
    return (t_longreal)tan(a);
}

t_longreal (f_Math__asin)(t_longreal a)
{
    return (t_longreal)asin(a);
}

t_longreal (f_Math__acos)(t_longreal a)
{
    return (t_longreal)acos(a);
}

t_longreal (f_Math__atan)(t_longreal a)
{
    return (t_longreal)atan(a);
}

t_longreal (f_Math__atan2)(t_longreal a, t_longreal b)
{
    return (t_longreal)atan2(a,b);
}

t_longreal (f_Math__sinh)(t_longreal a)
{
    return (t_longreal)sinh(a);
}

t_longreal (f_Math__cosh)(t_longreal a)
{
    return (t_longreal)cosh(a);
}

t_longreal (f_Math__tanh)(t_longreal a)
{
    return (t_longreal)tanh(a);
}

t_longreal (f_Math__exp)(t_longreal a)
{
    return (t_longreal)exp(a);
}

t_longreal (f_Math__log)(t_longreal a)
{
    return (t_longreal)log(a);
}

t_longreal (f_Math__log10)(t_longreal a)
{
    return (t_longreal)log10(a);
}

t_longreal (f_Math__pow)(t_longreal a,  t_longreal b)
{
    return (t_longreal)pow(a, b);
}

t_longreal (f_Math__sqrt)(t_longreal a)
{
    return (t_longreal)sqrt(a);
}

t_longreal (f_Math__ceil)(t_longreal x)
{
    return (t_longreal)ceil(x);
}

t_longreal (f_Math__floor)(t_longreal x)
{
    return (t_longreal)floor(x);
}

t_longreal (f_Math__fabs)(t_longreal a)
{
    return (t_longreal)fabs(a);
}

t_longreal (f_Math__ldexp)(t_longreal a, t_integer n)
{
    return (t_longreal)ldexp(a, n);
}

t_longreal (f_Math__frexp)(t_longreal a, t_integer *n)
{
    return (t_longreal)frexp(a, n);
}

t_longreal (f_Math__modf)(t_longreal a, t_longreal *n)
{
    return (t_longreal)modf(a, n);
}

t_longreal (f_Math__fmod)(t_longreal a,t_longreal b)
{
    return (t_longreal)fmod(a, b);
}

t_integer (f_Math__LeftShift)(t_integer a, t_integer b)
{
    return (t_integer)(a << b);
}

t_integer (f_Math__RightShift)(t_integer a, t_integer b)
{
    return (t_integer)(a >> b);
}

t_integer (f_Math__BitwiseAnd)(t_integer a, t_integer b)
{
    return (t_integer)(a & b);
}

t_integer (f_Math__BitwiseOr)(t_integer a, t_integer b)
{
    return (t_integer)(a | b);
}

t_integer (f_Math__BitwiseXor)(t_integer a, t_integer b)
{
    return (t_integer)(a ^ b);
}

t_longreal (f_Math__IntToLongReal)(int n)
{
    return (t_longreal) n;
}

int (f_Math__LongRealToInt)(t_longreal r)
{
    return (int) r;
}

t_longreal (f_Math__LongRealMin)(t_longreal a,  t_longreal b)
{
    return a > b ? b : a;
}

t_longreal (f_Math__LongRealMax)(t_longreal a,  t_longreal b)
{
    return a < b ? b : a;
}

t_longreal (f_Math__LongRealAbs)(t_longreal a)
{
    return (t_longreal)fabs(a);
}
