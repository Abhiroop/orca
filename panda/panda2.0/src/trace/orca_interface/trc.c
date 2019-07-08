#include <interface.h>

#include "trace.h"
#include "trace/trace.h"

char *
trc__nmconv(t_string *name)
{
	/* Convert an Orca string (which is possibly not null-terminated)
	   to a C string.
	*/
	register char	*s = sys_malloc(name->a_sz + 1);

	(void) strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
	s[name->a_sz] = 0;
	return s;
}

void f_trace__new_thread(t_integer v_buf_size, t_string *v_name)
{
	char	*s = trc__nmconv(v_name);

	trc_new_thread(v_buf_size, s);
	free(s);
}

t_integer f_trace__set_level(t_integer v_level)
{
	return trc_set_level(v_level);
}
