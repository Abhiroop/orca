/*
 * We want to know what sequence of #molecules is needed to get a sequence
 * of Water runs that perform a linaerly increasing amount of work. Since
 * the relation between the number of molecules M and the amount of work W is
 *
 *     W = M**3,
 *
 * we can easily compute this information.
 *
 * Input: 
 *        1) minimum value of M
 *        2) maximum value of M
 *        3) desired number of M values between the min. and max. values.
 *
 * Output:
 * 	Row sizes (as many as desired), such that the amount of work
 * 	increases linearly as the row size increases.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define rint(d)  floor((d) + 0.5)

static char *progname;

void
main(int argc, char **argv)
{
    double sz, lb, ub, start, end, step, root2;
    int npoints;

    progname = argv[0];

    if (argc != 1 && argc != 4) {
	fprintf(stderr, "Usage: %s lower upper npoints\n", progname);
	exit(1);
    }

    if (argc == 4) {
	lb = atof(argv[1]);
	ub = atof(argv[2]);
	npoints = atoi(argv[3]);
    } else {
	lb = 256.0;
	ub = 2048.0;
	npoints = 8;
    }

    start = lb * lb;
    end = ub * ub;
    step = (end - start) / (npoints - 1);

    for (sz = start; sz <= end; sz += step) {
	root2 = pow((double)sz, 0.5);
	printf("%10.0f %10.0f\n", rint(sz), rint(root2));
    }

    exit(0);
}




