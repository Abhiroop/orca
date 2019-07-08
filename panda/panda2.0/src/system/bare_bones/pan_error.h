/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _PAN_GENERIC_ERROR_
#define _PAN_GENERIC_ERROR_

#include <assert.h>
#include <stdio.h>
#include <errno.h>

#ifdef DEBUG
#define Debug(x) x
#else
#define Debug(x)
#endif

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif

#define Debugn(n, x) if (n <= DEBUG_LEVEL) { x; }

#endif

