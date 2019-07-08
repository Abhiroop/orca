/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_UTIL_STRDUP_H__
#define __PANDA_UTIL_STRDUP_H__

char *strdup(const char *s);

void pan_strdup_start(void);

void pan_strdup_end(void);

#endif
