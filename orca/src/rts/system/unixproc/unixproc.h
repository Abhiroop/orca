/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: unixproc.h,v 1.9 1997/04/03 14:11:59 ceriel Exp $ */

#ifdef SUNOS4
#include <setjmp.h>
#endif

/* Process structure, one for each Orca process. */
struct proc {
	prc_dscr	*prc_d;		/* reference to process descriptor */
	int		prc_inuse;	/* set when the next field is valid */
	pthread_t	prc_p;		/* thread identification */
	void		**prc_args;	/* process arguments */
	int		prc_cpu;	/* CPU of this process */
#ifdef SUNOS4
	jmp_buf		prc_env;
#endif
};

/* Keeping track of the current number of Orca processes. */
struct proccnt {
	pthread_mutex_t	lock;		/* the count mutex */
	pthread_cond_t	cond;		/* its condition variable */
	int		count;		/* the count itself */
};

extern struct proccnt	prccnt;
extern pthread_key_t	key;
extern pthread_mutex_t	procmutex;
extern pthread_mutex_t	malloc_lock;
extern int		ncpus;

void end_procs(void);
