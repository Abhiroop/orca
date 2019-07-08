/* $Id: main.c,v 1.4 1996/07/04 08:53:50 ceriel Exp $ */

#include <interface.h>

extern void	ini_OrcaMain(void);
extern prc_dscr OrcaMain;
static int	ac;
static char	**av;

main(int argc, char *argv[])
{
  ac = argc;
  av = &argv[0];
  if (argc > 1) {
	if (! strcmp(argv[1], "-OC")) {
		ac = argc - 1;
		argv[1] = argv[0];
		av = &argv[1];
	}
  }
  ini_OrcaMain();
  (*(OrcaMain.prc_name))((void **) 0);
  exit(0);
}

t_integer f_args__Argc(void)
{
  return	ac;
}

void f_args__Argv(t_integer n, t_string *s)
{
  free_string(s);
  if (n >= 0 && n < ac) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(av[n]));
	strncpy(&((char *)(s->a_data))[s->a_offset], av[n], s->a_sz);
  }
}

void f_args__Getenv(t_string *e, t_string *s)
{
  char	buf[1024];
  char	*p = buf;
  char	*ev;

  if (e->a_sz >= 1024) {
	p = m_malloc(e->a_sz + 1);
  }
  strncpy(p, &((char *)(e->a_data))[e->a_offset], e->a_sz);
  p[e->a_sz] = 0;
  ev = getenv(p);
  if (e->a_sz >= 1024) m_free(p);
  free_string(s);
  if (ev) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(ev));
	strncpy(&((char *)(s->a_data))[s->a_offset], ev, s->a_sz);
  }
}
