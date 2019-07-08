/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>



int
main(int argc, char *argv[])
{
    struct rlimit   fileres;

#ifdef AMOEBA
    printf("%d\n", FOPEN_MAX);
#else
    getrlimit(RLIMIT_NOFILE, &fileres);
    printf("%ld\n", fileres.rlim_max);
#endif

    return 0;
}
