/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: DoFork.c,v 1.22 1998/12/09 18:15:05 ceriel Exp $ */

#include <interface.h>
#include "unixproc.h"
#ifdef SUNOS4
#include <signal.h>
#endif

pthread_key_t key;
pthread_mutex_t procmutex;

static struct proc *prcfree;

struct proc *
new_proc(void)
{
  register struct proc *prc;

  pthread_mutex_lock(&procmutex);
  if (prcfree) {
  	prc = prcfree;
  	prcfree = (struct proc *) (void *) (prc->prc_d);
  }
  else {
  	prc = (struct proc *) m_malloc(sizeof(struct proc));
  	prc->prc_inuse = 0;
  }
  pthread_mutex_unlock(&procmutex);
  return prc;
}

void
end_procs(void)
{
  register struct proc *prc = prcfree;

  while (prc) {
  	if (prc->prc_inuse) {
  		pthread_join(prc->prc_p, NULL);
  	}
  	prc = (struct proc *) (void *) (prc->prc_d);
  }
}

void
free_proc(struct proc *p)
{
  pthread_mutex_lock(&procmutex);
  p->prc_d = (prc_dscr *) (void *) prcfree;
  prcfree = p;
  pthread_mutex_unlock(&procmutex);
}

#ifdef SUNOS4
static void
catch_sig(sig, info)
  int sig;
  struct siginfo *info;
{
  void *dummy;
  struct proc *prc;
  struct sigaction a;

  a.sa_handler = SIG_DFL;
  a.sa_mask = 0;
  a.sa_flags = 0;

  pthread_getspecific(key, &dummy);
  prc = dummy;
  if ((sig == SIGILL && info->si_code == ILL_STACK) ||
      (sig == SIGSEGV && FC_CODE(info->si_code) == FC_PROT) ||
      (sig == SIGBUS &&  FC_CODE(info->si_code) == FC_OBJERR)) {
  	longjmp(prc->prc_env, 1);
  }
  sigaction(sig, &a, (struct sigaction *) 0);
  pthread_kill(prc->prc_p, sig);
}
#endif

static void *
process(void *arg)
{
  register struct proc *prc = arg;
  int dosig = 0;
  int npars;

  pthread_setspecific(key, prc);

#ifdef SUNOS4
  if (setjmp(prc->prc_env) != 0) {
	m_liberr("Limit exceeded", "stack overflow");
	return 0;
  }
  else {
	struct sigaction a;

	a.sa_handler = catch_sig;
	a.sa_mask = 0;
	a.sa_flags = SA_ONSTACK;

	sigaction(SIGBUS, &a, (struct sigaction *) 0);
	sigaction(SIGILL, &a, (struct sigaction *) 0);
	sigaction(SIGSEGV, &a, (struct sigaction *) 0);
  }
#endif
  (*(prc->prc_d->prc_name))(prc->prc_args);

  npars = td_nparams(prc->prc_d->prc_func);
  if (npars) {
  	register par_dscr *d = td_params(prc->prc_d->prc_func);
  	register void **p = prc->prc_args;

  	while (npars--) {
  		if (d->par_mode == SHARED) {
			if (d->par_descr->td_type == ARRAY) {
				tp_dscr *ed = td_elemdscr(d->par_descr);
				t_array *a;
				register char *ap;
				register int i;

				assert(ed->td_type == OBJECT);
				a = (t_array *) (*p);
				ap = (char *) a->a_data +
					a->a_offset * ed->td_size;
				for (i = 0; i < a->a_sz; i++) {
					pthread_mutex_lock(&((t_object *) ap)->o_mutex);
					((t_object *) ap)->o_shared--;
					pthread_mutex_unlock(&((t_object *) ap)->o_mutex);
					ap += ed->td_size;
				}
			}
			else {
				pthread_mutex_lock(&((t_object *) *p)->o_mutex);
				((t_object *) *p)->o_shared--;
				pthread_mutex_unlock(&((t_object *) *p)->o_mutex);
			}
			if (d->par_free) {
				(*(d->par_free))(*p);
			}
  		}
  		m_free(*p);
  		d++;
  		p++;
  	}
  	m_free((void *)(prc->prc_args));
  }

  pthread_mutex_lock(&prccnt.lock);
  prccnt.count--;
  if (prccnt.count == 0) dosig = 1;
  pthread_mutex_unlock(&prccnt.lock);
  if (dosig) pthread_cond_signal(&prccnt.cond);
  free_proc(prc);
  return 0;
}

void DoFork(int cpu, prc_dscr *procdscr, void **argtab)
{
  register struct proc *prc = new_proc();
  register unsigned int npars;
#if defined(BSDI) || defined(LINUX)
  static pthread_attr_t attr;
  static int initialized;

  if (! initialized) {
	initialized = 1;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  }
#endif

  if (prc->prc_inuse) {
  	pthread_join(prc->prc_p, NULL);
  }
  prc->prc_inuse = 1;
  prc->prc_cpu = cpu;
  prc->prc_d = procdscr;
  npars = td_nparams(prc->prc_d->prc_func);
#ifdef KOEN_STAT
  fprintf(stderr, "FORK 0x%08lx ON %d ", (long) procdscr, cpu);
#endif
  if (npars) {
  	register par_dscr *d = td_params(prc->prc_d->prc_func);
  	register void **p;

  	prc->prc_args = p = (void **) m_malloc(sizeof(void *) * npars);
  	while (npars--) {
  		*p = m_malloc(d->par_descr->td_size);
  		if (d->par_mode == SHARED) {
  			memcpy(*p, *argtab, d->par_descr->td_size);
			if (d->par_descr->td_type == ARRAY) {
				t_array *a = *p;
				tp_dscr *ed = td_elemdscr(d->par_descr);
				size_t off = ed->td_size * a->a_offset;
				register int i;
				char *ap;

				a->a_data = (char *) m_malloc(
					a->a_sz * ed->td_size) - off;
				ap = (char *) (a->a_data) + off;
				(void) memcpy((char *) ap,
					      (char *) (((t_array *) (*argtab))->a_data) + off,
					      a->a_sz * ed->td_size);
				for (i = 0; i < a->a_sz; i++) {
					pthread_mutex_lock(&((t_object *) ap)->o_mutex);
					((t_object *) ap)->o_shared++;
					((t_object *) ap)->o_refcount++;
					pthread_mutex_unlock(&((t_object *) ap)->o_mutex);
					ap += ed->td_size;
				}
			}
			else {
#ifdef KOEN_STAT
				fprintf(stderr, "0x%lx 0x%08lx  ",
					((t_object *) (*p))->o_fields,
					d->par_descr);
#endif
				pthread_mutex_lock(&((t_object *) *p)->o_mutex);
  				((t_object *) (*p))->o_shared++;
  				((t_object *) (*p))->o_refcount++;
				pthread_mutex_unlock(&((t_object *) *p)->o_mutex);
			}
  		}
  		else {
			memset(*p, '\0', d->par_descr->td_size);
  			(*(d->par_copy))(*p, *argtab);
  		}
		argtab++;
  		d++;
  		p++;
  	}
  }
#ifdef KOEN_STAT
  fprintf(stderr, "\n", cpu);
#endif
  pthread_mutex_lock(&prccnt.lock);
  prccnt.count++;
  pthread_mutex_unlock(&prccnt.lock);
#if defined(BSDI) || defined(LINUX)
  if (pthread_create(&prc->prc_p, &attr, process, prc) == -1) {
  	m_liberr("Limit exceeded", "FORK failed");
  }
#else
  if (pthread_create(&prc->prc_p, NULL, process, prc) == -1) {
  	m_liberr("Limit exceeded", "FORK failed");
  }
#endif
#if defined(BSDI) || defined(LINUX)
  sched_yield();
#else
  pthread_yield(NULL);
#endif
}

