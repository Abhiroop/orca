/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: defaults.c,v 1.7 1995/07/31 08:56:49 ceriel Exp $ */

/*   D E F A U L T   V A L U E S   F O R   C O M P I L A T I O N   */

/* A table containing names of compilers, compiler flags, etc.,
   and routines to access the table.
*/

#include <stdio.h>

#include "ansi.h"
#include "defaults.h"

typedef struct {
	char	*co_name;
	char	*co_value;
} comp_info;

/* Make OC_HOME patchable. */
static char oc_home[1024] = OC_HOME;

static comp_info comp_values[] = {
	{ "OC_COMP",		OC_COMP },
	{ "OC_FLAGS",		OC_FLAGS },
	{ "OC_INCLUDES",	OC_INCLUDES },
	{ "OC_RTSINCLUDES",	OC_RTSINCLUDES },
	{ "OC_CCOMP",		OC_CCOMP },
	{ "OC_CFLAGS",		OC_CFLAGS },
	{ "OC_LD",		OC_LD },
	{ "OC_LDFLAGS",		OC_LDFLAGS },
	{ "OC_STARTOFF",	OC_STARTOFF },
	{ "OC_LIBS",		OC_LIBS },
	{ "OC_HOME",		oc_home },
	{ "OC_MACH",		OC_MACH },
	{ "OC_RTSNAM",		OC_RTSNAM },
	{ "OC_LIBNAM",		OC_LIBNAM },
	{ "OC_LIBNAMOLD",	OC_LIBNAMOLD },
	{ "OC_SPECIAL",		OC_SPECIAL },
	{ 0,			0 }
};

char *
get_value(name)
	char	*name;
{
	register comp_info *p = &comp_values[0];

	while (p->co_name &&
	       (p->co_name[3] != name[3] || strcmp(p->co_name, name))) p++;
	return p->co_name ? p->co_value : (char *)0;
}

int
set_value(name, val)
	char	*name, *val;
{
	register comp_info *p = &comp_values[0];

	while (p->co_name &&
	       (p->co_name[3] != name[3] || strcmp(p->co_name, name))) p++;
	if (! p->co_name) return 0;
	p->co_value = val;
	return 1;
}

#ifndef __STDC__
extern char *getenv();
#endif

void
init_defaults()
{
	register comp_info *p = &comp_values[0];

	while (p->co_name) {
		char *e = getenv(p->co_name);
		if (e) {
			p->co_value = e;
		}
		p++;
	}
}

void
print_values()
{
	register comp_info *p = &comp_values[0];

	while (p->co_name) {
		printf("%s=%s\n", p->co_name, p->co_value);
		p++;
	}
}
