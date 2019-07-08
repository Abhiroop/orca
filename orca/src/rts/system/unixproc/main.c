/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: main.c,v 1.11 1998/12/09 18:15:05 ceriel Exp $ */

#include <interface.h>
#include "unixproc.h"

extern void	ini_OrcaMain(void);
extern prc_dscr OrcaMain;
int		ncpus = 1;
struct proccnt	prccnt;
int		schedcount = SCHEDINIT;
static int	ac;
static char	**av;

int
main(int argc, char *argv[])
{
  ac = argc;
  av = argv;
  if (argc > 1) {
	int	c = argv[1][0];
	if (c >= '0' && c <= '9') {
  		ncpus = atoi(argv[1]);
		ac--;
		av++;
		av[0] = argv[0];
	}
	if (ac > 1 && ! strcmp(av[1], "-OC")) {
		ac--;
		av++;
		av[0] = argv[0];
	}
  }

  ini_OrcaMain();
#ifndef LINUX
  pthread_init();
#endif
  if (pthread_mutex_init(&malloc_lock, NULL) == -1) {
  	m_syserr("mutex initialization failed (main, malloc_lock)");
  }
  if (pthread_mutex_init(&prccnt.lock, NULL) == -1) {
  	m_syserr("mutex initialization failed (main, prccnt)");
  }
  if (pthread_cond_init(&prccnt.cond, NULL) == -1) {
  	m_syserr("cond initialization failed (main, prccnt)");
  }
  if (pthread_key_create(&key, NULL) == -1) {
  	m_syserr("key initialization failed (main)");
  }
  if (pthread_mutex_init(&procmutex, NULL) == -1) {
  	m_syserr("mutex initialization failed (main, procmutex)");
  }
  DoFork(0, &OrcaMain, (void **) 0);
  pthread_mutex_lock(&prccnt.lock);
  while (prccnt.count != 0) {
  	pthread_cond_wait(&prccnt.cond, &prccnt.lock);
  }
  pthread_mutex_unlock(&prccnt.lock);
  end_procs();
  exit(0);
  return 0;
}

t_integer
m_mycpu(void)
{
  void *dummy;

#if defined(BSDI) || defined(LINUX)
  dummy = pthread_getspecific(key);
#else
  pthread_getspecific(key, &dummy);
#endif
  return dummy != NULL ? ((struct proc *) dummy)->prc_cpu : 0;
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
