/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stdio.h>
#include <math.h>

#include "pan_stddev.h"

#undef DEBUG

double
mn_stdev(double sumsqr, double sum, int n)
{
#ifdef DEBUG
    fprintf(stderr, "mn_stdev :: sumsqr = %f; sum = %f; n = %d\n",
	    sumsqr, sum, n);
#endif
    if (n < 2)
	return 0.0;
    return sqrt((sumsqr - sum * sum / n) / (n * (n - 1)));
}

double
stdev(double sumsqr, double sum, int n)
{
#ifdef DEBUG
    fprintf(stderr, "stdev :: sumsqr = %f; sum = %f; n = %d\n", sumsqr, sum, n);
#endif
    if (n < 2)
	return 0.0;
    return sqrt((sumsqr - sum * sum / n) / (n - 1));
}
