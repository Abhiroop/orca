/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#include <interface.h>

#include "pan_sys.h"
#include "pan_trace.h"

#include "trace.h"

#ifdef NOT_NEEDED_

static char *
trc__nmconv(t_string *name)
{
	/* Convert an Orca string (which is possibly not null-terminated)
	   to a C string.
	*/
	register char	*s = m_malloc(name->a_sz + 1);

	(void) strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
	s[name->a_sz] = 0;
	return s;
}

void f_trace__new_thread(t_integer v_buf_size, t_string *v_name)
{
	char	*s = trc__nmconv(v_name);

	trc_new_thread(v_buf_size, s);
	m_free(s);
}

#endif /* NOT_NEEDED_ */

void f_trace__flush(void)
{
	trc_flush();
}

t_integer f_trace__set_level(t_integer v_level)
{
	return trc_set_level(v_level);
}
