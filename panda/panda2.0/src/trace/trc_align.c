/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Supplies alignment info for non-structured types
 */

#include <stddef.h>



/* Run-time measurement of alignments that should always work */

size_t
f_align(void)
/* double */
{
    struct ALIGN_T {
	double      v1;
	char        a1;
	double      v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}

size_t
d_align(void)
/* int */
{
    struct ALIGN_T {
	int         v1;
	char        a1;
	int         v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}

size_t
p_align(void)
/* void * */
{
    struct ALIGN_T {
	void       *v1;
	char        a1;
	void       *v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}

size_t
h_align(void)
/* short int */
{
    struct ALIGN_T {
	short int   v1;
	char        a1;
	short int   v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}

size_t
l_align(void)
/* long int */
{
    struct ALIGN_T {
	long int    v1;
	char        a1;
	long int    v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}

size_t
c_align(void)
/* char */
{
    struct ALIGN_T {
	char        v1;
	char        a1;
	char        v2;
	char        a2;
    }           v;

    return (char *)&v.v2 - &v.a1;
}
